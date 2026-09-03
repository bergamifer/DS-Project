/* ESP32-C3-MINI-1 — DS11 Beamforming Bridge
 *
 * Canal de monitoreo unificado via USB Serial/JTAG:
 *   [ESP32] mensajes propios del ESP32
 *   [STM32] debug output del STM32H743 (forwardeado desde UART1 RX)
 *
 * Hardware DS11:
 *   UART1 TX → GPIO 0 → PA1 → STM32 UART4 RX   tramas binarias
 *   UART1 RX ← GPIO 1 ← PA0 ← STM32 UART4 TX   texto debug ASCII
 *
 *   NOTA PCB (DS9/DS10/DS11, sin cambios): las trazas GPIO0/GPIO1 van a PA1/PA0 respectivamente
 *   (cruzadas respecto al numero de pin). GPIO0→PA1 y GPIO1→PA0.
 *
 * USB Serial/JTAG (GPIO 18/19) → PC: terminal de monitoreo
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_partition.h"
#include "mdns.h"
#include "cJSON.h"

// ============== Configuracion hardware ==============

#define BLINK_GPIO       GPIO_NUM_10   // LED en GPIO10
#define BLINK_PERIOD_MS  500           // 500ms ON / 500ms OFF = 1Hz

// WiFi credentials live in secrets.h (gitignored). Copy secrets.h.example to
// secrets.h and fill in your network — keeps passwords out of the public repo.
#include "secrets.h"

/* Identidad ÚNICA por placa, derivada del MAC del ESP32.
 *
 * Antes esto era `#define DEVICE_ID "DS_01"` y el hostname mDNS era
 * "domosonica", los dos constantes de compilación. Con una placa no molestaba;
 * con dos encendidas a la vez **las dos se llamaban igual y las dos se
 * anunciaban igual**: `domosonica.local` resolvía a cualquiera de las dos y la
 * app no podía distinguirlas. Para una instalación de 10-16 nodos es inviable.
 *
 * El MAC ya es único y no hace falta administrarlo ni grabarlo en ningún lado.
 * Se usan los tres bytes bajos: MAC 08:92:72:1e:1f:30 -> DS_1E1F30 /
 * ds-1e1f30.local.
 *
 * ⚠️ Consecuencia deliberada: **`domosonica.local` ya no resuelve.** Se prefirió
 * romperlo ahora, con dos placas y las herramientas a mano, antes que con
 * dieciséis montadas en una sala. La IP sigue funcionando siempre. */
static char device_id[16]  = "DS_000000";
static char mdns_host[20]  = "ds-000000";

static void device_identity_init(void)
{
    uint8_t mac[6] = {0};
    /* El MAC de fábrica del WiFi: disponible antes de levantar la radio. */
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) return;
    snprintf(device_id, sizeof(device_id), "DS_%02X%02X%02X",
             mac[3], mac[4], mac[5]);
    snprintf(mdns_host, sizeof(mdns_host), "ds-%02x%02x%02x",
             mac[3], mac[4], mac[5]);
}

// UART0: link con STM32. PCB: GPIO0→PA1(STM32 RX), GPIO1→PA0(STM32 TX)
#define STM32_UART_NUM   UART_NUM_0
#define STM32_UART_TX    GPIO_NUM_0   // ESP32 TX → GPIO0 → PA1 → STM32 UART4 RX
#define STM32_UART_RX    GPIO_NUM_1   // ESP32 RX ← GPIO1 ← PA0 ← STM32 UART4 TX
#define STM32_UART_BAUD  115200
#define UART_BUF_SIZE    512

// Buffer de linea para el monitor del STM32
#define STM32_LINE_BUF   256

// ============== Protocolo binario ==============

#define PROTO_HEADER_1   0xAA
#define PROTO_HEADER_2   0x55
#define PROTO_FOOTER_1   0x0D
#define PROTO_FOOTER_2   0x0A
#define CMD_UPDATE_CFG   0x01
#define CMD_SET_MODE     0x02   // payload: uint8 indice de modo
#define CMD_GET_STATUS   0x03   // payload: vacio (len=0)
#define CMD_STATUS       0x04   // STM32→ESP32: ds_status_t
#define CMD_SET_BEAM     0x07   // payload: 9 bytes (type, az, fx, fy, fz)
#define CMD_ESP_INFO     0x08   // ESP32→STM32: IP en ASCII. El STM32 NO lo ACKea
                                // a proposito: un ACK espurio le robaria el ack_sem
                                // a un comando HTTP concurrente. Ver uart_protocol.h
#define CMD_SET_VOLUME   0x09   // payload: int8 dB (0..-63, <=-64 = mute). Volumen
                                // maestro via atenuacion del PCM1690 (todos los modos).
#define CMD_SET_AMPS     0x0A   // payload: uint8 (1=on, 0=off) — enable de los 32 PAM8302A
#define CMD_FACTORY_RESET 0x0C  // sin payload — todo a valores de fabrica, y persiste
#define CMD_SET_INGAIN   0x0B   // payload: uint8 dB (0..36) — ganancia de ENTRADA
// Autodiagnostico: el STM32 vuelve a leer por I2C lo que los chips REALMENTE
// tienen adentro. Es una prueba ACTIVA, no telemetria: un DAC que perdio su
// configuracion sigue reportando underruns=0 y CPU normal.
#define CMD_SELFTEST     0x0D   // sin payload
#define CMD_HEALTH       0x0E   // STM32 -> ESP32, ds_health_t
// Captura remota del ADC: bajar las muestras crudas para analizarlas afuera.
#define CMD_CAP_START    0x0F   // payload: uint16 n_samples
#define CMD_CAP_INFO     0x10   // STM32 -> ESP32, ds_cap_info_t
#define CMD_CAP_READ     0x11   // payload: uint32 offset en bytes
#define CMD_CAP_DATA     0x12   // STM32 -> ESP32: [offset:4][datos]
#define CAP_CHUNK_BYTES  (PROTO_MAX_PAYLOAD - 4)
// Resultado del AUTOFOCUS. NO dispara un scan: devuelve lo que midio el ultimo.
// Los 32 canales no entran en una trama (32x7 = 224 contra 128 de payload), asi
// que se lee por tramos como la captura.
#define CMD_SET_AUTOSTART 0x18  // payload: uint8 (1=on, 0=off), persistido
#define CMD_SET_TONE      0x19  // payload: [hz:2 LE][dither:1] — tono de medicion
// Barrido de fase por parlante (modo 22). 32 x 14 x 8 medidas complejas: se
// bajan por tramos con el mismo patron que la captura del ADC.
#define CMD_SET_MICS       0x1E // payload: [ch1:1][ch2:1] — que entradas son los mics
#define CMD_GET_PHASE_INFO 0x1A // payload: none
#define CMD_PHASE_INFO     0x1B // STM32 -> ESP32: ds_phase_info_t
#define CMD_PHASE_READ     0x1C // payload: uint32 offset en bytes
#define CMD_PHASE_DATA     0x1D // STM32 -> ESP32: [offset:4][datos]
#define PHASE_MAX_PTS      16
#define CMD_GET_MODES    0x16   // payload: uint8 indice del primer modo
#define CMD_MODES_DATA   0x17   // STM32 -> ESP32: ds_modes_hdr_t + nombres NUL
#define CMD_GET_CAL      0x13   // payload: uint8 indice del primer canal
#define CMD_CAL_DATA     0x14   // STM32 -> ESP32: ds_cal_hdr_t + N x ds_cal_ch_t
#define CAL_CHUNK_CH     16
// Actualizacion de firmware del STM32 por WiFi. Los tres primeros NO se
// contestan desde el parser del STM32: el ACK llega recien cuando termino de
// escribir la flash, y ESE es el control de flujo — mientras no llega, no se
// manda el tramo siguiente. Por eso cada uno tiene su propio tiempo de espera.
#define CMD_FW_BEGIN     0x20   // payload: fw_begin_t (12 B)
#define CMD_FW_DATA      0x21   // payload: [seq:2 LE][datos:1..FW_CHUNK_MAX]
#define CMD_FW_END       0x22   // payload: uint32 LE = CRC32 de la imagen
#define CMD_FW_ABORT     0x23   // sin payload
#define CMD_ACK          0x06
#define CMD_NACK         0x15

#define NUM_CHANNELS     32     // los 32 de DS11 (era 16 = media matriz)
// Techo del payload. DEBE coincidir con PROTO_MAX_PAYLOAD del STM32: si el
// STM32 manda mas de lo que aca se acepta, la trama se descarta por larga y el
// endpoint queda mudo SIN error visible.
#define PROTO_MAX_PAYLOAD 128
#define ESP_IP_PUSH_MS   10000  // reenvio periodico: si el STM32 resetea, recupera la IP

// Version del protocolo con la que se compilo ESTE firmware. ESPEJO de
// PROTO_VERSION en firmware/Core/Inc/uart_protocol.h — los dos chips tienen que
// coincidir. Se publica en /status como "proto_esp" junto al "proto_ver" que
// reporta el STM32: si difieren, la pagina lo dice y avisa CUAL hay que
// actualizar, en vez de mostrar un timeout indistinguible de una placa muerta.
#define PROTO_VERSION    17

// Datos por tramo de firmware: 126, no 128, porque los dos primeros bytes del
// payload llevan el numero de tramo.
#define FW_CHUNK_MAX     (PROTO_MAX_PAYLOAD - 2)

// Espejo de ds_phase_info_t del STM32 (uart_protocol.h).
typedef struct __attribute__((packed)) {
    uint8_t  n_spk;
    uint8_t  n_pts;
    uint8_t  n_ch;
    uint8_t  state;        // 0 idle, 1 corriendo, 2 listo
    uint8_t  progress;     // parlantes TERMINADOS
    uint8_t  reserved;
    uint32_t total_bytes;
    uint16_t freq_hz[PHASE_MAX_PTS];
} ds_phase_info_t;

// Espejo de ds_cap_info_t del STM32 (uart_protocol.h).
typedef struct __attribute__((packed)) {
    uint16_t n_samples;
    uint8_t  n_channels;
    uint8_t  shift;          // muestra_enviada = crudo_24bit >> shift
    uint32_t rate_hz;
    int32_t  peak;           // pico absoluto en escala de 24 bits
    uint32_t total_bytes;
} ds_cap_info_t;
_Static_assert(sizeof(ds_cap_info_t) == 16, "ds_cap_info_t debe ser 16 bytes");

// Espejo de ds_health_t del STM32 (uart_protocol.h).
typedef struct __attribute__((packed)) {
    uint8_t  dac_present;    // bit N = el DAC N contesta por I2C
    uint8_t  dac_fmt[4];     // registro 65 (FORMAT) — se espera 0x06
    uint8_t  dac_att[4];     // registro 72 (ATT ch1) — dB = -(255-att)/2
    uint8_t  ak_present;
    uint8_t  ak_regs[4];     // R01..R04 — se espera R02=0x31 R03=0x40 R04=0x02
    uint8_t  eeprom_present;
    uint8_t  eeprom_ver;
    int16_t  sync_us;        // desfase entre los dos bancos de SAI
    uint8_t  rsv[2];
} ds_health_t;
_Static_assert(sizeof(ds_health_t) == 20, "ds_health_t debe ser 20 bytes");

// Espejo de ds_cal_hdr_t / ds_cal_ch_t del STM32 (uart_protocol.h).
typedef struct __attribute__((packed)) {
    uint8_t  first;
    uint8_t  count;
    uint8_t  n_channels;
    uint8_t  state;
    uint8_t  progress;
    uint8_t  ok_count;
    uint8_t  mic1;        // entrada del ADC del microfono 1
    uint8_t  mic2;        // idem del 2
} ds_cal_hdr_t;
_Static_assert(sizeof(ds_cal_hdr_t) == 8, "ds_cal_hdr_t debe ser 8 bytes");

typedef struct __attribute__((packed)) {
    int32_t  tof_q8;      // pico de correlacion, muestras Q8 (1/256)
    int8_t   sign;        // polaridad medida
    uint16_t quality;     // pico/promedio x10
} ds_cal_ch_t;
_Static_assert(sizeof(ds_cal_ch_t) == 7, "ds_cal_ch_t debe ser 7 bytes");

typedef struct __attribute__((packed)) {
    uint32_t len;
    uint32_t fw_id;
    uint32_t build;
} fw_begin_t;
_Static_assert(sizeof(fw_begin_t) == 12, "fw_begin_t debe ser 12 bytes");

// Espejo de ds_config_t del STM32 — 102 bytes packed LE. packed explicito: sin
// el, el uint32 de mutes se alinearia distinto y el STM32 leeria basura.
typedef struct __attribute__((packed)) {
    uint8_t  master_gain;
    uint8_t  mute_global;
    uint16_t delays[NUM_CHANNELS];
    uint8_t  gains[NUM_CHANNELS];
    uint32_t mutes;         // 32 bits: con 16 no se podia silenciar mas alla del ch15
} ds_config_t;
_Static_assert(sizeof(ds_config_t) == 102, "ds_config_t debe ser 102 bytes");

// Espejo de ds_status_t del STM32 (uart_protocol.h) — 64 bytes packed LE (v4).
// Extender SOLO appendeando campos (proto_ver versiona el layout).
typedef struct __attribute__((packed)) {
    uint8_t  proto_ver;
    uint8_t  mode_idx;
    uint8_t  num_modes;
    int16_t  rx_az_deg;
    int16_t  fx_mm, fy_mm, fz_mm;
    uint32_t underruns;
    uint32_t sai_errors;
    uint8_t  ovf;
    uint8_t  cal_state;
    int32_t  adc_pk[8];
    int8_t   master_vol_db;   // v2: volumen maestro actual (0..-63, <=-64 mute)
    uint8_t  in_gain_db;      // v3: ganancia de ENTRADA actual (0..36)
    uint32_t adc_ovf_count;   // v3: MONOTONICO — satura el ADC (bajar la fuente)
    uint32_t out_clip_count;  // v3: MONOTONICO — satura la salida (bajar ingain)
    uint8_t  state_flags;     // v4: bit0 amps encendidos, bit1 beam custom activo
    uint32_t uptime_ms;       // v5: uptime DEL STM32 (el otro uptime es de este chip)
    uint16_t cpu_fills_x10;   // v6: % de CPU en los 4 fills, x10 (263 = 26,3%)
    uint8_t  mic_live_mask;   // v7: bit N = entrada N con senal (no solo ruido)
    uint8_t  cal_progress;    // v7: parlantes ya medidos por el AUTOFOCUS
    uint8_t  cal_ok_count;    // v7: de esos, cuantos con calidad suficiente
    uint32_t fw_id;           // v8: hash git corto del firmware STM32 que corre
    uint32_t fw_build;        // v8: epoch unix de esa compilacion
    uint8_t  ota_state;       // v8: 0 idle, 1 recibiendo, 2 lista, 3 error
    uint8_t  ota_pct;         // v8: avance de la carga, 0..100
    int16_t  dma_phase;
    uint32_t reset_rsr;   // v14: por que arranco el STM32       // v9: fase RX-TX de los DMA en muestras (multinode nivel 2)
} ds_status_t;
_Static_assert(sizeof(ds_status_t) == 89, "ds_status_t debe ser 89 bytes");
#define STATE_F_AMPS    0x01
#define STATE_F_CUSTOM  0x02
#define STATE_F_CALIB   0x04   // v7: hay calibracion de AUTOFOCUS en EEPROM
#define STATE_F_AUTOSTART 0x08 // v13: el nodo enciende los amps solo, sin red
#define STATE_F_AUTO_HELD 0x10 // v13: ...pero esta suspendido tras N intentos

// Espejo de ui_states[] del STM32 — actualizar junto con el firmware STM32.
// El indice es lo que viaja en CMD_SET_MODE; el nombre es solo para la UI.
/* ===== La lista de modos ya NO se copia: se le pregunta al STM32 =====
 *
 * Aca vivia MODE_NAMES[], un espejo a mano de ui_states[] del STM32, y la pagina
 * web tenia un tercer espejo propio. No era cosmetico: POST /mode validaba el
 * indice contra ESTA copia, asi que un modo agregado en el STM32 y olvidado aca
 * quedaba inalcanzable por HTTP — y el sintoma era mudo, porque /status informa
 * el num_modes del STM32 y la API decia tener mas modos de los que aceptaba.
 * Paso dos veces (issue #20): la ultima al agregar el TONO FIJO, con el STM32
 * informando 22 y el ESP32 rechazando el 21 sin decir por que.
 *
 * Ahora hay una sola fuente. La cache se llena la primera vez que alguien
 * pregunta y sobrevive hasta el reinicio; si el STM32 no contesta se queda
 * vacia y se vuelve a intentar en el pedido siguiente, que es lo que hay que
 * hacer cuando el otro chip todavia esta arrancando. */
#define MAX_MODES        40
#define MODE_NAME_MAX    64

typedef struct __attribute__((packed)) {
    uint8_t first, count, total, bytes;
} ds_modes_hdr_t;

static char        modes_store[MAX_MODES * 32];       // nombres, uno tras otro
static const char *modes_name[MAX_MODES];
static uint8_t     modes_count = 0;                   // 0 = cache vacia
static ds_modes_hdr_t    last_modes_hdr;
static uint8_t           last_modes_buf[PROTO_MAX_PAYLOAD];
static uint16_t          last_modes_len = 0;
static SemaphoreHandle_t modes_sem;

/** Llena la cache si hace falta. Definida mas abajo, donde estan los pedidos. */
static bool modes_ensure(void);

/** Nombre de un modo, o "?" si la cache no lo tiene. Nunca devuelve NULL: se usa
 *  dentro de un %s de JSON y un NULL ahi es una pagina rota, no un dato faltante. */
static const char *mode_name_of(int idx)
{
    if (idx < 0 || idx >= modes_count || !modes_name[idx]) return "?";
    return modes_name[idx];
}

// Sincronizacion request/response con el STM32
static SemaphoreHandle_t ack_sem;      // ACK/NACK recibido
static SemaphoreHandle_t status_sem;   // CMD_STATUS recibido
static volatile bool     last_ack_ok = false;
static ds_status_t       last_status;  // valido tras tomar status_sem
static uint16_t          last_status_len; // largo REAL de la trama (puede != sizeof)
static ds_health_t       last_health;   // valido tras tomar health_sem
static SemaphoreHandle_t health_sem;
static ds_cap_info_t     last_cap_info; // valido tras tomar cap_info_sem
static SemaphoreHandle_t cap_info_sem;
static uint8_t           last_cap_data[CAP_CHUNK_BYTES];
static uint32_t          last_cap_off;
static uint16_t          last_cap_len;
static SemaphoreHandle_t cap_data_sem;
static ds_phase_info_t   last_phase_info;
static SemaphoreHandle_t phase_info_sem;
static uint8_t           last_phase_data[CAP_CHUNK_BYTES];
static uint32_t          last_phase_off;
static uint16_t          last_phase_len;
static SemaphoreHandle_t phase_data_sem;
static ds_cal_hdr_t      last_cal_hdr;  // valido tras tomar cal_sem
static ds_cal_ch_t       last_cal_ch[CAL_CHUNK_CH];
static SemaphoreHandle_t cal_sem;

// Serializa las tramas hacia el STM32. Sin esto, el push periodico de la IP
// (monitor task) puede intercalarse con la transaccion comando→ACK de un handler
// HTTP y corromper ambas. Recursivo: uart_send_and_wait_ack() llama a
// uart_send_frame() ya teniendo el lock.
static SemaphoreHandle_t uart_tx_mutex;
#define UART_TX_LOCK()    do { if (uart_tx_mutex) xSemaphoreTakeRecursive(uart_tx_mutex, portMAX_DELAY); } while (0)
#define UART_TX_UNLOCK()  do { if (uart_tx_mutex) xSemaphoreGiveRecursive(uart_tx_mutex); } while (0)

// IP propia, publicada al STM32 para que aparezca en su heartbeat.
static char my_ip[16] = {0};
static void uart_push_esp_ip(void);   // definida junto a uart_send_frame()

// ============== WiFi ==============

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
// Redes conocidas (secrets.h) — el instrumento viaja entre casas, asi que se
// prueban en orden y se cicla entre ellas indefinidamente en background: al
// agotar los reintentos de una red se pasa a la siguiente, sin descartar
// ninguna (agregar una red nueva nunca pierde la anterior).
typedef struct { const char *ssid; const char *pass; } wifi_net_t;
static const wifi_net_t WIFI_NETWORKS[] = {
    { WIFI_SSID,   WIFI_PASS },
    { WIFI_SSID_2, WIFI_PASS_2 },
};
#define NUM_WIFI_NETWORKS (sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]))
static uint8_t wifi_net_idx = 0;

static int retry_num = 0;
#define MAX_RETRY 5   // por red, antes de pasar a la siguiente (ciclo infinito)

static httpd_handle_t server = NULL;

// ============== LED Blink ==============

// 0 = buscando WiFi (blink 200ms)
// 1 = conectado (fijo ON)
// 2 = fallo WiFi (blink lento 1s)
static volatile int led_wifi_state = 0;

static void led_blink_task(void *arg)
{
    int state = 0;
    while (1) {
        switch (led_wifi_state) {
            case 0:  // buscando — blink rapido 200ms
                state = !state;
                gpio_set_level(BLINK_GPIO, state);
                vTaskDelay(200 / portTICK_PERIOD_MS);
                break;
            case 1:  // conectado — fijo ON
                gpio_set_level(BLINK_GPIO, 1);
                vTaskDelay(500 / portTICK_PERIOD_MS);
                break;
            case 2:  // fallo — blink lento 1s
                state = !state;
                gpio_set_level(BLINK_GPIO, state);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;
            default:
                vTaskDelay(500 / portTICK_PERIOD_MS);
                break;
        }
    }
}

// ============== UART ==============

static void uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = STM32_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Orden correcto: param_config → set_pin → driver_install
    ESP_ERROR_CHECK(uart_param_config(STM32_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(STM32_UART_NUM,
                                 STM32_UART_TX, STM32_UART_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(STM32_UART_NUM,
                                        UART_BUF_SIZE * 2,
                                        UART_BUF_SIZE * 2,
                                        0, NULL, 0));

    printf("[ESP32] UART1 listo: TX=GPIO%d, RX=GPIO%d, %d baud\n",
           STM32_UART_TX, STM32_UART_RX, STM32_UART_BAUD);
}

// ============== Parser de tramas binarias STM32→ESP32 ==============
// El STM32 manda por UART4: ACK/NACK (trama corta AA 55 cmd 0D 0A, sin len)
// y CMD_STATUS (trama completa con len+payload+checksum). Todo lo que no
// matchee framing se trata como ASCII (fallback de debug).

typedef enum {
    FR_IDLE, FR_HDR2, FR_CMD, FR_LEN_L, FR_LEN_H,
    FR_PAYLOAD, FR_CKSUM, FR_FTR1, FR_FTR2
} fr_state_t;

static fr_state_t fr_state = FR_IDLE;
static uint8_t  fr_cmd, fr_cksum;
static uint16_t fr_len, fr_idx;
static uint8_t  fr_payload[PROTO_MAX_PAYLOAD];

// Devuelve true si el byte fue consumido por la capa de tramas
static bool frame_feed(uint8_t b)
{
    switch (fr_state) {
    case FR_IDLE:
        if (b == PROTO_HEADER_1) { fr_state = FR_HDR2; return true; }
        return false;                       // ASCII passthrough
    case FR_HDR2:
        fr_state = (b == PROTO_HEADER_2) ? FR_CMD : FR_IDLE;
        return true;
    case FR_CMD:
        fr_cmd = b;
        if (b == CMD_ACK || b == CMD_NACK) fr_state = FR_FTR1;  // trama corta
        else fr_state = FR_LEN_L;
        return true;
    case FR_LEN_L:
        fr_len = b; fr_state = FR_LEN_H; return true;
    case FR_LEN_H:
        fr_len |= ((uint16_t)b << 8);
        fr_idx = 0;
        if (fr_len == 0) fr_state = FR_CKSUM;
        else if (fr_len <= PROTO_MAX_PAYLOAD) fr_state = FR_PAYLOAD;
        else fr_state = FR_IDLE;            // len invalido → resync
        return true;
    case FR_PAYLOAD:
        fr_payload[fr_idx++] = b;
        if (fr_idx >= fr_len) fr_state = FR_CKSUM;
        return true;
    case FR_CKSUM:
        fr_cksum = b; fr_state = FR_FTR1; return true;
    case FR_FTR1:
        fr_state = (b == PROTO_FOOTER_1) ? FR_FTR2 : FR_IDLE;
        return true;
    case FR_FTR2:
        fr_state = FR_IDLE;
        if (b != PROTO_FOOTER_2) return true;
        // Trama completa — despachar
        if (fr_cmd == CMD_ACK || fr_cmd == CMD_NACK) {
            last_ack_ok = (fr_cmd == CMD_ACK);
            xSemaphoreGive(ack_sem);
        } else if (fr_cmd == CMD_CAP_INFO && fr_len == sizeof(ds_cap_info_t)) {
            uint8_t x = 0;
            for (uint16_t i = 0; i < fr_len; i++) x ^= fr_payload[i];
            if (x == fr_cksum) {
                memcpy(&last_cap_info, fr_payload, sizeof(ds_cap_info_t));
                xSemaphoreGive(cap_info_sem);
            }
        } else if (fr_cmd == CMD_PHASE_INFO && fr_len == sizeof(ds_phase_info_t)) {
            memcpy(&last_phase_info, fr_payload, sizeof(ds_phase_info_t));
            xSemaphoreGive(phase_info_sem);
        } else if (fr_cmd == CMD_PHASE_DATA && fr_len >= 4 && fr_len <= PROTO_MAX_PAYLOAD) {
            last_phase_off = (uint32_t)fr_payload[0]        | ((uint32_t)fr_payload[1] << 8)
                           | ((uint32_t)fr_payload[2] << 16) | ((uint32_t)fr_payload[3] << 24);
            last_phase_len = (uint16_t)(fr_len - 4);
            if (last_phase_len) memcpy(last_phase_data, &fr_payload[4], last_phase_len);
            xSemaphoreGive(phase_data_sem);
        } else if (fr_cmd == CMD_CAP_DATA && fr_len >= 4 && fr_len <= PROTO_MAX_PAYLOAD) {
            uint8_t x = 0;
            for (uint16_t i = 0; i < fr_len; i++) x ^= fr_payload[i];
            if (x == fr_cksum) {
                last_cap_off = (uint32_t)fr_payload[0]        | ((uint32_t)fr_payload[1] << 8)
                             | ((uint32_t)fr_payload[2] << 16) | ((uint32_t)fr_payload[3] << 24);
                last_cap_len = (uint16_t)(fr_len - 4);
                if (last_cap_len) memcpy(last_cap_data, &fr_payload[4], last_cap_len);
                xSemaphoreGive(cap_data_sem);
            }
        } else if (fr_cmd == CMD_MODES_DATA && fr_len >= sizeof(ds_modes_hdr_t) &&
                   fr_len <= PROTO_MAX_PAYLOAD) {
            uint8_t x = 0;
            for (uint16_t i = 0; i < fr_len; i++) x ^= fr_payload[i];
            if (x == fr_cksum) {
                memcpy(&last_modes_hdr, fr_payload, sizeof(ds_modes_hdr_t));
                last_modes_len = (uint16_t)(fr_len - sizeof(ds_modes_hdr_t));
                /* El largo real manda sobre lo que dice la cabecera: una trama
                 * truncada con `bytes` alto haria leer basura del buffer. */
                if (last_modes_hdr.bytes > last_modes_len)
                    last_modes_hdr.bytes = (uint8_t)last_modes_len;
                if (last_modes_len)
                    memcpy(last_modes_buf, &fr_payload[sizeof(ds_modes_hdr_t)],
                           last_modes_len);
                xSemaphoreGive(modes_sem);
            }
        } else if (fr_cmd == CMD_CAL_DATA && fr_len >= sizeof(ds_cal_hdr_t) &&
                   fr_len <= PROTO_MAX_PAYLOAD) {
            uint8_t x = 0;
            for (uint16_t i = 0; i < fr_len; i++) x ^= fr_payload[i];
            if (x == fr_cksum) {
                ds_cal_hdr_t h;
                memcpy(&h, fr_payload, sizeof(ds_cal_hdr_t));
                /* El largo manda sobre lo que dice la cabecera: una trama
                 * truncada con count alto leeria basura del buffer. */
                uint16_t have = (uint16_t)((fr_len - sizeof(ds_cal_hdr_t)) / sizeof(ds_cal_ch_t));
                if (h.count > have) h.count = (uint8_t)have;
                if (h.count > CAL_CHUNK_CH) h.count = CAL_CHUNK_CH;
                last_cal_hdr = h;
                if (h.count) memcpy(last_cal_ch, &fr_payload[sizeof(ds_cal_hdr_t)],
                                    (size_t)h.count * sizeof(ds_cal_ch_t));
                xSemaphoreGive(cal_sem);
            }
        } else if (fr_cmd == CMD_HEALTH && fr_len == sizeof(ds_health_t)) {
            uint8_t x = 0;
            for (uint16_t i = 0; i < fr_len; i++) x ^= fr_payload[i];
            if (x == fr_cksum) {
                memcpy(&last_health, fr_payload, sizeof(ds_health_t));
                xSemaphoreGive(health_sem);
            }
        } else if (fr_cmd == CMD_STATUS && fr_len >= 3 && fr_len <= PROTO_MAX_PAYLOAD) {
            /* Se acepta CUALQUIER largo razonable, no solo el exacto.
             *
             * Antes se exigia fr_len == sizeof(ds_status_t) y la trama de un
             * STM32 con otra version de protocolo se descartaba sin mas: la
             * pagina mostraba "stm32: timeout", que es lo MISMO que muestra si
             * la placa esta muerta. Un desajuste de firmwares —el caso normal
             * mientras se actualiza uno de los dos— era indistinguible de una
             * falla de hardware.
             *
             * El primer byte del status es proto_ver, asi que una trama de otro
             * largo igual dice con que version fue hecha. Se copia el prefijo
             * comun sobre un struct en cero y la pagina compara versiones y dice
             * cual de los dos hay que actualizar. */
            uint8_t x = 0;
            for (uint16_t i = 0; i < fr_len; i++) x ^= fr_payload[i];
            if (x == fr_cksum) {
                uint16_t n = (fr_len < sizeof(ds_status_t)) ? fr_len : sizeof(ds_status_t);
                memset(&last_status, 0, sizeof(ds_status_t));
                memcpy(&last_status, fr_payload, n);
                last_status_len = fr_len;
                xSemaphoreGive(status_sem);
            }
        }
        return true;
    }
    return false;
}

// Tarea que lee continuamente del STM32: tramas binarias → dispatcher,
// resto → forwardea al terminal con prefijo [STM32]
static void stm32_monitor_task(void *arg)
{
    uint8_t raw[128];
    char    line[STM32_LINE_BUF];
    int     line_idx = 0;
    uint32_t total_bytes = 0;
    uint32_t last_report = 0;
    uint32_t last_ip_push = 0;

    printf("[ESP32] Monitor STM32 iniciado (UART%d RX=GPIO%d, %d baud)\n",
           STM32_UART_NUM, STM32_UART_RX, STM32_UART_BAUD);

    while (1) {
        /* Se espera al PRIMER byte y despues se vacia lo que ya este en el
         * buffer, sin esperar mas. Antes esto pedia sizeof(raw) bytes con 100 ms
         * de plazo, y uart_read_bytes no vuelve hasta juntar los que le pediste
         * o agotar el plazo: como una respuesta del STM32 son 5 bytes, SIEMPRE
         * se comian los 100 ms enteros.
         *
         * Con una consulta de estado por segundo eso no se notaba. Con una carga
         * de firmware, que son ~660 tramos y cada uno espera su ACK, son 100 ms
         * por tramo = mas de un minuto de puro plazo vencido, y encima hacia que
         * el push periodico de IP cayera en medio de una transaccion (ver
         * uart_push_esp_ip). Medido: 9 tramos/s cuando el cable da 80. */
        int len = uart_read_bytes(STM32_UART_NUM, raw, 1, 100 / portTICK_PERIOD_MS);
        if (len == 1) {
            int more = uart_read_bytes(STM32_UART_NUM, raw + 1, sizeof(raw) - 1, 0);
            if (more > 0) len += more;
        }

        // Periodic report even if no data
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_report >= 5000) {
            last_report = now;
            printf("[ESP32] STM32 monitor: %lu bytes recibidos total\n", total_bytes);
        }

        // Publicar la IP al STM32 (para su heartbeat). Periodico, asi un reset
        // del STM32 la recupera sin reiniciar el ESP32.
        if (my_ip[0] && (last_ip_push == 0 || now - last_ip_push >= ESP_IP_PUSH_MS)) {
            last_ip_push = now;
            uart_push_esp_ip();
        }

        if (len <= 0) continue;
        total_bytes += len;

        // Debug: show first raw bytes when data starts arriving
        if (total_bytes == (uint32_t)len) {
            printf("[ESP32] Primer dato! %d bytes: ", len);
            for (int i = 0; i < len && i < 20; i++)
                printf("%02X ", raw[i]);
            printf("\n");
        }

        for (int i = 0; i < len; i++) {
            if (frame_feed(raw[i])) continue;   // byte de trama binaria
            char c = (char)raw[i];

            if (c == '\n') {
                line[line_idx] = '\0';
                if (line_idx > 0) {
                    printf("[STM32] %s\n", line);
                    fflush(stdout);
                }
                line_idx = 0;
            } else if (c == '\r') {
                // ignorar CR
            } else if (line_idx < STM32_LINE_BUF - 1) {
                line[line_idx++] = c;
            } else {
                // linea demasiado larga — flush y empezar nueva
                line[line_idx] = '\0';
                printf("[STM32] %s[...]\n", line);
                fflush(stdout);
                line_idx = 0;
                line[line_idx++] = c;
            }
        }
    }
}

// ============== Protocolo binario ==============

static uint8_t calc_checksum(const uint8_t *data, int len)
{
    uint8_t xor = 0;
    for (int i = 0; i < len; i++) xor ^= data[i];
    return xor;
}

// Forward: definido mas abajo, junto al resto del transporte UART.
static int uart_send_and_wait_ack(uint8_t cmd, const uint8_t *payload,
                                  uint16_t len, int timeout_ms);

// Serializa la config y la manda. Devuelve 1 = ACK, 0 = NACK, -1 = timeout.
//
// El armado de la trama y la espera del ACK los hace uart_send_and_wait_ack,
// igual que el resto de los comandos: antes esta funcion tenia su PROPIA copia
// del framing y mandaba a ciegas, sin lock del UART ni ACK. El HTTP contestaba
// "Config sent to STM32" pasara lo que pasara, asi que un largo de payload mal
// calculado —justo lo que cambio al pasar de 16 a 32 canales— se habria visto
// como exito.
static int uart_send_config(const ds_config_t *cfg)
{
    // master_gain(1) + mute_global(1) + delays(64) + gains(32) + mutes(4) = 102
    const uint16_t payload_len = 1 + 1 + (NUM_CHANNELS * 2) + NUM_CHANNELS + 4;
    uint8_t payload[sizeof(ds_config_t)];
    int idx = 0;

    payload[idx++] = cfg->master_gain;
    payload[idx++] = cfg->mute_global;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        payload[idx++] = cfg->delays[i] & 0xFF;
        payload[idx++] = (cfg->delays[i] >> 8) & 0xFF;
    }
    for (int i = 0; i < NUM_CHANNELS; i++) {
        payload[idx++] = cfg->gains[i];
    }
    payload[idx++] = cfg->mutes & 0xFF;
    payload[idx++] = (cfg->mutes >> 8) & 0xFF;
    payload[idx++] = (cfg->mutes >> 16) & 0xFF;
    payload[idx++] = (cfg->mutes >> 24) & 0xFF;

    int ack = uart_send_and_wait_ack(CMD_UPDATE_CFG, payload, payload_len, 400);
    printf("[ESP32] Config -> STM32: %u bytes, %s (master_gain=%d, mute_global=%d)\n",
           (unsigned)payload_len,
           ack == 1 ? "ACK" : (ack == 0 ? "NACK" : "sin respuesta"),
           cfg->master_gain, cfg->mute_global);
    return ack;
}

// Trama generica: Header + Cmd + Len + Payload + Checksum + Footer
static void uart_send_frame(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    uint8_t frame[8 + PROTO_MAX_PAYLOAD];
    int fi = 0;
    frame[fi++] = PROTO_HEADER_1;
    frame[fi++] = PROTO_HEADER_2;
    frame[fi++] = cmd;
    frame[fi++] = len & 0xFF;
    frame[fi++] = (len >> 8) & 0xFF;
    if (len) { memcpy(&frame[fi], payload, len); fi += len; }
    frame[fi++] = calc_checksum(payload, len);   // len=0 → 0
    frame[fi++] = PROTO_FOOTER_1;
    frame[fi++] = PROTO_FOOTER_2;
    UART_TX_LOCK();
    uart_write_bytes(STM32_UART_NUM, frame, fi);
    UART_TX_UNLOCK();
}

// Publica la IP al STM32 (aparece en su heartbeat). Sin ACK: es un push.
//
// ⚠️ Toma el lock SIN ESPERAR, y si esta ocupado se saltea. La razon es un
// abrazo mortal real, encontrado el 2026-08-23 depurando la carga de firmware:
//
//   - esto corre en stm32_monitor_task, que es el UNICO que lee el UART y por
//     lo tanto el unico que puede detectar un ACK;
//   - uart_send_and_wait_ack retiene el lock durante TODA la transaccion,
//     incluida la espera del ACK (hasta 3 s en una carga de firmware);
//   - si el push cae ahi, el monitor se queda esperando el lock, deja de leer,
//     el ACK del STM32 llega y NADIE lo ve. La carga muere por plazo vencido en
//     un tramo al azar, y desde afuera parece que el STM32 dejo de contestar.
//
// El push es periodico cada 10 s y solo informativo: saltearse uno no cuesta
// nada. Bloquear al lector, si.
static void uart_push_esp_ip(void)
{
    if (!my_ip[0]) return;
    if (uart_tx_mutex && xSemaphoreTakeRecursive(uart_tx_mutex, 0) != pdTRUE) return;
    uart_send_frame(CMD_ESP_INFO, (const uint8_t *)my_ip, strlen(my_ip));
    if (uart_tx_mutex) xSemaphoreGiveRecursive(uart_tx_mutex);
}

// Envia un comando y espera el ACK/NACK del STM32.
// Devuelve: 1 = ACK, 0 = NACK, -1 = timeout.
static int uart_send_and_wait_ack(uint8_t cmd, const uint8_t *payload,
                                  uint16_t len, int timeout_ms)
{
    // Lock sobre toda la transaccion: nadie puede meter una trama entre el
    // comando y su ACK.
    UART_TX_LOCK();
    xSemaphoreTake(ack_sem, 0);                 // drenar ACK viejo
    uart_send_frame(cmd, payload, len);
    int rc = (xSemaphoreTake(ack_sem, timeout_ms / portTICK_PERIOD_MS) != pdTRUE)
             ? -1 : (last_ack_ok ? 1 : 0);
    UART_TX_UNLOCK();
    return rc;
}

// ============== JSON Parsing ==============

static bool parse_and_send_command(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        printf("[ESP32] ERROR: JSON parse failed\n");
        return false;
    }

    printf("[ESP32] ========== COMANDO RECIBIDO ==========\n");

    cJSON *command      = cJSON_GetObjectItem(root, "command");
    cJSON *device_id    = cJSON_GetObjectItem(root, "device_id");
    cJSON *mute         = cJSON_GetObjectItem(root, "mute");
    cJSON *master_gain  = cJSON_GetObjectItem(root, "master_gain");
    cJSON *focal_dist   = cJSON_GetObjectItem(root, "focal_distance");

    if (command)    printf("[ESP32]   command:        %s\n", command->valuestring);
    if (device_id)  printf("[ESP32]   device_id:      %s\n", device_id->valuestring);
    if (mute)       printf("[ESP32]   mute:           %s\n", cJSON_IsTrue(mute) ? "true" : "false");
    if (master_gain)printf("[ESP32]   master_gain:    %.2f (DEPRECADO: el STM32 ya no lo aplica — usar POST /volume)\n", master_gain->valuedouble);
    if (focal_dist) printf("[ESP32]   focal_distance: %.2f m\n", focal_dist->valuedouble);

    ds_config_t cfg = {0};

    if (master_gain)
        cfg.master_gain = (uint8_t)(master_gain->valuedouble * 255.0);
    cfg.mute_global = (mute && cJSON_IsTrue(mute)) ? 1 : 0;

    cJSON *channels = cJSON_GetObjectItem(root, "channels");
    if (channels && cJSON_IsArray(channels)) {
        int n = cJSON_GetArraySize(channels);
        printf("[ESP32]   canales: %d\n", n);
        cJSON *ch;
        cJSON_ArrayForEach(ch, channels) {
            cJSON *ch_num  = cJSON_GetObjectItem(ch, "channel_num");
            cJSON *delay   = cJSON_GetObjectItem(ch, "delay_samples");
            cJSON *gain    = cJSON_GetObjectItem(ch, "channel_gain");
            cJSON *ch_mute = cJSON_GetObjectItem(ch, "mute");

            int c = ch_num ? ch_num->valueint : -1;
            printf("[ESP32]   CH%02d: delay=%4d gain=%.2f mute=%d\n",
                   c,
                   delay   ? delay->valueint          : 0,
                   gain    ? gain->valuedouble         : 0.0,
                   ch_mute ? ch_mute->valueint         : 0);

            if (c >= 0 && c < NUM_CHANNELS) {
                cfg.delays[c] = delay   ? (uint16_t)delay->valueint              : 0;
                cfg.gains[c]  = gain    ? (uint8_t)(gain->valuedouble * 255.0)   : 0;
                if (ch_mute && ch_mute->valueint)
                    cfg.mutes |= (1u << c);   // 1u: con 32 canales c llega a 31
            }
        }
    }

    printf("[ESP32] ========================================\n");

    int ack = uart_send_config(&cfg);
    cJSON_Delete(root);
    return ack == 1;      // solo es exito si el STM32 la acepto
}

// ============== HTTP Handlers ==============

static esp_err_t command_handler(httpd_req_t *req)
{
    /* Un JSON con los 32 canales son ~2,5 KB — con 2048 no entraba y el POST
     * moria en "Content too large". 4096 deja margen para formato indentado.
     * `static` para que viva en .bss y no como marco de 4 KB en la pila de la
     * task HTTP (8 KB en total); el server corre en una sola task, asi que no
     * hay dos requests usandolo a la vez. */
    static char buf[4096];
    int received  = 0;
    int remaining = req->content_len;

    printf("[ESP32] HTTP POST /command — Content-Length: %d\n", req->content_len);

    if (remaining > (int)(sizeof(buf) - 1)) {
        printf("[ESP32] ERROR: payload demasiado grande\n");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too large");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int ret = httpd_req_recv(req, buf + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        received  += ret;
        remaining -= ret;
    }
    buf[received] = '\0';

    bool ok = parse_and_send_command(buf);

    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"status\":\"%s\",\"device_id\":\"%s\",\"message\":\"%s\"}",
        ok ? "ok" : "error", device_id,
        ok ? "Config aplicada por el STM32"
           : "JSON invalido, o el STM32 no la acepto (ver log serie)");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// GET /status — pide CMD_GET_STATUS al STM32 y devuelve la telemetria real
/* Identidad de ESTE chip, para el JSON de /status.
 *
 * Va en las DOS ramas del handler (con y sin respuesta del STM32) a proposito:
 * cuando el STM32 esta callado es justamente cuando mas hace falta saber que
 * firmware tiene el ESP32 y con que version de protocolo fue compilado — es la
 * mitad del diagnostico de "por que no se hablan".
 *
 * La version sale de git sin configurar nada: si no hay PROJECT_VER ni
 * version.txt, ESP-IDF usa `git describe --always --tags --dirty`. */
static int esp_self_json(char *out, size_t n)
{
    const esp_app_desc_t  *d = esp_app_get_description();
    const esp_partition_t *r = esp_ota_get_running_partition();
    /* El identificador es el SHA-256 del ELF, no la fecha de compilacion.
     *
     * `d->date`/`d->time` NO se actualizan entre compilaciones: salen de
     * __DATE__/__TIME__ de un archivo de ESP-IDF que no se recompila salvo que
     * cambie el, y quedan congelados en la primera vez. Medido: dos binarios
     * distintos reportaban la misma hora al segundo. Un identificador que a
     * veces miente es peor que no tener ninguno.
     *
     * El SHA del ELF, en cambio, cambia con cada binario distinto — y NO cambia
     * si el binario es el mismo, que es exactamente la propiedad que se quiere.
     * ESP-IDF ya lo guarda (CONFIG_APP_RETRIEVE_LEN_ELF_SHA=9). */
    return snprintf(out, n,
        "\"esp_fw\":\"%s\",\"esp_sha\":\"%s\",\"esp_slot\":\"%s\","
        "\"proto_esp\":%u",
        d ? d->version : "?", esp_app_get_elf_sha256_str(),
        r ? r->label : "?", PROTO_VERSION);
}

/** Por que arranco el STM32, en una palabra.
 *
 * Se decodifica ACA y no en el STM32 a proposito: el STM32 manda RCC->RSR crudo,
 * asi que una captura vieja se puede reinterpretar cuando cambie lo que sabemos
 * de los flags. Ya paso una vez — BOR y POR no se distinguen con BOR_LEV de
 * fabrica, y el firmware afirmaba que si. */
static const char *reset_name(uint32_t rsr)
{
    if (rsr & (1u << 19)) return "POR";    /* PORRSTF  */
    if (rsr & (1u << 21)) return "BOR";    /* BORRSTF  */
    if (rsr & (1u << 26)) return "IWDG";   /* IWDG1RSTF */
    if (rsr & (1u << 25)) return "WWDG";   /* WWDG1RSTF */
    if (rsr & (1u << 30)) return "LPWR";   /* LPWRRSTF */
    if (rsr & (1u << 24)) return "SFT";    /* SFTRSTF  */
    if (rsr & (1u << 22)) return "PIN";    /* PINRSTF  */
    return "?";
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char self[192];
    esp_self_json(self, sizeof(self));

    xSemaphoreTake(status_sem, 0);              // drenar status viejo
    uart_send_frame(CMD_GET_STATUS, NULL, 0);

    /* 768: el JSON ronda los 550 chars con los campos de la v8. snprintf trunca
     * en silencio, y un JSON truncado hace que la pagina entera se quede en
     * "sin conexion" sin decir por que — asi que conviene el margen. */
    char buf[960];
    /* 800 ms, no 400. El STM32 manda el status byte a byte sirviendo los fills
     * de audio entre medio (para no meter un pop de 6 ms), asi que en el modo
     * mas pesado —el 15, 8 fuentes x 32 salidas— la trama entera puede tardar
     * varios cientos de ms. Con 400 ms se veian timeouts esporadicos JUSTO en
     * el modo que mas interesa monitorear. El audio siempre gana; la telemetria
     * espera. Sigue entrando en el poll de 1 s de la pagina. */
    if (xSemaphoreTake(status_sem, 800 / portTICK_PERIOD_MS) == pdTRUE) {
        const ds_status_t *s = &last_status;
        modes_ensure();
        const char *mname = mode_name_of(s->mode_idx);
        snprintf(buf, sizeof(buf),
            "{\"stm32\":\"ok\",\"device_id\":\"%s\","
            "\"proto_ver\":%u,\"mode\":%u,\"mode_name\":\"%s\",\"num_modes\":%u,"
            "\"rx_az\":%d,\"focus\":{\"fx\":%d,\"fy\":%d,\"fz\":%d},"
            "\"underruns\":%lu,\"sai_errors\":%lu,\"ovf\":%u,\"cal_state\":%u,"
            "\"vol_db\":%d,\"ingain_db\":%u,\"amps\":%s,\"custom\":%s,"
            "\"adc_ovf\":%lu,\"out_clip\":%lu,"
            "\"adc_pk\":[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld],"
            "\"cpu_fills_pct\":%u.%u,"
            "\"mic_live\":%u,\"cal_prog\":%u,\"cal_ok\":%u,\"cal_saved\":%s,"
            "\"fw_id\":\"%08lX\",\"fw_build\":%lu,\"ota_state\":%u,\"ota_pct\":%u,"
            "\"dma_phase\":%d,\"autostart\":%s,\"autostart_held\":%s,"
            "\"reset\":\"%s\",\"reset_rsr\":\"%08lX\","
            "%s,\"status_len\":%u,"
            "\"stm32_uptime_ms\":%lu,\"uptime_ms\":%lu}",
            device_id,
            s->proto_ver, s->mode_idx, mname, s->num_modes,
            s->rx_az_deg, s->fx_mm, s->fy_mm, s->fz_mm,
            (unsigned long)s->underruns, (unsigned long)s->sai_errors,
            s->ovf, s->cal_state, (int)s->master_vol_db,
            s->in_gain_db,
            (s->state_flags & STATE_F_AMPS)   ? "true" : "false",
            (s->state_flags & STATE_F_CUSTOM) ? "true" : "false",
            (unsigned long)s->adc_ovf_count, (unsigned long)s->out_clip_count,
            (long)s->adc_pk[0], (long)s->adc_pk[1], (long)s->adc_pk[2],
            (long)s->adc_pk[3], (long)s->adc_pk[4], (long)s->adc_pk[5],
            (long)s->adc_pk[6], (long)s->adc_pk[7],
            /* Los DOS uptimes, separados a proposito: uptime_ms era el de este
             * chip y se leia como si fuera el de la placa, asi que un reinicio
             * del STM32 resultaba INVISIBLE desde la red. */
            s->cpu_fills_x10 / 10, s->cpu_fills_x10 % 10,
            s->mic_live_mask, s->cal_progress, s->cal_ok_count,
            (s->state_flags & STATE_F_CALIB) ? "true" : "false",
            (unsigned long)s->fw_id, (unsigned long)s->fw_build,
            s->ota_state, s->ota_pct,
            /* Vale sólo si la trama llego completa: un STM32 con firmware
             * viejo manda 83 bytes y este campo seria basura de pila. */
            (last_status_len >= sizeof(ds_status_t)) ? (int)s->dma_phase : -1,
            (s->state_flags & STATE_F_AUTOSTART) ? "true" : "false",
            (s->state_flags & STATE_F_AUTO_HELD) ? "true" : "false",
            reset_name(s->reset_rsr), (unsigned long)s->reset_rsr,
            self, (unsigned)last_status_len,
            (unsigned long)s->uptime_ms,
            (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    } else {
        /* Sin respuesta del STM32 no hay uptime suyo que informar; el que va
         * es el de este chip, y por eso lleva su nombre. */
        snprintf(buf, sizeof(buf),
            "{\"stm32\":\"timeout\",\"device_id\":\"%s\","
            "%s,\"uptime_ms\":%lu}",
            device_id, self,
            (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

// POST /mode {"mode":N} — cambia el modo de operacion (indice de ui_states)
static esp_err_t mode_handler(httpd_req_t *req)
{
    char buf[128];
    int n = httpd_req_recv(req, buf, req->content_len < (int)sizeof(buf) - 1
                                     ? req->content_len : (int)sizeof(buf) - 1);
    if (n <= 0) return ESP_FAIL;
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    cJSON *mode = root ? cJSON_GetObjectItem(root, "mode") : NULL;
    int idx = (mode && cJSON_IsNumber(mode)) ? mode->valueint : -1;
    if (root) cJSON_Delete(root);

    char resp[192];
    /* El limite sale de la cache, que sale del STM32. Si la cache no se pudo
     * llenar NO se inventa un limite: se manda igual y decide el STM32, que es
     * el unico que sabe. Desde la v12 contesta NACK a un modo que no existe, asi
     * que un indice de mas se ve como "nack" y no como un "ok" que no hace nada. */
    bool have = modes_ensure();
    if (idx < 0 || (have && idx >= (int)modes_count)) {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"error\",\"message\":\"mode 0-%d\"}",
                 (int)modes_count - 1);
    } else {
        uint8_t payload = (uint8_t)idx;
        int ack = uart_send_and_wait_ack(CMD_SET_MODE, &payload, 1, 300);
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"%s\",\"mode\":%d,\"mode_name\":\"%s\"}",
                 ack == 1 ? "ok" : (ack == 0 ? "nack" : "timeout"),
                 idx, mode_name_of(idx));
        printf("[ESP32] POST /mode %d (%s) -> %s\n", idx, mode_name_of(idx),
               ack == 1 ? "ACK" : "sin ACK");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /beam — steering custom fuera de los presets
//   {"type":"rx","az":30}                      → beam RX a 30°
//   {"type":"tx","fx":0,"fy":0,"fz":1000}      → foco TX en (0,0,1000)mm
//   {"type":"mf","src":2,"fx":300,"fy":-240,"fz":1000}
//       → MULTIFOCUS: mueve SOLO el foco de la fuente 2 (0-3). En la trama
//         binaria el indice viaja en el campo az (type=2, sin cambio de layout)
static esp_err_t beam_handler(httpd_req_t *req)
{
    char buf[256];
    int n = httpd_req_recv(req, buf, req->content_len < (int)sizeof(buf) - 1
                                     ? req->content_len : (int)sizeof(buf) - 1);
    if (n <= 0) return ESP_FAIL;
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"bad json\"}");
        return ESP_OK;
    }
    cJSON *type = cJSON_GetObjectItem(root, "type");
    bool is_tx = type && type->valuestring && strcmp(type->valuestring, "tx") == 0;
    bool is_mf = type && type->valuestring && strcmp(type->valuestring, "mf") == 0;
    cJSON *jaz = cJSON_GetObjectItem(root, "az");
    cJSON *jsrc = cJSON_GetObjectItem(root, "src");
    cJSON *jfx = cJSON_GetObjectItem(root, "fx");
    cJSON *jfy = cJSON_GetObjectItem(root, "fy");
    cJSON *jfz = cJSON_GetObjectItem(root, "fz");
    int16_t az = jaz ? (int16_t)jaz->valueint : 0;
    if (is_mf) az = jsrc ? (int16_t)jsrc->valueint : 0;   // az = indice de fuente
    int16_t fx = jfx ? (int16_t)jfx->valueint : 0;
    int16_t fy = jfy ? (int16_t)jfy->valueint : 0;
    int16_t fz = jfz ? (int16_t)jfz->valueint : 1000;
    cJSON_Delete(root);

    // Payload ds_beam_req_t: type(1) + az(2) + fx(2) + fy(2) + fz(2), LE
    uint8_t p[9];
    p[0] = is_mf ? 2 : (is_tx ? 1 : 0);
    p[1] = az & 0xFF;  p[2] = (az >> 8) & 0xFF;
    p[3] = fx & 0xFF;  p[4] = (fx >> 8) & 0xFF;
    p[5] = fy & 0xFF;  p[6] = (fy >> 8) & 0xFF;
    p[7] = fz & 0xFF;  p[8] = (fz >> 8) & 0xFF;
    int ack = uart_send_and_wait_ack(CMD_SET_BEAM, p, sizeof(p), 300);

    char resp[160];
    snprintf(resp, sizeof(resp),
             "{\"status\":\"%s\",\"type\":\"%s\",\"az\":%d,"
             "\"fx\":%d,\"fy\":%d,\"fz\":%d}",
             ack == 1 ? "ok" : (ack == 0 ? "nack" : "timeout"),
             is_mf ? "mf" : (is_tx ? "tx" : "rx"), az, fx, fy, fz);
    printf("[ESP32] POST /beam %s -> %s\n",
           is_mf ? "mf" : (is_tx ? "tx" : "rx"),
           ack == 1 ? "ACK" : "sin ACK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /amps {"on":true|false} — enable del banco de 32 amplificadores.
// Los amps arrancan APAGADOS (ver docs/ds11_errata.md): con ellos off corre
// todo el pipeline en silencio, lo que aisla si un reinicio viene de la
// demanda de los PAM8302A sobre el buck.
/** POST /autostart {"on":true|false}
 *
 * Con esto en true el nodo enciende sus 32 amplificadores SOLO al arrancar, sin
 * red y sin que nadie mande nada. Es lo que hace que un nodo que perdio el WiFi
 * no quede mudo toda la funcion (issue #19).
 *
 * Se persiste en la EEPROM del STM32, o sea que sobrevive a un corte de luz —
 * que es exactamente el caso que hay que cubrir. */
static esp_err_t autostart_handler(httpd_req_t *req)
{
    char buf[128];
    int n = httpd_req_recv(req, buf, req->content_len < (int)sizeof(buf) - 1
                                     ? req->content_len : (int)sizeof(buf) - 1);
    if (n <= 0) return ESP_FAIL;
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"bad json\"}");
        return ESP_OK;
    }
    cJSON *jon = cJSON_GetObjectItem(root, "on");
    uint8_t on = (jon && (cJSON_IsTrue(jon) || jon->valueint)) ? 1 : 0;
    cJSON_Delete(root);

    int ack = uart_send_and_wait_ack(CMD_SET_AUTOSTART, &on, 1, 500);
    char resp[96];
    snprintf(resp, sizeof(resp), "{\"status\":\"%s\",\"autostart\":%s}",
             ack == 1 ? "ok" : (ack == 0 ? "nack" : "timeout"), on ? "true" : "false");
    printf("[ESP32] POST /autostart %s -> %s\n", on ? "ON" : "OFF",
           ack == 1 ? "ACK" : "sin ACK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* POST /tone {"hz":2000,"dither":0}
 *
 * Frecuencia del emisor y dither, para MEDIR. Sin esto una medicion automatica
 * no repite: la captura son 170 ms contra el LFO de 333 ms de los modos
 * "ondulante", asi que cada corrida cae en otra fase del dither.
 *
 * Vale solo mientras dure el modo: elegir un modo vuelve a poner la frecuencia
 * de la tabla y re-enciende el dither. Es a proposito — ver el comentario de
 * CMD_SET_TONE en uart_protocol.h. */
/* POST /mics {"m1":0,"m2":7}
 *
 * Que entradas del ADC son los microfonos de medicion. Estaban clavadas en 0 y
 * 1; el arnes real dio senal en c0 y c7. No se persiste a proposito: es un dato
 * del CABLEADO de este banco, y que sobreviva a un reinicio significa que
 * dentro de un mes alguien mide con el cableado de hoy sin saberlo. */
static esp_err_t mics_handler(httpd_req_t *req)
{
    char buf[128];
    int n = httpd_req_recv(req, buf, req->content_len < (int)sizeof(buf) - 1
                                     ? req->content_len : (int)sizeof(buf) - 1);
    if (n <= 0) return ESP_FAIL;
    buf[n] = '\0';
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"bad json\"}");
        return ESP_OK;
    }
    cJSON *j1 = cJSON_GetObjectItem(root, "m1");
    cJSON *j2 = cJSON_GetObjectItem(root, "m2");
    int m1 = j1 ? j1->valueint : 0;
    int m2 = j2 ? j2->valueint : 1;
    cJSON_Delete(root);
    if (m1 < 0 || m1 > 7) m1 = 0;
    if (m2 < 0 || m2 > 7) m2 = 1;

    uint8_t p[2] = { (uint8_t)m1, (uint8_t)m2 };
    int ack = uart_send_and_wait_ack(CMD_SET_MICS, p, sizeof(p), 500);
    char resp[96];
    snprintf(resp, sizeof(resp), "{\"status\":\"%s\",\"m1\":%d,\"m2\":%d}",
             ack == 1 ? "ok" : (ack == 0 ? "nack" : "timeout"), m1, m2);
    printf("[ESP32] POST /mics c%d y c%d -> %s\n", m1, m2,
           ack == 1 ? "ACK" : "sin ACK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t tone_handler(httpd_req_t *req)
{
    char buf[128];
    int n = httpd_req_recv(req, buf, req->content_len < (int)sizeof(buf) - 1
                                     ? req->content_len : (int)sizeof(buf) - 1);
    if (n <= 0) return ESP_FAIL;
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"bad json\"}");
        return ESP_OK;
    }
    cJSON *jhz  = cJSON_GetObjectItem(root, "hz");
    cJSON *jdit = cJSON_GetObjectItem(root, "dither");
    int hz = jhz ? jhz->valueint : 2000;
    /* El clamp REAL lo hace el STM32 (20..8000) y lo avisa por el serie. Aca
     * solo se acota a lo que entra en 16 bits, para no mandar basura. */
    if (hz < 0) hz = 0;
    if (hz > 65535) hz = 65535;
    uint8_t dit = (jdit && (cJSON_IsTrue(jdit) || jdit->valueint)) ? 1 : 0;
    cJSON_Delete(root);

    uint8_t p[3] = { (uint8_t)(hz & 0xFF), (uint8_t)((hz >> 8) & 0xFF), dit };
    int ack = uart_send_and_wait_ack(CMD_SET_TONE, p, sizeof(p), 500);

    char resp[96];
    snprintf(resp, sizeof(resp), "{\"status\":\"%s\",\"hz\":%d,\"dither\":%s}",
             ack == 1 ? "ok" : (ack == 0 ? "nack" : "timeout"), hz,
             dit ? "true" : "false");
    printf("[ESP32] POST /tone %d Hz dither=%u -> %s\n", hz, (unsigned)dit,
           ack == 1 ? "ACK" : "sin ACK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t amps_handler(httpd_req_t *req)
{
    char buf[128];
    int n = httpd_req_recv(req, buf, req->content_len < (int)sizeof(buf) - 1
                                     ? req->content_len : (int)sizeof(buf) - 1);
    if (n <= 0) return ESP_FAIL;
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"bad json\"}");
        return ESP_OK;
    }
    cJSON *jon = cJSON_GetObjectItem(root, "on");
    uint8_t on = (jon && (cJSON_IsTrue(jon) || jon->valueint)) ? 1 : 0;
    cJSON_Delete(root);

    int ack = uart_send_and_wait_ack(CMD_SET_AMPS, &on, 1, 300);
    char resp[96];
    snprintf(resp, sizeof(resp), "{\"status\":\"%s\",\"amps\":%s}",
             ack == 1 ? "ok" : (ack == 0 ? "nack" : "timeout"), on ? "true" : "false");
    printf("[ESP32] POST /amps %s -> %s\n", on ? "ON" : "OFF",
           ack == 1 ? "ACK" : "sin ACK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /reset — reset de fabrica. Sin payload: no hay nada que elegir.
//
// El instrumento tiene muchos parametros que se pisan en vivo (foco por fuente,
// azimut, mutes, ganancias, volumen, ganancia de entrada) y varios se guardan
// solos en la EEPROM. Sin una vuelta a cero se puede quedar en un estado raro
// sin forma evidente de salir — y quien lo opera no necesariamente sabe que
// parametro toco. Deja volumen -24 dB, ganancia 0 dB, modo 0 y los focos por
// defecto. NO toca el enable de los amplificadores: apagarlos de sorpresa (o
// encenderlos) es justo lo que no se quiere de un boton de panico.
static esp_err_t reset_handler(httpd_req_t *req)
{
    int ack = uart_send_and_wait_ack(CMD_FACTORY_RESET, NULL, 0, 600);
    char resp[96];
    snprintf(resp, sizeof(resp), "{\"status\":\"%s\"}",
             ack == 1 ? "ok" : (ack == 0 ? "nack" : "timeout"));
    printf("[ESP32] POST /reset -> %s\n", ack == 1 ? "ACK" : "sin ACK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /ingain {"db":0} — ganancia de la etapa de ENTRADA, 0..36 dB.
//
// NO confundir con /volume: aquel es el atenuador del PCM1690, o sea la SALIDA.
// Este es cuanto se amplifica la senal DESPUES del ADC y ANTES del clamp digital.
//
// Por que existe: el modo 0 aplicaba +30 dB fijos (para poder oir un microfono) y
// el volumen tiraba -40 dB despues. Con una fuente de linea eso hacia recortar el
// clamp digital mucho antes de que a los parlantes les llegara nada — medido el
// 2026-08-18: 3152 recortes/s con el ADC ni enterado. Para fuentes de linea va en
// 0-12 dB; +30 es para microfonos.
static esp_err_t ingain_handler(httpd_req_t *req)
{
    char buf[128];
    int n = httpd_req_recv(req, buf, req->content_len < (int)sizeof(buf) - 1
                                     ? req->content_len : (int)sizeof(buf) - 1);
    if (n <= 0) return ESP_FAIL;
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"bad json\"}");
        return ESP_OK;
    }
    cJSON *jdb = cJSON_GetObjectItem(root, "db");
    int db = (jdb && cJSON_IsNumber(jdb)) ? jdb->valueint : 0;
    cJSON_Delete(root);
    if (db < 0)  db = 0;
    if (db > 36) db = 36;

    uint8_t p = (uint8_t)db;
    int ack = uart_send_and_wait_ack(CMD_SET_INGAIN, &p, 1, 300);
    char resp[96];
    snprintf(resp, sizeof(resp), "{\"status\":\"%s\",\"ingain_db\":%d}",
             ack == 1 ? "ok" : (ack == 0 ? "nack" : "timeout"), db);
    printf("[ESP32] POST /ingain %d dB -> %s\n", db, ack == 1 ? "ACK" : "sin ACK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// POST /volume {"db":-20} — volumen maestro via atenuacion del PCM1690.
// Afecta TODOS los modos (walker incluido), a diferencia de master_gain.
static esp_err_t volume_handler(httpd_req_t *req)
{
    char buf[128];
    int n = httpd_req_recv(req, buf, req->content_len < (int)sizeof(buf) - 1
                                     ? req->content_len : (int)sizeof(buf) - 1);
    if (n <= 0) return ESP_FAIL;
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    cJSON *db   = root ? cJSON_GetObjectItem(root, "db") : NULL;
    int   want  = (db && cJSON_IsNumber(db)) ? db->valueint : 1;  // 1 = invalido
    if (root) cJSON_Delete(root);

    char resp[160];
    if (want > 0 || want < -64) {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"error\",\"message\":\"db 0..-63, o -64 = mute\"}");
    } else {
        int8_t payload = (int8_t)want;
        int ack = uart_send_and_wait_ack(CMD_SET_VOLUME,
                                         (const uint8_t *)&payload, 1, 300);
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"%s\",\"db\":%d}",
                 ack == 1 ? "ok" : (ack == 0 ? "nack" : "timeout"), want);
        printf("[ESP32] POST /volume %d dB -> %s\n", want,
               ack == 1 ? "ACK" : "sin ACK");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// ============== GET /capture — muestras crudas del ADC ==============
//
// Baja las 8 entradas tal como las ve el ADC, para analizarlas en la
// computadora: forma de onda, espectro, fase entre canales, correlacion.
//
// Existe porque hasta ahora medir era mirar numeros por el cable serie. El modo
// 19 hace una medicion completa y devuelve UN pico y UN retardo; todo lo demas
// se calculaba adentro del STM32 y se tiraba. Con las muestras afuera, la placa
// deja de tener que saber de antemano que se le va a preguntar.
//
// Formato: 16 bytes de cabecera (ds_cap_info_t) + int16 intercalados por canal.
// Ver tools/captura.py, que lo convierte en .wav y saca las cuentas.
//
// ⚠️ El pedido de cada tramo NO retiene el lock del UART mientras espera la
// respuesta. Eso es deliberado: retenerlo durante la espera es exactamente el
// abrazo mortal que rompio las primeras cargas de firmware (el lector es el
// unico que puede ver la respuesta, y se quedaba esperando el lock). Cada trama
// se escribe entera bajo el lock, que es lo unico que hace falta para que no se
// entremezclen bytes de dos tramas.

static esp_err_t capture_handler(httpd_req_t *req)
{
    char err[160];

    int n = 2048;                       // ~43 ms, ~3 s de transferencia
    {
        char q[64], v[16];
        if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK &&
            httpd_query_key_value(q, "n", v, sizeof(v)) == ESP_OK) {
            n = atoi(v);
        }
    }
    if (n < 64)   n = 64;
    if (n > 8192) n = 8192;             // el techo del buffer del STM32

    uint8_t p2[2] = { (uint8_t)(n & 0xFF), (uint8_t)(n >> 8) };
    xSemaphoreTake(cap_info_sem, 0);
    uart_send_frame(CMD_CAP_START, p2, 2);

    // La respuesta llega recien cuando la captura se lleno: 8192 muestras a
    // 48 kHz son 170 ms, y el STM32 puede estar sirviendo audio en el medio.
    if (xSemaphoreTake(cap_info_sem, 3000 / portTICK_PERIOD_MS) != pdTRUE) {
        snprintf(err, sizeof(err),
                 "{\"status\":\"error\",\"message\":\"el STM32 no arranco la captura\"}");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, err, strlen(err));
        return ESP_OK;
    }

    ds_cap_info_t info = last_cap_info;
    uint32_t total = info.total_bytes;

    printf("[ESP32] Captura: %u muestras x %u canales, shift %u, pico %ld (%lu bytes)\n",
           info.n_samples, info.n_channels, info.shift,
           (long)info.peak, (unsigned long)total);

    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=captura.ds");
    httpd_resp_send_chunk(req, (const char *)&info, sizeof(info));

    for (uint32_t off = 0; off < total; ) {
        uint8_t r4[4] = { (uint8_t)off, (uint8_t)(off >> 8),
                          (uint8_t)(off >> 16), (uint8_t)(off >> 24) };
        int ok = 0;
        // Hasta tres intentos por tramo: reenviar es inofensivo porque el
        // pedido lleva el offset — el STM32 devuelve siempre lo mismo.
        for (int t = 0; t < 3 && !ok; t++) {
            xSemaphoreTake(cap_data_sem, 0);
            uart_send_frame(CMD_CAP_READ, r4, 4);
            if (xSemaphoreTake(cap_data_sem, 1500 / portTICK_PERIOD_MS) == pdTRUE &&
                last_cap_off == off && last_cap_len > 0) {
                ok = 1;
            }
        }
        if (!ok) {
            printf("[ESP32] Captura CORTADA en el byte %lu\n", (unsigned long)off);
            httpd_resp_send_chunk(req, NULL, 0);   // cierra truncado a proposito
            return ESP_OK;
        }
        httpd_resp_send_chunk(req, (const char *)last_cap_data, last_cap_len);
        off += last_cap_len;
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// ============== GET /fase — el barrido de fase por parlante ==============
//
// Devuelve BINARIO, como /capture y por el mismo motivo: son 32 x 14 x 8 = 3584
// medidas complejas, y en JSON eso son mas de 100 kB que habria que armar en
// RAM. El encabezado va adelante (ds_phase_info_t) y despues los datos crudos;
// tools/fase.py lo parsea.
//
// Si el barrido no termino, contesta JSON con el progreso en vez de datos a
// medio hacer. Es deliberado: un lector que reciba ceros de los parlantes que
// faltan los leeria como "ese parlante no suena", que es la conclusion mas cara
// posible de sacar por error.
static esp_err_t fase_handler(httpd_req_t *req)
{
    char msg[256];

    xSemaphoreTake(phase_info_sem, 0);          // drenar respuesta vieja
    uart_send_frame(CMD_GET_PHASE_INFO, NULL, 0);
    if (xSemaphoreTake(phase_info_sem, 1500 / portTICK_PERIOD_MS) != pdTRUE) {
        int n = snprintf(msg, sizeof(msg),
                 "{\"ok\":false,\"message\":\"el STM32 no contesto el pedido de fase\"}");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, msg, n);
        return ESP_OK;
    }

    ds_phase_info_t info = last_phase_info;
    if (info.state != 2) {
        static const char *st[] = { "sin correr", "corriendo", "listo" };
        int n = snprintf(msg, sizeof(msg),
                 "{\"ok\":false,\"state\":\"%s\",\"progress\":%u,\"n_spk\":%u,"
                 "\"message\":\"el barrido de fase no termino. Entra al modo 22 "
                 "y espera ~3 min.\"}",
                 info.state < 3 ? st[info.state] : "?",
                 (unsigned)info.progress, (unsigned)info.n_spk);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, msg, n);
        return ESP_OK;
    }

    uint32_t total = info.total_bytes;
    printf("[ESP32] Fase: %u parlantes x %u frecuencias x %u entradas (%lu bytes)\n",
           (unsigned)info.n_spk, (unsigned)info.n_pts, (unsigned)info.n_ch,
           (unsigned long)total);

    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=fase.ds");
    httpd_resp_send_chunk(req, (const char *)&info, sizeof(info));

    for (uint32_t off = 0; off < total; ) {
        uint8_t r4[4] = { (uint8_t)off, (uint8_t)(off >> 8),
                          (uint8_t)(off >> 16), (uint8_t)(off >> 24) };
        int ok = 0;
        // Hasta tres intentos: reenviar es inofensivo porque el pedido lleva el
        // offset y el STM32 devuelve siempre lo mismo.
        for (int t = 0; t < 3 && !ok; t++) {
            xSemaphoreTake(phase_data_sem, 0);
            uart_send_frame(CMD_PHASE_READ, r4, 4);
            if (xSemaphoreTake(phase_data_sem, 1500 / portTICK_PERIOD_MS) == pdTRUE &&
                last_phase_off == off && last_phase_len > 0) {
                ok = 1;
            }
        }
        if (!ok) {
            printf("[ESP32] Fase CORTADA en el byte %lu\n", (unsigned long)off);
            httpd_resp_send_chunk(req, NULL, 0);   // cierra truncado a proposito
            return ESP_OK;
        }
        httpd_resp_send_chunk(req, (const char *)last_phase_data, last_phase_len);
        off += last_phase_len;
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// ============== GET /health — autodiagnostico ==============
//
// Le pide al STM32 que vuelva a LEER POR I2C lo que los chips tienen adentro y
// arma un veredicto por cada cosa, con el numero atras.
//
// Por que no alcanza con /status: el status es telemetria pasiva. Un PCM1690 al
// que se le fue el registro de formato deja de decodificar el TDM y saca ruido
// o silencio — y sin embargo underruns sigue en cero, el CPU sigue igual y el
// status entero se ve perfecto. Ningun contador ve eso; hay que ir a preguntar.
//
// Cada chequeo devuelve estado + valor + esperado. Un rojo que no se puede
// discutir es un rojo en el que no se confia.

// 2 = bien, 1 = sospechoso, 0 = mal.
static int hchk(char *o, size_t n, int *first, const char *name, int st,
                const char *val, const char *want)
{
    int k = snprintf(o, n, "%s{\"n\":\"%s\",\"st\":%d,\"v\":\"%s\",\"w\":\"%s\"}",
                     *first ? "" : ",", name, st, val, want);
    *first = 0;
    return k;
}

/* GET /cal — el resultado del ultimo scan del AUTOFOCUS, los 32 canales.
 *
 * NO dispara un scan (para eso esta POST /mode con el modo del AUTOFOCUS).
 * Devuelve lo que haya medido el ultimo, y `state`/`progress` dicen si quedo a
 * mitad de camino.
 *
 * `tof_q8` viene CRUDO, en muestras Q8: 1/256 de muestra = 81 ns = 28 um de
 * camino acustico. Lo que vale del scan no es el numero absoluto sino la
 * diferencia entre parlantes; normalizarlo aca le sacaria informacion al que
 * despues le reste la geometria. La conversion a microsegundos se hace afuera:
 *     us = tof_q8 / 256 / 48000 * 1e6
 */
/** @brief Llena la cache de nombres de modo preguntandole al STM32.
 *
 * Idempotente: si ya esta llena no hace nada. Devuelve true si quedo utilizable.
 *
 * Corta por tres lados —tramo vacio, cobertura completa, o tope del arreglo— y
 * no por "hasta que total sea multiplo del tramo". Un `count == 0` con modos
 * pendientes significa que un nombre no entra en una trama; se corta igual, y la
 * cache queda con lo que se pudo, en vez de pedir el mismo indice para siempre.
 */
static bool modes_ensure(void)
{
    if (modes_count) return true;

    uint16_t used = 0;
    uint8_t  n = 0;
    uint8_t  first = 0;

    for (int guard = 0; guard < MAX_MODES; guard++) {
        xSemaphoreTake(modes_sem, 0);               // drenar respuesta vieja
        uart_send_frame(CMD_GET_MODES, &first, 1);
        if (xSemaphoreTake(modes_sem, 800 / portTICK_PERIOD_MS) != pdTRUE) {
            return false;                            // el STM32 no contesto
        }
        ds_modes_hdr_t h = last_modes_hdr;
        if (h.count == 0) break;

        for (uint8_t i = 0, off = 0; i < h.count && n < MAX_MODES; i++) {
            const char *src = (const char *)&last_modes_buf[off];
            /* Largo acotado al tramo: si el STM32 mandara un nombre sin NUL,
             * strlen se saldria del buffer. */
            size_t len = strnlen(src, (size_t)(h.bytes - off));
            if (off + len >= h.bytes && src[len] != '\0') break;   // truncado
            if (used + len + 1 > sizeof(modes_store)) break;
            memcpy(&modes_store[used], src, len);
            modes_store[used + len] = '\0';
            modes_name[n++] = &modes_store[used];
            used += (uint16_t)(len + 1);
            off  += (uint8_t)(len + 1);
        }

        first = (uint8_t)(h.first + h.count);
        if (first >= h.total || n >= MAX_MODES) break;
    }

    modes_count = n;
    return n > 0;
}

/** GET /modes — la lista, tal como la informa el STM32.
 *
 * Existe para que la pagina web y la app no tengan que llevar su propia copia:
 * era el tercer espejo de ui_states[] y el que mas seguido quedaba viejo. */
static esp_err_t modes_handler(httpd_req_t *req)
{
    char buf[2048];
    int  p = 0;
    httpd_resp_set_type(req, "application/json");

    if (!modes_ensure()) {
        p = snprintf(buf, sizeof(buf),
                     "{\"ok\":false,\"message\":\"el STM32 no contesto la lista de modos\"}");
        httpd_resp_send(req, buf, p);
        return ESP_OK;
    }

    p += snprintf(buf + p, sizeof(buf) - p, "{\"ok\":true,\"modes\":[");
    for (int i = 0; i < modes_count; i++) {
        p += snprintf(buf + p, sizeof(buf) - p, "%s\"%s\"", i ? "," : "",
                      mode_name_of(i));
        if (p >= (int)sizeof(buf) - 8) break;
    }
    p += snprintf(buf + p, sizeof(buf) - p, "]}");
    httpd_resp_send(req, buf, p);
    return ESP_OK;
}

static esp_err_t cal_handler(httpd_req_t *req)
{
    /* 64 entradas (32 parlantes x 2 microfonos) de ~50 caracteres = ~3,2 kB,
     * mas la cola. Con 3600 entraba justo y snprintf habria TRUNCADO el JSON en
     * silencio: la pagina veria un objeto sin cerrar y no una lista incompleta.
     * 4608 deja mas de un kilobyte de aire. */
    char buf[4608];
    int  p = 0;
    httpd_resp_set_type(req, "application/json");

    ds_cal_hdr_t hdr0;
    bool have_hdr = false;
    int  total = 0;

    p += snprintf(buf + p, sizeof(buf) - p, "{\"ok\":true,\"channels\":[");

    /* 2 microfonos x 32 parlantes. El tope del lazo es un techo de seguridad;
     * quien manda es h.n_channels, que llega en cada trama. */
    const int CAL_NUM_MIC = 2;
    for (int first = 0; first < 32 * CAL_NUM_MIC; first += CAL_CHUNK_CH) {
        uint8_t r1 = (uint8_t)first;
        xSemaphoreTake(cal_sem, 0);                 // drenar respuesta vieja
        uart_send_frame(CMD_GET_CAL, &r1, 1);

        if (xSemaphoreTake(cal_sem, 1500 / portTICK_PERIOD_MS) != pdTRUE) {
            p = snprintf(buf, sizeof(buf),
                         "{\"ok\":false,\"message\":\"el STM32 no contesto el pedido "
                         "de calibracion (canal %d)\"}", first);
            httpd_resp_send(req, buf, p);
            return ESP_OK;
        }

        ds_cal_hdr_t h = last_cal_hdr;
        if (!have_hdr) { hdr0 = h; have_hdr = true; }

        for (int i = 0; i < h.count; i++) {
            const ds_cal_ch_t *c = &last_cal_ch[i];
            p += snprintf(buf + p, sizeof(buf) - p,
                          "%s{\"ch\":%d,\"mic\":%d,\"tof_q8\":%ld,"
                          "\"sign\":%d,\"quality\":%u}",
                          total ? "," : "",
                          (h.first + i) % (h.n_channels / CAL_NUM_MIC),
                          (h.first + i) / (h.n_channels / CAL_NUM_MIC) + 1,
                          (long)c->tof_q8, (int)c->sign, (unsigned)c->quality);
            total++;
        }

        /* Se termino cuando este tramo cubrio hasta el ultimo canal, o cuando
         * vino vacio (pedido fuera de rango). Sin esto el lazo confiaria en que
         * n_channels sea multiplo de CAL_CHUNK_CH. */
        if (h.count == 0 || (h.first + h.count) >= h.n_channels) break;
    }

    static const char *st_name[] = { "idle", "settle", "arm", "capt", "corr", "done" };
    uint8_t st = have_hdr ? hdr0.state : 0;
    p += snprintf(buf + p, sizeof(buf) - p,
                  "],\"n_channels\":%u,\"mic1\":%u,\"mic2\":%u,"
                  "\"state\":\"%s\",\"progress\":%u,"
                  "\"ok_count\":%u,\"q8_per_sample\":256,\"rate_hz\":48000}",
                  have_hdr ? hdr0.n_channels : 0,
                  have_hdr ? hdr0.mic1 : 0, have_hdr ? hdr0.mic2 : 1,
                  st < (sizeof(st_name) / sizeof(st_name[0])) ? st_name[st] : "?",
                  have_hdr ? hdr0.progress : 0,
                  have_hdr ? hdr0.ok_count : 0);

    httpd_resp_send(req, buf, p);
    return ESP_OK;
}

static esp_err_t health_handler(httpd_req_t *req)
{
    xSemaphoreTake(health_sem, 0);              // drenar respuesta vieja
    uart_send_frame(CMD_SELFTEST, NULL, 0);

    char buf[1400];
    int  p = 0;

    if (xSemaphoreTake(health_sem, 3000 / portTICK_PERIOD_MS) != pdTRUE) {
        snprintf(buf, sizeof(buf),
                 "{\"ok\":false,\"message\":\"el STM32 no contesto el autodiagnostico\"}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, buf, strlen(buf));
        return ESP_OK;
    }

    const ds_health_t *h = &last_health;
    int first = 1, worst = 2;
    char v[80], w[48];

    p += snprintf(buf + p, sizeof(buf) - p, "{\"ok\":true,\"checks\":[");

    // --- los cuatro PCM1690 ---
    for (int d = 0; d < 4; d++) {
        char name[16];
        snprintf(name, sizeof(name), "DAC%d", d);
        int present = (h->dac_present >> d) & 1;
        int st;
        if (!present) {
            st = 0;
            snprintf(v, sizeof(v), "no contesta por I2C");
        } else if (h->dac_fmt[d] != 0x06) {
            /* El que importa de verdad: sin FORMAT=0x06 el chip no decodifica
             * el TDM8 y saca ruido, con toda la telemetria en verde. */
            st = 0;
            snprintf(v, sizeof(v), "FORMAT=0x%02X (deberia ser 0x06)", h->dac_fmt[d]);
        } else {
            st = 2;
            int db10 = -((255 - h->dac_att[d]) * 10) / 2;
            snprintf(v, sizeof(v), "FORMAT ok, atenuacion %d.%d dB",
                     db10 / 10, abs(db10 % 10));
        }
        if (st < worst) worst = st;
        p += hchk(buf + p, sizeof(buf) - p, &first, name, st, v, "FORMAT 0x06");
    }

    // --- AK5538VN ---
    {
        int st;
        if (!h->ak_present) { st = 0; snprintf(v, sizeof(v), "no contesta por I2C3"); }
        else if (h->ak_regs[1] != 0x31 || h->ak_regs[2] != 0x40 || h->ak_regs[3] != 0x02) {
            st = 1;
            snprintf(v, sizeof(v), "R02=0x%02X R03=0x%02X R04=0x%02X",
                     h->ak_regs[1], h->ak_regs[2], h->ak_regs[3]);
        } else {
            st = 2;
            snprintf(v, sizeof(v), "TDM256 y group delay corto confirmados");
        }
        if (st < worst) worst = st;
        p += hchk(buf + p, sizeof(buf) - p, &first, "ADC AK5538VN", st, v,
                  "R02=0x31 R03=0x40 R04=0x02");
    }

    // --- EEPROM ---
    {
        int st = h->eeprom_present ? 2 : 1;
        snprintf(v, sizeof(v), h->eeprom_present
                 ? "presente, layout v%d" : "no responde (la config no sobrevive al reinicio)",
                 h->eeprom_ver);
        if (st < worst) worst = st;
        p += hchk(buf + p, sizeof(buf) - p, &first, "EEPROM", st, v, "presente");
    }

    // --- SYNC entre bancos ---
    {
        int a = h->sync_us < 0 ? -h->sync_us : h->sync_us;
        /* Se midieron 1-4 us en operacion normal. Decenas ya es raro;
         * milisegundos significa que las filas 0-1 y 2-3 estan tocando ventanas
         * de tiempo distintas y el enfoque vertical directamente no existe. */
        int st = (a <= 20) ? 2 : ((a <= 200) ? 1 : 0);
        snprintf(v, sizeof(v), "%d us", h->sync_us);
        if (st < worst) worst = st;
        p += hchk(buf + p, sizeof(buf) - p, &first, "SYNC entre bancos", st, v, "pocos us");
    }

    (void)w;
    p += snprintf(buf + p, sizeof(buf) - p, "],\"worst\":%d}", worst);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

// ============== POST /firmware — actualizacion del STM32 por WiFi ==============
//
// El cuerpo del POST es el .bin crudo (build/ds11_beamforming.bin), tal cual.
// Este chip NO lo guarda en ningun lado: lo va reenviando al STM32 a medida que
// lo recibe. No hace falta particion de almacenamiento, no hace falta que entre
// en RAM, y —lo importante— una transferencia cortada a la mitad no deja nada
// que limpiar: el STM32 escribe en un banco de flash que no ejecuta, y sin la
// trama final de cierre esa imagen es invisible para su bootloader.
//
// Ver firmware/Core/Inc/ota.h para por que eso es seguro.

// Mismo CRC-32 (zlib) que ota.c del STM32, reimplementado a proposito en vez de
// usar el de la ROM del ESP32: las variantes de la ROM difieren en si el valor
// entra y sale invertido, y un desacuerdo silencioso aca haria fallar TODAS las
// cargas con "el CRC no coincide" y ninguna pista de por que.
static const uint32_t CRC32_TAB[16] = {
    0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
    0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
    0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
    0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL
};

static uint32_t crc32_up(uint32_t crc, const uint8_t *p, int len)
{
    while (len--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ CRC32_TAB[crc & 0x0F];
        crc = (crc >> 4) ^ CRC32_TAB[crc & 0x0F];
    }
    return crc;
}

// Techo: el hueco de la aplicacion en el banco 1 del STM32 (OTA_APP_SIZE).
#define FW_MAX_BYTES (896 * 1024)

// Tiempos de espera, muy distintos entre si porque el trabajo del otro lado lo
// es: FW_BEGIN borra sectores de flash, FW_DATA escribe 126 bytes, FW_END relee
// la imagen entera para el CRC.
#define FW_TO_BEGIN  20000
#define FW_TO_DATA    3000
#define FW_TO_END    20000

static esp_err_t firmware_handler(httpd_req_t *req)
{
    char resp[224];
    int  total = req->content_len;

    if (total <= 0 || total > FW_MAX_BYTES) {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"error\",\"message\":\"tamano invalido (%d bytes)\"}", total);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }

    // id y build son informativos: viajan para que quede registrado que binario
    // se subio. El STM32 igual reporta por /status el fw_id que trae compilado
    // adentro, que es la fuente de verdad de "que esta corriendo".
    uint32_t id = 0, build = 0;
    {
        char q[96], v[32];
        if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
            if (httpd_query_key_value(q, "id", v, sizeof(v)) == ESP_OK)
                id = (uint32_t)strtoul(v, NULL, 16);
            if (httpd_query_key_value(q, "build", v, sizeof(v)) == ESP_OK)
                build = (uint32_t)strtoul(v, NULL, 10);
        }
    }

    printf("[ESP32] OTA STM32: %d bytes, id %08lX\n", total, (unsigned long)id);

    fw_begin_t beg = { .len = (uint32_t)total, .fw_id = id, .build = build };
    int ack = uart_send_and_wait_ack(CMD_FW_BEGIN, (const uint8_t *)&beg,
                                     sizeof(beg), FW_TO_BEGIN);
    if (ack != 1) {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"error\",\"message\":\"el STM32 no acepto la carga (%s)\"}",
                 ack == 0 ? "rechazada" : "sin respuesta");
        goto fail;
    }

    uint32_t crc = 0xFFFFFFFFUL;
    uint16_t seq = 0;
    int      left = total;
    uint8_t  chunk[2 + FW_CHUNK_MAX];

    while (left > 0) {
        int want = (left < FW_CHUNK_MAX) ? left : FW_CHUNK_MAX;
        int got  = 0;
        while (got < want) {
            int r = httpd_req_recv(req, (char *)&chunk[2 + got], want - got);
            if (r <= 0) {
                // El navegador corto. Se avisa para que el STM32 cierre la
                // sesion; si el aviso tampoco llega, no importa: sin la trama
                // final la imagen a medias no la aplica nadie.
                uart_send_and_wait_ack(CMD_FW_ABORT, NULL, 0, 500);
                snprintf(resp, sizeof(resp),
                         "{\"status\":\"error\",\"message\":\"se corto la subida en el byte %d\"}",
                         total - left + got);
                goto fail;
            }
            got += r;
        }

        crc = crc32_up(crc, &chunk[2], got);
        chunk[0] = (uint8_t)(seq & 0xFF);
        chunk[1] = (uint8_t)(seq >> 8);

        // Hasta tres intentos por tramo. Reenviar es inofensivo: el STM32
        // reconoce el numero de tramo anterior como repetido y lo vuelve a
        // ACKear sin escribirlo dos veces.
        int a = -1;
        for (int t = 0; t < 3 && a != 1; t++)
            a = uart_send_and_wait_ack(CMD_FW_DATA, chunk, (uint16_t)(2 + got), FW_TO_DATA);

        if (a != 1) {
            uart_send_and_wait_ack(CMD_FW_ABORT, NULL, 0, 500);
            snprintf(resp, sizeof(resp),
                     "{\"status\":\"error\",\"message\":\"el STM32 dejo de contestar en el tramo %u\"}",
                     (unsigned)seq);
            goto fail;
        }
        seq++;
        left -= got;
    }

    crc ^= 0xFFFFFFFFUL;
    uint8_t endp[4] = { (uint8_t)crc, (uint8_t)(crc >> 8),
                        (uint8_t)(crc >> 16), (uint8_t)(crc >> 24) };
    ack = uart_send_and_wait_ack(CMD_FW_END, endp, 4, FW_TO_END);

    if (ack != 1) {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"error\",\"message\":\"la imagen no verifico en el STM32 (%s)\"}",
                 ack == 0 ? "CRC distinto" : "sin respuesta");
        goto fail;
    }

    printf("[ESP32] OTA STM32: %d bytes ok, crc %08lX\n", total, (unsigned long)crc);
    snprintf(resp, sizeof(resp),
             "{\"status\":\"ok\",\"bytes\":%d,\"crc\":\"%08lX\","
             "\"message\":\"imagen verificada; el STM32 se reinicia para aplicarla\"}",
             total, (unsigned long)crc);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;

fail:
    printf("[ESP32] OTA STM32 FALLO: %s\n", resp);
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// ============== Confirmacion de arranque tras un OTA propio ==============
//
// Con la vuelta atras habilitada, una imagen recien instalada arranca en estado
// "a prueba" (ESP_OTA_IMG_PENDING_VERIFY) y tiene que declararse buena. Si no lo
// hace, el bootloader la descarta y vuelve a la anterior.
//
// El criterio de "buena" es el unico que importa para este chip: **se conecto a
// la red**. Un firmware que compila, arranca y no encuentra el WiFi es
// exactamente el que te deja sin equipo, y es el que hay que deshacer.
//
// Las dos ramas son deliberadas:
//   - se conecto        → se confirma y a partir de ahi la imagen es la buena;
//   - no se conecto en 90 s → se marca invalida y se reinicia A LA ANTERIOR, sin
//     esperar a que alguien vaya a apretar un boton.
//
// Si no hay nada pendiente de confirmar (arranque normal, o flasheo por cable),
// esto no hace absolutamente nada y la tarea se muere sola.
static void ota_confirm_task(void *arg)
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;

    if (!run || esp_ota_get_state_partition(run, &st) != ESP_OK ||
        st != ESP_OTA_IMG_PENDING_VERIFY) {
        vTaskDelete(NULL);
        return;
    }

    printf("[ESP32] Imagen A PRUEBA en %s: esperando conexion para confirmarla\n",
           run->label);

    /* Cinturon: si alguien vuelve a mover esta tarea antes de wifi_init(),
     * esperar en vez de entrar en panico sobre un handle NULL. */
    for (int i = 0; i < 100 && !wifi_event_group; i++)
        vTaskDelay(100 / portTICK_PERIOD_MS);
    if (!wifi_event_group) {
        printf("[ESP32] No hay grupo de eventos de WiFi: no se puede confirmar\n");
        vTaskDelete(NULL);
        return;
    }

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdFALSE,
                                           90000 / portTICK_PERIOD_MS);

    if (bits & WIFI_CONNECTED_BIT) {
        esp_ota_mark_app_valid_cancel_rollback();
        printf("[ESP32] Imagen confirmada: %s queda como la buena\n", run->label);
    } else {
        printf("[ESP32] La imagen nueva NO se conecto en 90 s. "
               "Volviendo a la anterior.\n");
        esp_ota_mark_app_invalid_rollback_and_reboot();   // no vuelve
    }
    vTaskDelete(NULL);
}

// ============== POST /firmware-esp — actualizar ESTE chip por WiFi ==============
//
// Mucho mas simple que el del STM32 porque ESP-IDF ya trae todo: el bootloader
// de segunda etapa elige entre dos ranuras de aplicacion (ota_0 / ota_1) segun
// lo que diga la particion `otadata`, y esp_ota_* escribe la ranura que NO se
// esta ejecutando. No hay que escribir un bootloader ni tocar nada delicado.
//
// Y es mas SEGURO que el del STM32, que es lo que hace que valga la pena:
// con CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE la imagen nueva arranca "a prueba"
// y tiene que confirmar que anda (ota_confirm_task, abajo). Si no confirma, el
// bootloader vuelve sola a la anterior. Actualizar el chip que TIENE LA RED
// deja de ser un acto de fe: si la imagen nueva no se conecta al WiFi, se
// deshace sola y volves a tener el equipo.

static esp_err_t firmware_esp_handler(httpd_req_t *req)
{
    char resp[224];
    int  total = req->content_len;

    const esp_partition_t *upd = esp_ota_get_next_update_partition(NULL);
    if (!upd) {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"error\",\"message\":\"no hay segunda ranura: "
                 "la tabla de particiones es la vieja, hace falta un flasheo por cable\"}");
        goto fail;
    }
    if (total <= 0 || (uint32_t)total > upd->size) {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"error\",\"message\":\"tamano invalido (%d bytes, la ranura son %lu)\"}",
                 total, (unsigned long)upd->size);
        goto fail;
    }

    printf("[ESP32] OTA propio: %d bytes -> particion %s @ 0x%lx\n",
           total, upd->label, (unsigned long)upd->address);

    esp_ota_handle_t h = 0;
    esp_err_t e = esp_ota_begin(upd, total, &h);
    if (e != ESP_OK) {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"error\",\"message\":\"no se pudo abrir la ranura: %s\"}",
                 esp_err_to_name(e));
        goto fail;
    }

    char buf[1024];
    int left = total;
    while (left > 0) {
        int want = (left < (int)sizeof(buf)) ? left : (int)sizeof(buf);
        int got  = httpd_req_recv(req, buf, want);
        if (got <= 0) {
            esp_ota_abort(h);
            snprintf(resp, sizeof(resp),
                     "{\"status\":\"error\",\"message\":\"se corto la subida en el byte %d\"}",
                     total - left);
            goto fail;
        }
        e = esp_ota_write(h, buf, got);
        if (e != ESP_OK) {
            esp_ota_abort(h);
            snprintf(resp, sizeof(resp),
                     "{\"status\":\"error\",\"message\":\"fallo la escritura: %s\"}",
                     esp_err_to_name(e));
            goto fail;
        }
        left -= got;
    }

    // esp_ota_end verifica la firma y la cabecera de la imagen. Si el archivo no
    // es un firmware de ESP32-C3, muere aca — antes de tocar otadata, o sea sin
    // consecuencias: la ranura que se esta ejecutando no se toco.
    e = esp_ota_end(h);
    if (e != ESP_OK) {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"error\",\"message\":\"la imagen no valida: %s\"}",
                 esp_err_to_name(e));
        goto fail;
    }

    e = esp_ota_set_boot_partition(upd);
    if (e != ESP_OK) {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"error\",\"message\":\"no se pudo marcar el arranque: %s\"}",
                 esp_err_to_name(e));
        goto fail;
    }

    printf("[ESP32] OTA propio: listo, reiniciando en %s\n", upd->label);
    snprintf(resp, sizeof(resp),
             "{\"status\":\"ok\",\"bytes\":%d,\"slot\":\"%s\","
             "\"message\":\"imagen verificada; el ESP32 se reinicia (si no arranca bien vuelve sola a la anterior)\"}",
             total, upd->label);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));

    // Un respiro para que la respuesta llegue al navegador antes de irse.
    vTaskDelay(1200 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;

fail:
    printf("[ESP32] OTA propio FALLO: %s\n", resp);
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// ============== UI web embebida (GET /) ==============
// Pagina autocontenida (sin CDN): botones de modo + sliders de beam + telemetria.
static const char INDEX_HTML[] =
"<!DOCTYPE html><html lang=es><head><meta charset=utf-8>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>DS Beamformer</title><style>"
"body{font-family:system-ui,sans-serif;background:#111;color:#eee;margin:0;padding:16px;max-width:640px;margin-inline:auto}"
"h1{font-size:1.3em}h2{font-size:1em;color:#9cf;margin:18px 0 8px}"
".grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}"
"button{background:#224;border:1px solid #446;color:#eee;border-radius:8px;padding:10px 8px;font-size:.9em;cursor:pointer}"
"button:active{background:#33a}button.on{background:#2a6;border-color:#4c8}"
".card{background:#1a1a2a;border-radius:10px;padding:12px;margin:10px 0}"
"label{display:block;margin:8px 0 2px;font-size:.85em;color:#aaa}"
"input[type=range]{width:100%}"
".val{color:#9cf;font-weight:600}"
".meter{display:flex;gap:3px;height:36px;align-items:flex-end;margin-top:6px}"
".bar{flex:1;background:#2a6;min-height:2px;border-radius:2px 2px 0 0}"
"#st{font-size:.8em;color:#888;white-space:pre-wrap}"
/* Checklist del AUTOFOCUS: un punto por fila, rojo por defecto. El default es
 * rojo a proposito — "todavia no se comprobo" y "esta mal" se atienden igual, y
 * un verde por omision seria una mentira que cuesta 8 segundos de scan. */
".chk{list-style:none;padding:0;margin:6px 0;font-size:.85em}"
".chk li{padding:4px 0 4px 22px;position:relative;color:#bbb}"
".chk li:before{content:'';position:absolute;left:4px;top:9px;width:9px;"
"height:9px;border-radius:50%;background:#a33}"
".chk li.ok:before{background:#2a6}.chk li.warn:before{background:#c82}"
".row{display:flex;gap:8px;align-items:center}.row button{flex:0 0 auto}"
"</style></head><body>"
"<h1>DS Beamformer <span id=link style='font-size:.6em;color:#888'>...</span></h1>"
"<div id=custom style='color:#f84;font-size:.85em;margin-bottom:10px'></div>"
"<div class=card><h2>Volumen maestro</h2>"
"<div class=row><input type=range id=vol min=-63 max=0 value=0 step=1>"
"<span class=val id=volv>0 dB</span></div>"
"<div class=row style='margin-top:6px'><button onclick=setVol(-64)>Mute</button>"
"<button onclick=setVol(+vol.value)>Aplicar</button></div></div>"
"<div class=card><h2>Ganancia de entrada</h2>"
"<div class=row><input type=range id=ig min=0 max=36 value=0 step=1>"
"<span class=val id=igv>0 dB</span></div>"
"<div class=row style='margin-top:6px'><button onclick=setIg(0)>Linea (0)</button>"
"<button onclick=setIg(30)>Microfono (30)</button>"
"<span class=val id=clipv></span></div>"
"<div style='font-size:.75em;color:#888;margin-top:6px'>Si distorsiona y bajar el "
"volumen no lo arregla, es esto. Se guarda solo.</div></div>"
"<div class=card><h2>Amplificadores</h2>"
"<div class=row><button onclick=setAmps(1)>Encender amps</button>"
"<button onclick=setAmps(0)>Apagar amps</button>"
"<span class=val id=ampsv>...</span></div>"
/* El arranque autonomo va PEGADO a los amps porque es lo mismo visto en el
 * tiempo: quien decide encenderlos es quien tiene que decidir si se encienden
 * solos la proxima vez. */
"<div class=row><label style='font-size:.85em'>"
"<input type=checkbox id=autos onchange=setAuto(this.checked)> "
"Encender solos al arrancar</label>"
"<span class=val id=autosv></span></div>"
"<div style='font-size:.75em;color:#888;margin-top:4px'>Para instalaci&oacute;n: el "
"nodo enciende sus amplificadores <b>sin red y sin que nadie mande nada</b>, 15 s "
"despu&eacute;s de arrancar. Si el equipo se reinicia tres veces seguidas sin "
"estabilizarse, deja de intentar y avisa &mdash; para no rebotar toda la noche si "
"lo que falla es la alimentaci&oacute;n.</div>"
"<div style='font-size:.75em;color:#888;margin-top:6px'>Arrancan apagados. "
"El estado que se muestra viene de la placa, no de esta p&aacute;gina.</div></div>"
"<div class=card><h2>Modo</h2><div class=grid id=modes></div></div>"
"<div class=card><h2>RX beam (az&deg;)</h2>"
"<div class=row><input type=range id=az min=-90 max=90 value=0 step=5>"
"<span class=val id=azv>0&deg;</span><button onclick=setRx()>Aplicar</button></div>"
"<div style='font-size:.75em;color:#888;margin-top:6px'>Aplicar deja el equipo "
"escuchando los micr&oacute;fonos y <b>apaga el tono interno</b>, aunque arriba "
"siga marcado otro modo. Para volver, eleg&iacute; el modo de nuevo.</div></div>"
"<div class=card><h2>TX focus (mm rel centro)</h2>"
"<label>X (izq&minus;/der+): <span class=val id=fxv>0</span></label>"
"<input type=range id=fx min=-1500 max=1500 value=0 step=50>"
"<label>Y (fila1&minus;/fila4+): <span class=val id=fyv>0</span></label>"
"<input type=range id=fy min=-1000 max=1000 value=0 step=50>"
"<label>Z (distancia): <span class=val id=fzv>1000</span></label>"
"<input type=range id=fz min=200 max=4000 value=1000 step=50>"
"<div class=row style='margin-top:8px'><button onclick=setTx()>Aplicar foco TX</button></div></div>"
/* MULTIFOCUS: el modo 15 es el objetivo artistico del instrumento —8 entradas,
 * cada una colocada en un punto del espacio— y hasta ahora solo se manejaba con
 * un curl por fuente. Vista en planta: X horizontal, Z (distancia al array) en
 * vertical, el array abajo. Se arrastran los puntos. */
"<div class=card><h2>MULTIFOCUS &mdash; d&oacute;nde suena cada entrada</h2>"
"<svg id=pad viewBox='0 0 300 215' "
/* height:auto (no 215px fijos) para que la caja tome el aspecto del viewBox: el
 * dibujo pasa a ocupar TODO el ancho en vez de 300 px centrados, que es el
 * doble de superficie para agarrar los puntos. */
"style='width:100%;height:auto;background:#111;border-radius:6px;touch-action:none'>"
"<rect x=122 y=203 width=56 height=7 fill=#2a6 rx=2></rect>"
"<text x=150 y=199 fill=#666 font-size=7 text-anchor=middle>array</text>"
"<line x1=150 y1=12 x2=150 y2=203 stroke=#222 stroke-width=1></line>"
"<text x=292 y=200 fill=#444 font-size=7 text-anchor=end>+1.5 m</text>"
"<text x=8 y=200 fill=#444 font-size=7>&minus;1.5 m</text>"
"<text x=154 y=18 fill=#444 font-size=7>3 m</text></svg>"
"<div class=row style='margin-top:6px'>"
"<label style='flex:0 0 auto'>altura fuente <span class=val id=selv>1</span>:</label>"
"<input type=range id=fyv2 min=-800 max=800 value=0 step=25>"
"<span class=val id=fyvv>0</span></div>"
"<div style='font-size:.75em;color:#888;margin-top:6px'>Arrastr&aacute; cada punto. "
"Aplica al modo MULTIFOCUS activo (14 tonos / 15 entradas).</div></div>"
/* AUTOFOCUS: el modo mide con un microfono y hasta ahora solo se contaba por el
 * cable serie. Sin terminal no habia forma de saber si el microfono estaba
 * vivo, por que parlante iba el scan ni si el resultado servia — y el scan
 * terminaba diciendo "done" igual. Estas cinco filas son las cinco cosas que
 * pueden salir mal, en el orden en que hay que resolverlas. */
"<div class=card><h2>AUTOFOCUS &mdash; calibrar con micr&oacute;fono</h2>"
"<ol class=chk>"
"<li id=k1>amplificadores</li>"
"<li id=k2>micr&oacute;fono en la entrada 1</li>"
"<li id=k3>nivel del micr&oacute;fono</li>"
"<li id=k4>scan</li>"
"<li id=k5>resultado</li>"
"<li id=k6>calibraci&oacute;n guardada</li></ol>"
"<div class=row><button onclick=startAF()>Iniciar scan (modo 11)</button></div>"
"<div style='font-size:.75em;color:#888;margin-top:6px'>El micr&oacute;fono va "
"<b>en el punto donde quer&eacute;s el foco</b>, enchufado a la entrada 1 (c0), "
"y no se mueve durante los ~8 s del barrido. J9 no da alimentaci&oacute;n: "
"hace falta un preamplificador. Al terminar, el equipo reproduce un tono "
"enfocado ah&iacute;. Si el resultado sale en rojo, los focos anteriores "
"quedan intactos.</div></div>"
/* Autodiagnostico. Va arriba de las entradas porque es lo primero que uno
 * quiere saber al llegar: si algo esta mal, el resto de los numeros mienten. */
"<div class=card><h2>Diagn&oacute;stico</h2>"
"<div class=row><button onclick=doHealth()>Revisar el equipo</button>"
"<span class=val id=hvsum></span></div>"
"<ol class=chk id=hlist style='margin-top:10px'></ol>"
"<div style='font-size:.75em;color:#888;margin-top:6px'>Vuelve a leer por I2C lo "
"que los chips <b>realmente</b> tienen adentro &mdash; no es la telemetr&iacute;a "
"de siempre. Un DAC que perdi&oacute; su configuraci&oacute;n saca ruido con "
"<i>underruns</i> en cero y todo lo dem&aacute;s en verde: eso no lo ve ning&uacute;n "
"contador, hay que ir a preguntarle al chip.</div></div>"
"<div class=card><h2>Entradas ADC (peak)</h2><div class=meter id=meter></div>"
"<div id=st>esperando estado...</div></div>"
"<div class=card><h2>Volver a cero</h2>"
"<div class=row><button onclick=doReset()>Reset de f&aacute;brica</button>"
"<span class=val id=rstv></span></div>"
"<div style='font-size:.75em;color:#888;margin-top:6px'>Volumen &minus;24 dB, "
"ganancia 0 dB, modo 0 y focos por defecto. No toca los amplificadores.</div></div>"
/* Actualizacion del firmware del STM32 sin cable. Va al final de la pagina a
 * proposito: no es algo que uno haga durante una medicion. */
"<div class=card><h2>Firmware</h2>"
/* El aviso de desajuste va PRIMERO y ocupa toda la fila: cuando aparece, es lo
 * unico que importa de esta tarjeta. Dice cual actualizar y en que orden. */
"<div id=fwsync style='display:none;font-size:.85em;padding:8px;border-radius:6px;"
"margin-bottom:10px;background:#3a2a10;border:1px solid #a70'></div>"
"<div class=row style='font-size:.85em'><b style='width:60px'>STM32</b>"
"<span class=val id=fwid>&mdash;</span>"
"<span class=val id=fwdate style='color:#888'></span></div>"
"<div class=row><input type=file id=fwfile accept='.bin'>"
"<button onclick=doFw()>Actualizar STM32</button></div>"
"<div id=fwbarw style='display:none;background:#223;border-radius:4px;height:6px;margin-top:8px'>"
"<div id=fwbar style='background:#4c8;height:6px;width:0%;border-radius:4px'></div></div>"
"<div id=fwmsg style='font-size:.8em;margin-top:6px;color:#888'></div>"
"<div style='font-size:.75em;color:#888;margin:6px 0 14px'>"
"<b>firmware/build/ds11_beamforming.bin</b> &mdash; durante la carga (~11 s) el "
"equipo queda <b>mudo</b> y despu&eacute;s se reinicia con los amplificadores "
"apagados. Se escribe en un banco de flash que no se est&aacute; ejecutando: si "
"algo falla a mitad de camino no pasa nada.</div>"
"<div class=row style='font-size:.85em'><b style='width:60px'>ESP32</b>"
"<span class=val id=efwid>&mdash;</span>"
"<span class=val id=efwdate style='color:#888'></span></div>"
"<div class=row><input type=file id=efwfile accept='.bin'>"
"<button onclick=doFwEsp()>Actualizar ESP32</button></div>"
"<div id=efwbarw style='display:none;background:#223;border-radius:4px;height:6px;margin-top:8px'>"
"<div id=efwbar style='background:#4c8;height:6px;width:0%;border-radius:4px'></div></div>"
"<div id=efwmsg style='font-size:.8em;margin-top:6px;color:#888'></div>"
"<div style='font-size:.75em;color:#888;margin-top:6px'>"
"<b>firmware_esp32c3/build/ds11_beamform_c3.bin</b> &mdash; se escribe en la "
"ranura que no se est&aacute; usando y el chip se reinicia. <b>Si la imagen "
"nueva no logra conectarse al WiFi en 90 s, vuelve sola a la anterior.</b> "
"Perd&eacute;s la p&aacute;gina unos 15 s mientras rearranca.</div></div>"
"<script>"
/* Los botones se construyen con la lista que da GET /modes, que sale del STM32.
 *
 * Aca habia un tercer espejo de ui_states[] escrito a mano —`const N=22` y un
 * names[] literal— y era el que mas seguido quedaba viejo: el modo 17 estuvo sin
 * boton hasta que alguien lo noto, y despues el 20 y el 21 con N=20. Agregar un
 * modo tocaba TRES lugares; ahora toca uno.
 *
 * Si el pedido falla no se dibuja nada y se dice por que, en vez de dejar una
 * fila de botones vacia que parece un problema de la placa. */
"const md=document.getElementById('modes');"
"fetch('/modes').then(r=>r.json()).then(j=>{"
"if(!j.ok||!j.modes){md.textContent='No pude leer la lista de modos del STM32.';return;}"
/* El numero adelante: es lo que se usa para referenciar el modo en los comandos
 * (POST /mode {"mode":N}), en los logs y al hablar del equipo. */
"j.modes.forEach((nm,i)=>{const b=document.createElement('button');"
"b.innerHTML='<b style=opacity:.55>'+i+'</b> '+nm;b.id='m'+i;"
"b.onclick=()=>fetch('/mode',{method:'POST',body:JSON.stringify({mode:i})}).then(poll);"
"md.appendChild(b);});})"
".catch(()=>{md.textContent='No pude leer la lista de modos.';});"
"const mt=document.getElementById('meter');"
"for(let i=0;i<8;i++){const d=document.createElement('div');d.className='bar';d.id='b'+i;mt.appendChild(d);}"
"['az','fx','fy','fz'].forEach(id=>{const e=document.getElementById(id);"
"e.oninput=()=>document.getElementById(id+'v').textContent=e.value+(id=='az'?'\\u00b0':'');});"
"function setVol(d){fetch('/volume',{method:'POST',body:JSON.stringify({db:d})});}"
"function setAuto(o){fetch('/autostart',{method:'POST',body:JSON.stringify({on:!!o})})"
".then(r=>r.json()).then(s=>{autosv.textContent=s.status=='ok'?'':s.status;});}"
"function setAmps(o){fetch('/amps',{method:'POST',body:JSON.stringify({on:!!o})})"
".then(r=>r.json()).then(s=>{ampsv.textContent=s.status=='ok'?(o?'ON':'OFF'):s.status;});}"
"let volT=0,volTouch=0;"
"vol.oninput=()=>{volv.textContent=vol.value+' dB';volTouch=Date.now();"
"if(Date.now()-volT>200){volT=Date.now();setVol(+vol.value);}};"
"vol.onchange=()=>{volTouch=Date.now();setVol(+vol.value);};"
"let igTouch=0,lastClip=null;"
"function setIg(d){ig.value=d;igv.textContent=d+' dB';igTouch=Date.now();"
"fetch('/ingain',{method:'POST',body:JSON.stringify({db:d})});}"
"ig.oninput=()=>{igv.textContent=ig.value+' dB';igTouch=Date.now();};"
"ig.onchange=()=>setIg(+ig.value);"
/* ===== Pad MULTIFOCUS =====
 * Estado en mm; el SVG es solo la proyeccion. Los defaults espejan
 * MFocusIn_ApplyDefaults() del STM32: X de -700 a +700 cada 200, y=0, z=1000. */
"const SX=1500,SZ=3000,pad=document.getElementById('pad');"
"const src=[];for(let i=0;i<8;i++)src.push({x:-700+200*i,y:0,z:1000});"
"let sel=0,drag=-1,padT=0,curMode=-1;"
"const MX=x=>150+x*140/SX,MZ=z=>203-z*191/SZ;"
"const IX=p=>Math.max(-SX,Math.min(SX,Math.round((p-150)*SX/140)));"
"const IZ=p=>Math.max(100,Math.min(SZ,Math.round((203-p)*SZ/191)));"
"const dots=[];"
"for(let i=0;i<8;i++){"
"const g=document.createElementNS('http://www.w3.org/2000/svg','g');"
"const c=document.createElementNS('http://www.w3.org/2000/svg','circle');"
"c.setAttribute('r',8);c.setAttribute('fill','#9cf');c.setAttribute('opacity',.85);"
"const t=document.createElementNS('http://www.w3.org/2000/svg','text');"
"t.setAttribute('font-size',8);t.setAttribute('fill','#012');"
"t.setAttribute('text-anchor','middle');t.setAttribute('dy',3);t.textContent=i+1;"
"g.appendChild(c);g.appendChild(t);g.style.cursor='grab';pad.appendChild(g);"
"dots.push({c:c,t:t});g.onpointerdown=e=>{drag=i;sel=i;selv.textContent=i+1;"
"fyv2.value=src[i].y;fyvv.textContent=src[i].y;pad.setPointerCapture(e.pointerId);};}"
"function drawDots(){for(let i=0;i<8;i++){const X=MX(src[i].x),Z=MZ(src[i].z);"
"dots[i].c.setAttribute('cx',X);dots[i].c.setAttribute('cy',Z);"
"dots[i].t.setAttribute('x',X);dots[i].t.setAttribute('y',Z);"
"dots[i].c.setAttribute('fill',i==sel?'#fd7':'#9cf');}}"
/* El pad manda POST /beam type=mf, y ese comando APAGA el tono y enciende
 * MULTIFOCUS en el STM32 sin tocar el indice de modo. O sea que tocarlo estando
 * en el 12 o el 13 dejaba la placa sonando otra cosa mientras la pagina seguia
 * mostrando el modo viejo — parecia que "el modo 13 se rompio". Si no estamos
 * ya en un modo MULTIFOCUS, entramos al 14 EXPLICITAMENTE: la placa y lo que se
 * ve en pantalla dicen lo mismo. */
"function sendSrc(i){"
"if(curMode!=14&&curMode!=15){curMode=14;"
"fetch('/mode',{method:'POST',body:JSON.stringify({mode:14})}).then(poll);}"
"fetch('/beam',{method:'POST',body:JSON.stringify("
"{type:'mf',src:i,fx:src[i].x,fy:src[i].y,fz:src[i].z})});}"
/* De pixeles de pantalla a unidades del viewBox, con la matriz del PROPIO SVG.
 * La cuenta a mano —(clientX-r.left)*300/r.width— asume que el viewBox se
 * estira hasta ocupar la caja, y NO es lo que pasa: con preserveAspectRatio por
 * defecto el contenido se escala UNIFORME y se centra. El viewBox es 300x215 y
 * la caja en un escritorio son ~608x215, asi que el dibujo ocupa los 300 px del
 * medio con 154 px de aire a cada lado. El factor quedaba en 300/608 = 0,49: el
 * punto seguia al mouse a la MITAD de velocidad en X — y exacto en Y, porque la
 * altura si coincidia. getScreenCTM() contempla el centrado, el zoom de pagina
 * y cualquier aspecto, asi que tambien vale en un telefono angosto. */
"function toPad(e){const p=pad.createSVGPoint();p.x=e.clientX;p.y=e.clientY;"
"return p.matrixTransform(pad.getScreenCTM().inverse());}"
/* ⚠️ `e.buttons` NO es opcional: es lo que evita el ARRASTRE FANTASMA.
 *
 * Sintoma reportado el 2026-08-20: el equipo estaba en modo 0 y "despues de un
 * rato empezaba a tirar un tono solo", con `MFOCUS src0: ...` en el serie. No
 * era el STM32: era ESTA pagina mandando POST /beam type=mf, y ese comando
 * apaga el monitor y enciende MULTIFOCUS.
 *
 * Si un pointerup se pierde —soltar fuera de la ventana, cambiar de pestania a
 * mitad de arrastre, un gesto del trackpad— `drag` se quedaba en >=0 para
 * siempre, y a partir de ahi CUALQUIER paso del mouse por encima del pad
 * reapuntaba una fuente y cambiaba el modo. Sin hacer clic.
 *
 * Empeoro justo ahora porque el pad paso a ocupar todo el ancho (height:auto):
 * el cursor lo cruza mucho mas seguido, sobre todo al scrollear.
 *
 * En un pointermove, `buttons` vale 0 si no hay ningun boton apretado. Un
 * arrastre real nunca tiene buttons==0, asi que esto no cuesta nada y mata la
 * clase entera de fallas. */
"pad.onpointermove=e=>{if(drag<0)return;"
"if(!e.buttons){drag=-1;return;}"
"const q=toPad(e);"
"src[drag].x=IX(q.x);src[drag].z=IZ(q.y);drawDots();"
/* Throttle: arrastrar genera decenas de eventos por segundo y cada POST espera
 * el ACK del STM32 por UART a 115200. Sin esto se encola y la UI se traba. */
"if(Date.now()-padT>150){padT=Date.now();sendSrc(drag);}};"
"pad.onpointerup=()=>{if(drag>=0){sendSrc(drag);drag=-1;}};"
"pad.onpointercancel=()=>{drag=-1;};"
/* Redes de seguridad para los pointerup que nunca llegan al pad: soltar fuera
 * de la ventana, perder la captura del puntero, o irse a otra pestania. */
"addEventListener('pointerup',()=>{drag=-1;});"
"pad.addEventListener('lostpointercapture',()=>{drag=-1;});"
"addEventListener('blur',()=>{drag=-1;});"
"fyv2.oninput=()=>{src[sel].y=+fyv2.value;fyvv.textContent=fyv2.value;};"
"fyv2.onchange=()=>sendSrc(sel);"
"drawDots();"
/* Confirmacion antes de resetear: es destructivo (pisa focos y ajustes) y el
 * boton esta a un toque de distancia en un telefono. */
"function doReset(){if(!confirm('Volver todo a valores de fabrica?'))return;"
"rstv.textContent='...';fetch('/reset',{method:'POST'}).then(r=>r.json()).then(s=>{"
"rstv.textContent=s.status=='ok'?'listo':s.status;"
/* Los sliders quedaron mostrando lo viejo: reponer desde el estado real. */
"for(let i=0;i<8;i++){src[i]={x:-700+200*i,y:0,z:1000};}sel=0;drawDots();"
"fyv2.value=0;fyvv.textContent=0;igTouch=0;volTouch=0;poll();"
"setTimeout(()=>rstv.textContent='',3000);});}"
/* 0 = rojo (mal o sin comprobar), 1 = ambar (en curso o al limite), 2 = verde. */
"function chk(n,st,txt){const e=document.getElementById('k'+n);"
"e.className=st==2?'ok':(st==1?'warn':'');e.textContent=txt;}"
/* Volver a entrar al modo 11 relanza el scan — por eso el boton sirve tanto
 * para arrancar como para repetir tras corregir algo del checklist. */
"function startAF(){fetch('/mode',{method:'POST',body:JSON.stringify({mode:11})}).then(poll);}"
"function setRx(){fetch('/beam',{method:'POST',body:JSON.stringify({type:'rx',az:+az.value})});}"
"function setTx(){fetch('/beam',{method:'POST',body:JSON.stringify({type:'tx',fx:+fx.value,fy:+fy.value,fz:+fz.value})});}"
/* ===== Actualizacion del firmware del STM32 =====
 * XMLHttpRequest y no fetch(): fetch no da progreso de SUBIDA (el
 * ReadableStream de request body no esta disponible en Safari), y una barra que
 * no se mueve durante 15 s se lee como "se colgo". Aca la subida es lo lento.
 *
 * Se corta el poll de /status mientras dura: el server HTTP del ESP32 atiende
 * de a una, asi que cada poll competiria con la carga y ademas se veria como
 * "sin conexion" justo cuando todo va bien. */
"let fwBusy=false;"
/* Un renglon por chequeo: el veredicto Y el numero que lo respalda, para que un
 * rojo se pueda discutir en vez de solo creer. Misma disciplina que el
 * checklist del AUTOFOCUS. */
"function doHealth(){const L=document.getElementById('hlist'),S=document.getElementById('hvsum');"
"S.textContent='midiendo...';S.style.color='#9cf';L.innerHTML='';"
"fetch('/health').then(r=>r.json()).then(d=>{"
"if(!d.ok){S.textContent=d.message||'sin respuesta';S.style.color='#f84';return;}"
"const C=['#f84','#fc4','#6d6'],T=['\\u2717','\\u26a0','\\u2713'];"
"L.innerHTML=d.checks.map(c=>'<li style=\"color:'+C[c.st]+'\">'+T[c.st]+' <b>'+c.n+'</b> \\u2014 '+c.v+'</li>').join('');"
"S.textContent=d.worst==2?'todo en orden':(d.worst==1?'con reservas':'HAY ALGO MAL');"
"S.style.color=C[d.worst];"
"}).catch(()=>{S.textContent='sin conexion';S.style.color='#f84';});}"
"function doFw(){const f=document.getElementById('fwfile').files[0];"
"const m=document.getElementById('fwmsg'),bw=document.getElementById('fwbarw'),b=document.getElementById('fwbar');"
"if(!f){m.textContent='eleg\\u00ed un archivo .bin primero';return;}"
"if(f.size<1024||f.size>917504){m.textContent='ese archivo no parece un firmware ('+f.size+' bytes)';return;}"
"if(!confirm('Actualizar el firmware del STM32 con '+f.name+' ('+f.size+' bytes)?\\n\\n"
"El equipo queda mudo y se reinicia al terminar.'))return;"
"fwBusy=true;bw.style.display='block';b.style.width='0%';"
"m.style.color='#9cf';m.textContent='enviando...';"
"const x=new XMLHttpRequest();"
"x.open('POST','/firmware');"
"x.timeout=180000;"
"x.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded*100/e.total);"
"b.style.width=p+'%';m.textContent='enviando... '+p+'%';}};"
"x.onload=()=>{let r={};try{r=JSON.parse(x.responseText);}catch(e){}"
"if(r.status=='ok'){b.style.width='100%';m.style.color='#6d6';"
"m.textContent='\\u2713 '+r.message+' \\u2014 volv\\u00e9 a mirar dentro de unos segundos';"
"setTimeout(()=>{fwBusy=false;poll();},6000);}"
"else{m.style.color='#f84';m.textContent='\\u2717 '+(r.message||('error HTTP '+x.status));fwBusy=false;}};"
"x.onerror=()=>{m.style.color='#f84';m.textContent='\\u2717 se corto la conexion';fwBusy=false;};"
"x.ontimeout=()=>{m.style.color='#f84';m.textContent='\\u2717 el equipo no contesto a tiempo';fwBusy=false;};"
"x.send(f);}"
/* Subida del firmware del ESP32. Igual que la del STM32 salvo dos cosas: no hay
 * progreso posterior que mirar (el chip se va a reiniciar) y la pagina se cae
 * unos segundos, asi que se avisa antes en vez de dejar al usuario mirando un
 * error. */
"function doFwEsp(){const f=document.getElementById('efwfile').files[0];"
"const m=document.getElementById('efwmsg'),bw=document.getElementById('efwbarw'),b=document.getElementById('efwbar');"
"if(!f){m.textContent='eleg\\u00ed un archivo .bin primero';return;}"
"if(f.size<65536||f.size>2031616){m.textContent='ese archivo no parece un firmware de ESP32 ('+f.size+' bytes)';return;}"
"if(!confirm('Actualizar el firmware del ESP32 con '+f.name+' ('+f.size+' bytes)?\\n\\n"
"Se reinicia y vas a perder esta pagina unos 15 s.\\nSi la imagen nueva no se conecta al WiFi, vuelve sola a la anterior.'))return;"
"fwBusy=true;bw.style.display='block';b.style.width='0%';"
"m.style.color='#9cf';m.textContent='enviando...';"
"const x=new XMLHttpRequest();x.open('POST','/firmware-esp');x.timeout=180000;"
"x.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded*100/e.total);"
"b.style.width=p+'%';m.textContent='enviando... '+p+'%';}};"
"x.onload=()=>{let r={};try{r=JSON.parse(x.responseText);}catch(e){}"
"if(r.status=='ok'){b.style.width='100%';m.style.color='#6d6';"
"m.textContent='\\u2713 '+r.message;"
"setTimeout(()=>{fwBusy=false;location.reload();},18000);}"
"else{m.style.color='#f84';m.textContent='\\u2717 '+(r.message||('error HTTP '+x.status));fwBusy=false;}};"
"x.onerror=()=>{m.style.color='#f84';m.textContent='\\u2717 se corto la conexion';fwBusy=false;};"
"x.ontimeout=()=>{m.style.color='#f84';m.textContent='\\u2717 el equipo no contesto a tiempo';fwBusy=false;};"
"x.send(f);}"
"function poll(){if(fwBusy)return;fetch('/status').then(r=>r.json()).then(s=>{"
"document.getElementById('link').textContent=s.stm32=='ok'?'\\u25cf online':'\\u25cb '+s.stm32;"
/* ===== Identidad del ESP32 y compatibilidad entre los dos firmwares =====
 * Va ANTES del return de abajo a proposito: cuando el STM32 no contesta es
 * justo cuando hace falta ver que tiene el ESP32 y que version espera. */
"if(s.esp_fw){document.getElementById('efwid').textContent=(s.esp_sha||'?')+' ('+(s.esp_slot||'?')+')';"
"document.getElementById('efwdate').textContent=s.esp_fw||'';}"
/* El orden no es una regla que haya que acordarse: se deduce del estado y se
 * dice acá. Los dos chips publican con que version de protocolo fueron
 * compilados, y si no coinciden esto nombra CUAL falta.
 *
 * Cuando hay que actualizar los dos, el STM32 va PRIMERO: el ESP32 es el que
 * transporta la carga hasta el STM32, asi que conviene tenerlo estable y
 * conocido hasta el ultimo paso — y ademas el del ESP32 es el que se deshace
 * solo si sale mal, o sea el que menos importa dejar para el final. */
"{const g=document.getElementById('fwsync'),pv=s.proto_ver,pe=s.proto_esp;"
"g.style.display='block';"
"if(s.stm32!='ok'){g.style.background='#3a1414';g.style.borderColor='#a33';"
"g.innerHTML='\\u26a0 <b>El STM32 no contesta.</b> Si reci\\u00e9n actualizaste el ESP32, esper\\u00e1 unos segundos. "
"Si sigue as\\u00ed, revis\\u00e1 la alimentaci\\u00f3n o carg\\u00e1 el firmware por ST-Link.';}"
"else if(pv==null||pe==null||pv==pe){g.style.background='#16281a';g.style.borderColor='#2a5';"
"g.innerHTML='\\u2713 protocolo v'+pv+' en los dos chips. "
"<span style=color:#8a8>Para actualizar ambos: primero el STM32, despu\\u00e9s el ESP32 \\u2014 el ESP32 es el que lleva la carga hasta el STM32.</span>';}"
"else if(pv<pe){g.style.background='#3a2a10';g.style.borderColor='#a70';"
"g.innerHTML='\\u26a0 <b>Firmwares desparejos.</b> El ESP32 ya est\\u00e1 en v'+pe+' y el STM32 sigue en v'+pv+'. "
"<b>Falta actualizar el STM32</b> \\u2014 el bot\\u00f3n de arriba. Hasta entonces la telemetr\\u00eda puede venir incompleta.';}"
"else{g.style.background='#3a2a10';g.style.borderColor='#a70';"
"g.innerHTML='\\u26a0 <b>Firmwares desparejos.</b> El STM32 ya est\\u00e1 en v'+pv+' y el ESP32 sigue en v'+pe+'. "
"<b>Falta actualizar el ESP32</b> \\u2014 el bot\\u00f3n de abajo.';}}"
"if(s.stm32!='ok')return;"
"curMode=s.mode;"
/* Se recorren los botones QUE EXISTEN y no un contador fijo: la lista la trae
 * /modes, asi que la pagina no sabe de antemano cuantos son —que es justamente
 * el punto—. */
"md.querySelectorAll('button').forEach((b,i)=>{b.className=(i==s.mode)?'on':'';});"
"if(s.vol_db!=null&&s.vol_db>-64&&Date.now()-volTouch>3000){vol.value=s.vol_db;volv.textContent=s.vol_db+' dB';}"
"if(s.ingain_db!=null&&Date.now()-igTouch>3000){ig.value=s.ingain_db;igv.textContent=s.ingain_db+' dB';}"
/* El estado de los amps y el "custom" vienen del STM32, no de lo que la pagina
 * recuerde haber clickeado: un reboot o el K2 largo lo cambian por afuera. */
"if(s.amps!=null){ampsv.textContent=s.amps?'ON':'OFF';ampsv.style.color=s.amps?'#6d6':'#888';}"
/* El "suspendido" se dice fuerte: el caso peligroso no es que este apagado,
 * es creer que va a encender solo y que no vaya a pasar. */
"if(s.autostart!=null){autos.checked=s.autostart;"
"autosv.textContent=s.autostart_held?'SUSPENDIDO':'';"
"autosv.style.color=s.autostart_held?'#e66':'#888';}"
"const cm=document.getElementById('custom');"
"cm.textContent=s.custom?'\u26a0 beam modificado a mano \u2014 el modo marcado no es lo que suena':'';"
/* Los contadores del STM32 son monotonicos: lo que importa es que CREZCAN
 * entre dos polls — eso es "esta saturando ahora", no "saturo alguna vez". */
"if(s.out_clip!=null){const c=document.getElementById('clipv');"
"if(lastClip!==null){const dc=s.out_clip-lastClip.c,dv=s.adc_ovf-lastClip.v;"
"c.textContent=dv>0?'\\u26a0 satura la FUENTE':(dc>0?'\\u26a0 satura: bajar ganancia':'');"
"c.style.color=(dc>0||dv>0)?'#f84':'#9cf';}"
"lastClip={c:s.out_clip,v:s.adc_ovf};}"
"const pk=s.adc_pk||[];for(let i=0;i<8;i++){const v=Math.max(2,Math.min(36,Math.log10((pk[i]||1)+1)/7*36));"
"document.getElementById('b'+i).style.height=v+'px';}"
/* ===== Checklist del AUTOFOCUS =====
 * Cada fila dice el estado Y el numero que lo respalda, para que un rojo se
 * pueda discutir en vez de solo creer. */
"chk(1,s.amps?2:0,s.amps?'amplificadores encendidos':'amplificadores APAGADOS \\u2014 no va a salir sonido');"
"const mlive=(s.mic_live&1)!=0;"
"chk(2,mlive?2:0,mlive?'micr\\u00f3fono con se\\u00f1al en la entrada 1 (c0)':'sin se\\u00f1al en la entrada 1 (c0)');"
"const p0=pk[0]||0,db=p0>0?Math.round(20*Math.log10(p0/8388607)):-99;"
/* El techo en -6 dBFS no es el clipeo del ADC sino el margen que necesitan los
 * picos entre dos lecturas: si el pico de 1 s ya llega ahi, el chirp del scan
 * puede recortar y la correlacion mide una onda deformada. */
"chk(3,p0<2000?0:(db>-6?1:2),'nivel c0: '+db+' dBFS'+(p0<2000?' \\u2014 demasiado bajo':(db>-6?' \\u2014 muy caliente, baj\\u00e1 el preamp':' \\u2014 bien')));"
"const CS=['en reposo','estabilizando sala','preparando','capturando','correlacionando','terminado'];"
"const cs=s.cal_state|0,cp=s.cal_prog|0,ck=s.cal_ok|0;"
"chk(4,cs==0?0:(cs==5?2:1),cs>0&&cs<5?('midiendo parlante '+(cp+1)+'/32 \\u2014 no muevas el micr\\u00f3fono')"
":'scan '+CS[cs]);"
"chk(5,cs!=5?0:(ck>=16?2:0),cs!=5?'sin resultado todav\\u00eda'"
":(ck>=16?('foco aplicado \\u2014 '+ck+'/32 parlantes medidos con confianza')"
":('DESCARTADO \\u2014 solo '+ck+'/32 parlantes \\u00fatiles; focos anteriores intactos')));"
/* La calibracion vive en la EEPROM y sobrevive al reinicio. Las POLARIDADES se
 * aplican a todos los modos; el FOCO medido es de un punto y se recupera con
 * el modo 18. */
"chk(6,s.cal_saved?2:0,s.cal_saved"
"?'en EEPROM \\u2014 polaridades activas en todos los modos; foco medido en el modo 18'"
":'ninguna guardada \\u2014 el equipo usa la geometr\\u00eda nominal');"
/* Identidad del binario que corre. Con OTA dejo de ser obvia: puede haberla
 * subido otro, desde otra maquina. fw_id 00000000 = se compilo sin git. */
"if(s.fw_id!=null){document.getElementById('fwid').textContent="
"(s.fw_id=='00000000'?'sin identificar':s.fw_id.toLowerCase());"
"document.getElementById('fwdate').textContent="
"s.fw_build>0?('compilado '+new Date(s.fw_build*1000).toLocaleString()):'';}"
"document.getElementById('st').textContent="
"'modo: '+s.mode_name+'  |  vol: '+(s.vol_db<=-64?'MUTE':s.vol_db+'dB')+'  |  foco: ('+s.focus.fx+','+s.focus.fy+','+s.focus.fz+')mm'"
"+'\\nunderruns: '+s.underruns+'  errores SAI: '+s.sai_errors+'  OVF: '+s.ovf+'  cal: '+s.cal_state;"
"}).catch(()=>{document.getElementById('link').textContent='\\u25cb sin conexion';});}"
"setInterval(poll,1000);poll();"
"</script></body></html>";

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t ping_handler(httpd_req_t *req)
{
    char resp[64];
    snprintf(resp, sizeof(resp),
             "{\"status\":\"ok\",\"device_id\":\"%s\"}", device_id);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    /* La tabla va ANTES de httpd_start, y el limite sale de ella.
     *
     * HTTPD_DEFAULT_CONFIG deja max_uri_handlers en 8 y el registro que se pasa
     * FALLA EN SILENCIO: el endpoint simplemente no existe y el server contesta
     * "Nothing matches the given URI". Con 9 endpoints /ping ya estaba caido sin
     * que nadie lo notara, y agregar /reset tiro /status — o sea la telemetria
     * entera— sin un solo mensaje de error.
     *
     * Poner un numero fijo "con margen de sobra" NO alcanzo: el margen se
     * consumio solo. El 2026-09-02, con el limite en 16 y 17 rutas en la tabla,
     * la ruta /modes estaba CAIDA en la placa: GET /modes contestaba "Nothing
     * matches the given URI" y nadie lo noto, que es exactamente el modo de
     * falla que el comentario de arriba describia. El aviso del bucle de
     * registro sale por el UART del ESP32, o sea por el cable que casi nunca
     * esta puesto.
     *
     * Derivar el limite de la tabla saca la posibilidad de raiz: agregar una
     * ruta agranda el limite en la misma linea. +2 de colchon para los
     * handlers internos que el httpd registra por su cuenta. */
    httpd_uri_t uris[] = {
            { .uri = "/",        .method = HTTP_GET,  .handler = root_handler    },
            { .uri = "/command", .method = HTTP_POST, .handler = command_handler },
            { .uri = "/mode",    .method = HTTP_POST, .handler = mode_handler    },
            { .uri = "/beam",    .method = HTTP_POST, .handler = beam_handler    },
            { .uri = "/volume",  .method = HTTP_POST, .handler = volume_handler  },
            { .uri = "/amps",    .method = HTTP_POST, .handler = amps_handler    },
            { .uri = "/autostart", .method = HTTP_POST, .handler = autostart_handler },
            { .uri = "/tone",    .method = HTTP_POST, .handler = tone_handler    },
            { .uri = "/mics",    .method = HTTP_POST, .handler = mics_handler    },
            { .uri = "/reset",   .method = HTTP_POST, .handler = reset_handler   },
            { .uri = "/ingain",  .method = HTTP_POST, .handler = ingain_handler  },
            { .uri = "/status",  .method = HTTP_GET,  .handler = status_handler  },
            { .uri = "/ping",    .method = HTTP_GET,  .handler = ping_handler    },
            { .uri = "/firmware",.method = HTTP_POST, .handler = firmware_handler},
            { .uri = "/firmware-esp", .method = HTTP_POST, .handler = firmware_esp_handler },
            { .uri = "/health",  .method = HTTP_GET,  .handler = health_handler  },
            { .uri = "/capture", .method = HTTP_GET,  .handler = capture_handler },
            { .uri = "/cal",     .method = HTTP_GET,  .handler = cal_handler     },
            { .uri = "/modes",   .method = HTTP_GET,  .handler = modes_handler   },
            { .uri = "/fase",    .method = HTTP_GET,  .handler = fase_handler    },
    };

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = sizeof(uris) / sizeof(uris[0]) + 2;
    /* 10 s, no los 5 de fabrica. POST /firmware consume el cuerpo al ritmo al
     * que el STM32 puede grabar la flash (~15 s para 80 kB), asi que el socket
     * pasa mucho tiempo sin que se lo lea; con el timeout corto una pausa del
     * navegador cortaba la subida a mitad de camino. */
    config.recv_wait_timeout = 10;

    printf("[ESP32] Iniciando HTTP server en puerto %d (%u rutas)...\n",
           config.server_port, (unsigned)(sizeof(uris) / sizeof(uris[0])));

    if (httpd_start(&server, &config) == ESP_OK) {
        for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
            esp_err_t e = httpd_register_uri_handler(server, &uris[i]);
            if (e != ESP_OK)
                printf("[ESP32] !! NO se pudo registrar %s: %s — ese endpoint NO existe\n",
                       uris[i].uri, esp_err_to_name(e));
        }

        printf("[ESP32] HTTP server listo\n");
        printf("[ESP32]   GET  /         — UI web (control de modos + beam)\n");
        printf("[ESP32]   POST /command  — config JSON → STM32\n");
        printf("[ESP32]   POST /mode     — {\"mode\":N} (GET /modes lista los validos)\n");
        printf("[ESP32]   POST /beam     — {\"type\":\"rx\",\"az\":30} | {\"type\":\"tx\",\"fx\":0,\"fy\":0,\"fz\":1000}\n");
        printf("[ESP32]   POST /amps     — {\"on\":true|false} enable de los 32 amps\n");
        printf("[ESP32]   GET  /status   — telemetria real del STM32\n");
        printf("[ESP32]   GET  /ping     — health check\n");
        return server;
    }

    printf("[ESP32] ERROR: no se pudo iniciar HTTP server\n");
    return NULL;
}

// ============== WiFi ==============

// Aplica WIFI_NETWORKS[idx] como config activa y dispara la conexion.
// esp_wifi_set_config puede llamarse con el driver ya arrancado (STA_START
// ya disparado); ESP-IDF lo soporta y el connect subsiguiente usa la config
// nueva.
static void wifi_apply_network(uint8_t idx)
{
    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, WIFI_NETWORKS[idx].ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, WIFI_NETWORKS[idx].pass, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    printf("[ESP32] Conectando a WiFi '%s' (%u/%u)...\n",
           WIFI_NETWORKS[idx].ssid, (unsigned)(idx + 1), (unsigned)NUM_WIFI_NETWORKS);
    esp_wifi_connect();
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        wifi_apply_network(wifi_net_idx);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < MAX_RETRY) {
            retry_num++;
            printf("[ESP32] Reintentando '%s' (%d/%d)...\n",
                   WIFI_NETWORKS[wifi_net_idx].ssid, retry_num, MAX_RETRY);
            esp_wifi_connect();
        } else {
            // Agotados los reintentos de esta red: pasar a la siguiente y
            // seguir ciclando indefinidamente — nunca se abandona del todo
            // (el instrumento puede estar en cualquiera de las casas).
            retry_num = 0;
            wifi_net_idx = (wifi_net_idx + 1) % NUM_WIFI_NETWORKS;
            if (wifi_net_idx == 0) {
                // Dimos una vuelta completa sin conectar: avisar (LED lento)
                // pero seguir intentando en background.
                led_wifi_state = 2;
                xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            }
            wifi_apply_network(wifi_net_idx);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        led_wifi_state = 1;  // fijo ON: conectado
        printf("[ESP32] WiFi conectado — IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));
        // Guardar la IP; el monitor task la empuja al STM32 (no mandamos desde
        // el event task para no bloquearlo con el UART).
        snprintf(my_ip, sizeof(my_ip), IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    /* mDNS: publicar "domosonica.local" para no depender de la IP.
     *
     * El DHCP del router la cambia sin avisar, y averiguarla obliga a abrir el
     * monitor serie del STM32 (línea "ESP:") — o sea, un cable y una terminal
     * antes de poder mandar un solo comando. Con esto, cualquier navegador de
     * la red entra a http://domosonica.local sin saber nada.
     *
     * No es ESP_ERROR_CHECK a propósito: si mDNS falla, la placa tiene que
     * seguir funcionando por IP igual. Es una comodidad, no un requisito. */
    {
        esp_err_t merr = mdns_init();
        if (merr == ESP_OK) {
            char inst[48];
            snprintf(inst, sizeof(inst), "Domosonica %s", device_id);
            mdns_hostname_set(mdns_host);
            mdns_instance_name_set(inst);
            mdns_txt_item_t txt[] = { {"id", device_id} };
            mdns_service_add(NULL, "_http", "_tcp", 80, txt, 1);

            /* Ademas del _http generico, un tipo de servicio PROPIO.
             *
             * Con solo _http._tcp, un cliente que quiera armar la lista de la
             * instalacion tiene que enumerar todo lo que habla HTTP en la red
             * —impresoras, el router, camaras— y despues adivinar cual es un
             * nodo. Con _domosonica._tcp la busqueda devuelve exactamente los
             * nodos y nada mas, que es lo que la app iOS necesita para dejar de
             * pedir 15 direcciones IP a mano.
             *
             * En el TXT va lo que sirve para armar la lista SIN conectarse a
             * cada uno: el id de la placa y la version del firmware del ESP32.
             * El estado del STM32 NO va aca a proposito: cambia todo el tiempo
             * y un registro mDNS cacheado que diga "vivo" cuando no lo esta es
             * peor que no decir nada. Para eso esta GET /status. */
            const esp_app_desc_t *ad = esp_app_get_description();
            mdns_txt_item_t txt_ds[] = {
                { "id",  device_id },
                { "ver", ad ? ad->version : "?" },
            };
            mdns_service_add(NULL, "_domosonica", "_tcp", 80,
                             txt_ds, sizeof(txt_ds) / sizeof(txt_ds[0]));

            printf("[ESP32] mDNS: http://%s.local  (id %s, _domosonica._tcp)\n",
                   mdns_host, device_id);
        } else {
            printf("[ESP32] mDNS no arranco (%s) — usar la IP\n",
                   esp_err_to_name(merr));
        }
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t inst_any, inst_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &inst_any));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &inst_ip));

    // La config inicial la aplica wifi_apply_network() en el evento STA_START
    // (dispara con la red WIFI_NETWORKS[0]); esp_wifi_start() solo arranca el
    // driver.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("[ESP32] %u red(es) WiFi conocida(s), ciclando hasta conectar...\n",
           (unsigned)NUM_WIFI_NETWORKS);

    // Espera acotada: NUM_WIFI_NETWORKS x MAX_RETRY intentos, ~2-3s cada uno.
    // Si ninguna red respondio en ese tiempo, seguimos booteando (HTTP server
    // igual arranca) — el ciclo de reconexion sigue solo, en background,
    // via wifi_event_handler, sin bloquear mas el arranque.
    TickType_t boot_wait = pdMS_TO_TICKS(4000UL * NUM_WIFI_NETWORKS * MAX_RETRY);
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, boot_wait);

    if (!(bits & WIFI_CONNECTED_BIT))
        printf("[ESP32] ADVERTENCIA: sin WiFi todavia — HTTP server no accesible "
               "(reintentando en background)\n");
}

// ============== Main ==============

void app_main(void)
{
    // LED GPIO10
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BLINK_GPIO, 0);
    xTaskCreate(led_blink_task, "led_blink", 1024, NULL, 3, NULL);

    /* La identidad primero: el banner, el mDNS y todas las respuestas HTTP la
     * usan, y sale del MAC, que esta disponible sin levantar la radio. */
    device_identity_init();

    // Esperar USB Serial/JTAG
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    printf("\n");
    printf("[ESP32] ============================================\n");
    printf("[ESP32]  ESP32-C3-MINI-1 — DS11 Beamforming Bridge  \n");
    printf("[ESP32]  %s  ->  http://%s.local\n", device_id, mdns_host);
    printf("[ESP32] ============================================\n");
    printf("[ESP32] UART1 RX=GPIO%d ← STM32 PA1 | TX=GPIO%d → STM32 PA0\n",
           STM32_UART_RX, STM32_UART_TX);
    printf("[ESP32] [STM32] = output del STM32H743 forwardeado aqui\n");
    printf("[ESP32] ============================================\n\n");

    // NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Diagnóstico: leer GPIO0 y GPIO1 como inputs antes de UART
    {
        gpio_config_t io = {
            .pin_bit_mask = (1ULL << GPIO_NUM_0) | (1ULL << GPIO_NUM_1),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
        };
        gpio_config(&io);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        int g0 = gpio_get_level(GPIO_NUM_0);
        int g1 = gpio_get_level(GPIO_NUM_1);
        printf("[ESP32] DIAG: GPIO0 (TX->STM32 RX PA1) = %d, GPIO1 (RX<-STM32 TX PA0) = %d\n", g0, g1);
        printf("[ESP32] DIAG: Esperado: GPIO1=1 (UART idle HIGH) si STM32 conectado\n");
    }

    // UART1 → STM32
    uart_init();

    // Dar tiempo al STM32 para completar su secuencia de init (~2-3s):
    // PLL 480MHz, SAI init, PCM1690 MCLK bit-bang, I2C scan.
    // Sin este delay, el ESP32 puede generar ruido en TX (GPIO0→PA1)
    // antes de que el STM32 haya armado HAL_UART_Receive_IT(), causando
    // ORE (overrun error) que silencia el link en cold boot.
    printf("[ESP32] Esperando boot STM32 (3s)...\n");
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    // Semaforos request/response del protocolo binario (antes del monitor task)
    ack_sem = xSemaphoreCreateBinary();
    health_sem = xSemaphoreCreateBinary();
    cap_info_sem = xSemaphoreCreateBinary();
    cap_data_sem = xSemaphoreCreateBinary();
    phase_info_sem = xSemaphoreCreateBinary();
    phase_data_sem = xSemaphoreCreateBinary();
    cal_sem = xSemaphoreCreateBinary();
    modes_sem = xSemaphoreCreateBinary();
    status_sem = xSemaphoreCreateBinary();
    uart_tx_mutex = xSemaphoreCreateRecursiveMutex();

    // Tarea de monitoreo: lee UART1 RX y forwardea con prefijo [STM32]
    xTaskCreate(stm32_monitor_task, "stm32_mon", 4096, NULL, 5, NULL);

    // WiFi + HTTP
    wifi_init();
    start_webserver();

    /* Confirma (o deshace) una imagen recien instalada por OTA.
     *
     * ⚠️ VA DESPUES de wifi_init(): `wifi_event_group` se crea ahi adentro. La
     * primera version de esto lanzaba la tarea antes, con lo cual esperaba sobre
     * un grupo de eventos NULL, entraba en panico y reiniciaba.
     *
     * Lo interesante es que ese bug NO se vio al flashear por cable —una imagen
     * grabada por cable no esta "a prueba", asi que la tarea salia antes de
     * tocar nada— y aparecio en la primera actualizacion por WiFi de verdad. La
     * vuelta atras hizo justo lo que promete: la imagen nueva no confirmo, el
     * bootloader restauro la anterior, y el equipo siguio en la red. */
    xTaskCreate(ota_confirm_task, "ota_confirm", 3072, NULL, 4, NULL);

    printf("[ESP32] Sistema listo. Esperando comandos HTTP y output STM32...\n\n");

    // Loop principal — nada que hacer, las tareas corren en background
    while (1) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
