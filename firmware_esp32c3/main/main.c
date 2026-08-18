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
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "cJSON.h"

// ============== Configuracion hardware ==============

#define BLINK_GPIO       GPIO_NUM_10   // LED en GPIO10
#define BLINK_PERIOD_MS  500           // 500ms ON / 500ms OFF = 1Hz

// WiFi credentials live in secrets.h (gitignored). Copy secrets.h.example to
// secrets.h and fill in your network — keeps passwords out of the public repo.
#include "secrets.h"
#define DEVICE_ID        "DS_01"

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
#define CMD_ACK          0x06
#define CMD_NACK         0x15
#define NUM_CHANNELS     32     // los 32 de DS11 (era 16 = media matriz)
// Techo del payload. DEBE coincidir con PROTO_MAX_PAYLOAD del STM32: si el
// STM32 manda mas de lo que aca se acepta, la trama se descarta por larga y el
// endpoint queda mudo SIN error visible.
#define PROTO_MAX_PAYLOAD 128
#define ESP_IP_PUSH_MS   10000  // reenvio periodico: si el STM32 resetea, recupera la IP

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
} ds_status_t;
_Static_assert(sizeof(ds_status_t) == 68, "ds_status_t debe ser 68 bytes");
#define STATE_F_AMPS    0x01
#define STATE_F_CUSTOM  0x02

// Espejo de ui_states[] del STM32 — actualizar junto con el firmware STM32.
// El indice es lo que viaja en CMD_SET_MODE; el nombre es solo para la UI.
static const char *MODE_NAMES[] = {
    "ADC monitor",
    "Channel walker (1s/ch)",
    "RX beam az=0 (broadside)",
    "RX beam az=+30",
    "RX beam az=-30",
    "RX beam az=+60",
    "RX beam az=-60",
    "TX tono 1k centro @1m",
    "TX tono 1k izq 0.5m @1m",
    "TX tono 1k der 0.5m @1m",
    "TX tono 1k CERCA 0.3m",
    "AUTOFOCUS scan (mic->c0)",
    "TX tono 2k dither foco @1m",
    "TX tono 1k dither foco @1m",
    "MULTIFOCUS 4 tonos @1m",
    "MULTIFOCUS 8 entradas @1m",
};
#define NUM_MODES (sizeof(MODE_NAMES) / sizeof(MODE_NAMES[0]))

// Sincronizacion request/response con el STM32
static SemaphoreHandle_t ack_sem;      // ACK/NACK recibido
static SemaphoreHandle_t status_sem;   // CMD_STATUS recibido
static volatile bool     last_ack_ok = false;
static ds_status_t       last_status;  // valido tras tomar status_sem

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
        } else if (fr_cmd == CMD_STATUS && fr_len == sizeof(ds_status_t)) {
            uint8_t x = 0;
            for (uint16_t i = 0; i < fr_len; i++) x ^= fr_payload[i];
            if (x == fr_cksum) {
                memcpy(&last_status, fr_payload, sizeof(ds_status_t));
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
        int len = uart_read_bytes(STM32_UART_NUM, raw, sizeof(raw),
                                  100 / portTICK_PERIOD_MS);

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
static void uart_push_esp_ip(void)
{
    if (!my_ip[0]) return;
    uart_send_frame(CMD_ESP_INFO, (const uint8_t *)my_ip, strlen(my_ip));
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
        "{\"status\":\"%s\",\"device_id\":\"" DEVICE_ID "\",\"message\":\"%s\"}",
        ok ? "ok" : "error",
        ok ? "Config aplicada por el STM32"
           : "JSON invalido, o el STM32 no la acepto (ver log serie)");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// GET /status — pide CMD_GET_STATUS al STM32 y devuelve la telemetria real
static esp_err_t status_handler(httpd_req_t *req)
{
    xSemaphoreTake(status_sem, 0);              // drenar status viejo
    uart_send_frame(CMD_GET_STATUS, NULL, 0);

    char buf[640];
    /* 800 ms, no 400. El STM32 manda el status byte a byte sirviendo los fills
     * de audio entre medio (para no meter un pop de 6 ms), asi que en el modo
     * mas pesado —el 15, 8 fuentes x 32 salidas— la trama entera puede tardar
     * varios cientos de ms. Con 400 ms se veian timeouts esporadicos JUSTO en
     * el modo que mas interesa monitorear. El audio siempre gana; la telemetria
     * espera. Sigue entrando en el poll de 1 s de la pagina. */
    if (xSemaphoreTake(status_sem, 800 / portTICK_PERIOD_MS) == pdTRUE) {
        const ds_status_t *s = &last_status;
        const char *mname = (s->mode_idx < NUM_MODES)
                          ? MODE_NAMES[s->mode_idx] : "?";
        snprintf(buf, sizeof(buf),
            "{\"stm32\":\"ok\",\"device_id\":\"" DEVICE_ID "\","
            "\"proto_ver\":%u,\"mode\":%u,\"mode_name\":\"%s\",\"num_modes\":%u,"
            "\"rx_az\":%d,\"focus\":{\"fx\":%d,\"fy\":%d,\"fz\":%d},"
            "\"underruns\":%lu,\"sai_errors\":%lu,\"ovf\":%u,\"cal_state\":%u,"
            "\"vol_db\":%d,\"ingain_db\":%u,\"amps\":%s,\"custom\":%s,"
            "\"adc_ovf\":%lu,\"out_clip\":%lu,"
            "\"adc_pk\":[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld],"
            "\"stm32_uptime_ms\":%lu,\"uptime_ms\":%lu}",
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
            (unsigned long)s->uptime_ms,
            (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    } else {
        /* Sin respuesta del STM32 no hay uptime suyo que informar; el que va
         * es el de este chip, y por eso lleva su nombre. */
        snprintf(buf, sizeof(buf),
            "{\"stm32\":\"timeout\",\"device_id\":\"" DEVICE_ID "\","
            "\"uptime_ms\":%lu}",
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
    if (idx < 0 || idx >= (int)NUM_MODES) {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"error\",\"message\":\"mode 0-%d\"}",
                 (int)NUM_MODES - 1);
    } else {
        uint8_t payload = (uint8_t)idx;
        int ack = uart_send_and_wait_ack(CMD_SET_MODE, &payload, 1, 300);
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"%s\",\"mode\":%d,\"mode_name\":\"%s\"}",
                 ack == 1 ? "ok" : (ack == 0 ? "nack" : "timeout"),
                 idx, MODE_NAMES[idx]);
        printf("[ESP32] POST /mode %d (%s) -> %s\n", idx, MODE_NAMES[idx],
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
"style='width:100%;height:215px;background:#111;border-radius:6px;touch-action:none'>"
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
"<div class=card><h2>Entradas ADC (peak)</h2><div class=meter id=meter></div>"
"<div id=st>esperando estado...</div></div>"
"<div class=card><h2>Volver a cero</h2>"
"<div class=row><button onclick=doReset()>Reset de f&aacute;brica</button>"
"<span class=val id=rstv></span></div>"
"<div style='font-size:.75em;color:#888;margin-top:6px'>Volumen &minus;24 dB, "
"ganancia 0 dB, modo 0 y focos por defecto. No toca los amplificadores.</div></div>"
"<script>"
"const N=16,names=['ADC monitor','Walker 1s/ch','RX az=0','RX az=+30','RX az=-30','RX az=+60','RX az=-60','TX 1k centro @1m','TX 1k izq @1m','TX 1k der @1m','TX 1k cerca 0.3m','AUTOFOCUS mic','Tono 2k dither','Tono 1k dither','MULTIFOCUS 4t','MULTIFOCUS 8in'];"
"const md=document.getElementById('modes');"
/* El numero adelante: es lo que se usa para referenciar el modo en los comandos
 * (POST /mode {"mode":N}), en los logs y al hablar del equipo. */
"for(let i=0;i<N;i++){const b=document.createElement('button');"
"b.innerHTML='<b style=opacity:.55>'+i+'</b> '+names[i];b.id='m'+i;"
"b.onclick=()=>fetch('/mode',{method:'POST',body:JSON.stringify({mode:i})}).then(poll);md.appendChild(b);}"
"const mt=document.getElementById('meter');"
"for(let i=0;i<8;i++){const d=document.createElement('div');d.className='bar';d.id='b'+i;mt.appendChild(d);}"
"['az','fx','fy','fz'].forEach(id=>{const e=document.getElementById(id);"
"e.oninput=()=>document.getElementById(id+'v').textContent=e.value+(id=='az'?'\\u00b0':'');});"
"function setVol(d){fetch('/volume',{method:'POST',body:JSON.stringify({db:d})});}"
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
"pad.onpointermove=e=>{if(drag<0)return;const r=pad.getBoundingClientRect();"
"const px=(e.clientX-r.left)*300/r.width,py=(e.clientY-r.top)*215/r.height;"
"src[drag].x=IX(px);src[drag].z=IZ(py);drawDots();"
/* Throttle: arrastrar genera decenas de eventos por segundo y cada POST espera
 * el ACK del STM32 por UART a 115200. Sin esto se encola y la UI se traba. */
"if(Date.now()-padT>150){padT=Date.now();sendSrc(drag);}};"
"pad.onpointerup=()=>{if(drag>=0){sendSrc(drag);drag=-1;}};"
"pad.onpointercancel=()=>{drag=-1;};"
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
"function setRx(){fetch('/beam',{method:'POST',body:JSON.stringify({type:'rx',az:+az.value})});}"
"function setTx(){fetch('/beam',{method:'POST',body:JSON.stringify({type:'tx',fx:+fx.value,fy:+fy.value,fz:+fz.value})});}"
"function poll(){fetch('/status').then(r=>r.json()).then(s=>{"
"document.getElementById('link').textContent=s.stm32=='ok'?'\\u25cf online':'\\u25cb '+s.stm32;"
"if(s.stm32!='ok')return;"
"curMode=s.mode;"
"for(let i=0;i<N;i++)document.getElementById('m'+i).className=(i==s.mode)?'on':'';"
"if(s.vol_db!=null&&s.vol_db>-64&&Date.now()-volTouch>3000){vol.value=s.vol_db;volv.textContent=s.vol_db+' dB';}"
"if(s.ingain_db!=null&&Date.now()-igTouch>3000){ig.value=s.ingain_db;igv.textContent=s.ingain_db+' dB';}"
/* El estado de los amps y el "custom" vienen del STM32, no de lo que la pagina
 * recuerde haber clickeado: un reboot o el K2 largo lo cambian por afuera. */
"if(s.amps!=null){ampsv.textContent=s.amps?'ON':'OFF';ampsv.style.color=s.amps?'#6d6':'#888';}"
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
    const char *resp = "{\"status\":\"ok\",\"device_id\":\"" DEVICE_ID "\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    /* HTTPD_DEFAULT_CONFIG deja max_uri_handlers en 8 y el registro que se pasa
     * FALLA EN SILENCIO: el endpoint simplemente no existe y el server contesta
     * "Nothing matches the given URI". Con 9 endpoints /ping ya estaba caido sin
     * que nadie lo notara, y agregar /reset tiro /status — o sea la telemetria
     * entera— sin un solo mensaje de error. Margen de sobra y chequeo abajo. */
    config.max_uri_handlers = 16;

    printf("[ESP32] Iniciando HTTP server en puerto %d...\n", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uris[] = {
            { .uri = "/",        .method = HTTP_GET,  .handler = root_handler    },
            { .uri = "/command", .method = HTTP_POST, .handler = command_handler },
            { .uri = "/mode",    .method = HTTP_POST, .handler = mode_handler    },
            { .uri = "/beam",    .method = HTTP_POST, .handler = beam_handler    },
            { .uri = "/volume",  .method = HTTP_POST, .handler = volume_handler  },
            { .uri = "/amps",    .method = HTTP_POST, .handler = amps_handler    },
            { .uri = "/reset",   .method = HTTP_POST, .handler = reset_handler   },
            { .uri = "/ingain",  .method = HTTP_POST, .handler = ingain_handler  },
            { .uri = "/status",  .method = HTTP_GET,  .handler = status_handler  },
            { .uri = "/ping",    .method = HTTP_GET,  .handler = ping_handler    },
        };
        for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
            esp_err_t e = httpd_register_uri_handler(server, &uris[i]);
            if (e != ESP_OK)
                printf("[ESP32] !! NO se pudo registrar %s: %s — ese endpoint NO existe\n",
                       uris[i].uri, esp_err_to_name(e));
        }

        printf("[ESP32] HTTP server listo\n");
        printf("[ESP32]   GET  /         — UI web (control de modos + beam)\n");
        printf("[ESP32]   POST /command  — config JSON → STM32\n");
        printf("[ESP32]   POST /mode     — {\"mode\":0-%d}\n", (int)NUM_MODES - 1);
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
            mdns_hostname_set("domosonica");
            mdns_instance_name_set("Domosonica DS11");
            mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
            printf("[ESP32] mDNS: http://domosonica.local\n");
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

    // Esperar USB Serial/JTAG
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    printf("\n");
    printf("[ESP32] ============================================\n");
    printf("[ESP32]  ESP32-C3-MINI-1 — DS11 Beamforming Bridge  \n");
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
    status_sem = xSemaphoreCreateBinary();
    uart_tx_mutex = xSemaphoreCreateRecursiveMutex();

    // Tarea de monitoreo: lee UART1 RX y forwardea con prefijo [STM32]
    xTaskCreate(stm32_monitor_task, "stm32_mon", 4096, NULL, 5, NULL);

    // WiFi + HTTP
    wifi_init();
    start_webserver();

    printf("[ESP32] Sistema listo. Esperando comandos HTTP y output STM32...\n\n");

    // Loop principal — nada que hacer, las tareas corren en background
    while (1) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
