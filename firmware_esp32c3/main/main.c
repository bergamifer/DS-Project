/* ESP32-C3-MINI-1 — DS9 Beamforming Bridge
 *
 * Canal de monitoreo unificado via USB Serial/JTAG:
 *   [ESP32] mensajes propios del ESP32
 *   [STM32] debug output del STM32H743 (forwardeado desde UART1 RX)
 *
 * Hardware DS9:
 *   UART1 TX → GPIO 0 → PA1 → STM32 UART4 RX   tramas binarias
 *   UART1 RX ← GPIO 1 ← PA0 ← STM32 UART4 TX   texto debug ASCII
 *
 *   NOTA DS9 PCB: las trazas GPIO0/GPIO1 van a PA1/PA0 respectivamente
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
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "cJSON.h"

// ============== Configuracion hardware ==============

#define BLINK_GPIO       GPIO_NUM_10   // LED en GPIO10 (DS9 board)
#define BLINK_PERIOD_MS  500           // 500ms ON / 500ms OFF = 1Hz

#define WIFI_SSID        "CasaBergesi 2.4GHz-2.4GHz"
#define WIFI_PASS        "12943801"
#define DEVICE_ID        "DS_01"

// UART0: link con STM32. DS9 PCB: GPIO0→PA1(STM32 RX), GPIO1→PA0(STM32 TX)
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
#define NUM_CHANNELS     16

typedef struct {
    uint8_t  master_gain;
    uint8_t  mute_global;
    uint16_t delays[NUM_CHANNELS];
    uint8_t  gains[NUM_CHANNELS];
    uint16_t mutes;
} ds_config_t;

// ============== WiFi ==============

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int retry_num = 0;
#define MAX_RETRY 10

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

    // Test: enviar "HELLO\n" al STM32 para verificar TX funciona
    const char *hello = "HELLO_FROM_ESP32\r\n";
    uart_write_bytes(STM32_UART_NUM, hello, strlen(hello));
    printf("[ESP32] Enviado '%s' al STM32 via UART1 TX (GPIO%d)\n", "HELLO_FROM_ESP32", STM32_UART_TX);
}

// Tarea que lee continuamente del STM32 y forwardea al terminal con prefijo [STM32]
static void stm32_monitor_task(void *arg)
{
    uint8_t raw[128];
    char    line[STM32_LINE_BUF];
    int     line_idx = 0;
    uint32_t total_bytes = 0;
    uint32_t last_report = 0;

    printf("[ESP32] Monitor STM32 iniciado (UART%d RX=GPIO%d, %d baud)\n",
           STM32_UART_NUM, STM32_UART_RX, STM32_UART_BAUD);

    // === TEST GPIO CRUDO: deshabilitar UART, leer GPIO0 como input ===
    // Contar transiciones durante 2 segundos
    uart_driver_delete(STM32_UART_NUM);
    gpio_config_t io_test = {
        .pin_bit_mask = (1ULL << STM32_UART_RX),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_test);

    printf("[ESP32] GPIO TEST: leyendo GPIO%d crudo por 3 segundos...\n", STM32_UART_RX);
    printf("[ESP32] GPIO TEST: si STM32 transmite, deberiamos ver transiciones\n");

    int last_level = gpio_get_level(STM32_UART_RX);
    uint32_t transitions = 0;
    uint32_t samples = 0;
    uint32_t highs = 0;
    uint32_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < (3000 / portTICK_PERIOD_MS)) {
        int level = gpio_get_level(STM32_UART_RX);
        if (level != last_level) {
            transitions++;
            last_level = level;
        }
        if (level) highs++;
        samples++;
    }
    printf("[ESP32] GPIO TEST resultado: %lu transiciones en 3s, %lu muestras, %lu%% HIGH\n",
           transitions, samples, (highs * 100) / samples);
    if (transitions == 0) {
        printf("[ESP32] GPIO TEST: 0 transiciones = NO HAY DATOS en GPIO%d!\n", STM32_UART_RX);
        printf("[ESP32] GPIO TEST: Verificar cableado PA0 <-> GPIO1 (STM32 TX)\n");
    } else {
        printf("[ESP32] GPIO TEST: %lu transiciones = HAY DATOS! Problema es del UART driver\n", transitions);
    }

    // Re-inicializar UART para operación normal
    uart_config_t cfg = {
        .baud_rate  = STM32_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(STM32_UART_NUM, &cfg);
    uart_set_pin(STM32_UART_NUM, STM32_UART_TX, STM32_UART_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(STM32_UART_NUM, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 0, NULL, 0);

    printf("[ESP32] UART re-inicializado, continuando monitor normal...\n");

    while (1) {
        int len = uart_read_bytes(STM32_UART_NUM, raw, sizeof(raw),
                                  100 / portTICK_PERIOD_MS);

        // Periodic report even if no data
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_report >= 5000) {
            last_report = now;
            printf("[ESP32] STM32 monitor: %lu bytes recibidos total\n", total_bytes);
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

static void uart_send_config(const ds_config_t *cfg)
{
    // Payload: master_gain(1) + mute_global(1) + delays(32) + gains(16) + mutes(2) = 52 bytes
    const uint16_t payload_len = 1 + 1 + (NUM_CHANNELS * 2) + NUM_CHANNELS + 2;
    uint8_t payload[52];
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

    // Trama: Header(2) + Cmd(1) + Length(2) + Payload(52) + Checksum(1) + Footer(2) = 60 bytes
    uint8_t frame[60];
    int fi = 0;

    frame[fi++] = PROTO_HEADER_1;
    frame[fi++] = PROTO_HEADER_2;
    frame[fi++] = CMD_UPDATE_CFG;
    frame[fi++] = payload_len & 0xFF;
    frame[fi++] = (payload_len >> 8) & 0xFF;
    memcpy(&frame[fi], payload, payload_len);
    fi += payload_len;
    frame[fi++] = calc_checksum(payload, payload_len);
    frame[fi++] = PROTO_FOOTER_1;
    frame[fi++] = PROTO_FOOTER_2;

    int sent = uart_write_bytes(STM32_UART_NUM, frame, fi);
    printf("[ESP32] Trama enviada al STM32: %d bytes (master_gain=%d, mute_global=%d)\n",
           sent, cfg->master_gain, cfg->mute_global);

    // Debug hex (solo los primeros 10 bytes para no saturar el log)
    printf("[ESP32] HEX[0..9]: ");
    for (int i = 0; i < 10 && i < fi; i++) printf("%02X ", frame[i]);
    printf("...\n");
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
    if (master_gain)printf("[ESP32]   master_gain:    %.2f\n", master_gain->valuedouble);
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
                    cfg.mutes |= (1 << c);
            }
        }
    }

    printf("[ESP32] ========================================\n");

    uart_send_config(&cfg);
    cJSON_Delete(root);
    return true;
}

// ============== HTTP Handlers ==============

static esp_err_t command_handler(httpd_req_t *req)
{
    char buf[2048];
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
        ok ? "Config sent to STM32" : "JSON parse error");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    printf("[ESP32] HTTP GET /status\n");
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\",\"device_id\":\"" DEVICE_ID "\","
        "\"uart_baud\":%d,"
        "\"uart_tx_gpio\":%d,\"uart_rx_gpio\":%d,"
        "\"uptime_ms\":%lu}",
        STM32_UART_BAUD,
        STM32_UART_TX, STM32_UART_RX,
        (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
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

    printf("[ESP32] Iniciando HTTP server en puerto %d...\n", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uris[] = {
            { .uri = "/command", .method = HTTP_POST, .handler = command_handler },
            { .uri = "/status",  .method = HTTP_GET,  .handler = status_handler  },
            { .uri = "/ping",    .method = HTTP_GET,  .handler = ping_handler    },
        };
        for (int i = 0; i < 3; i++)
            httpd_register_uri_handler(server, &uris[i]);

        printf("[ESP32] HTTP server listo\n");
        printf("[ESP32]   POST /command  — config JSON → STM32\n");
        printf("[ESP32]   GET  /status   — estado\n");
        printf("[ESP32]   GET  /ping     — health check\n");
        return server;
    }

    printf("[ESP32] ERROR: no se pudo iniciar HTTP server\n");
    return NULL;
}

// ============== WiFi ==============

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < MAX_RETRY) {
            esp_wifi_connect();
            retry_num++;
            printf("[ESP32] Reintentando WiFi (%d/%d)...\n", retry_num, MAX_RETRY);
        } else {
            led_wifi_state = 2;  // blink lento: fallo
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            printf("[ESP32] ERROR: WiFi fallido tras %d intentos\n", MAX_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        led_wifi_state = 1;  // fijo ON: conectado
        printf("[ESP32] WiFi conectado — IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));
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

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t inst_any, inst_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &inst_any));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &inst_ip));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("[ESP32] Conectando a WiFi '%s'...\n", WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (!(bits & WIFI_CONNECTED_BIT))
        printf("[ESP32] ADVERTENCIA: sin WiFi — HTTP server no accesible\n");
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
    printf("[ESP32]  ESP32-C3-MINI-1 — DS9 Beamforming Bridge  \n");
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
