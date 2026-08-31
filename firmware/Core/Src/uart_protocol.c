/**
 * @file    uart_protocol.c
 * @brief   Binary UART protocol parser for ESP32 -> STM32 communication
 *
 * Receives byte-by-byte via UART RX interrupt, parses binary frames,
 * stores config, sends ACK/NACK, and prints debug info to UART7.
 */

#include "uart_protocol.h"
#include <string.h>
#include <stdio.h>

/* Parser states */
typedef enum {
    WAIT_HEADER_1,
    WAIT_HEADER_2,
    WAIT_COMMAND,
    WAIT_LENGTH_L,
    WAIT_LENGTH_H,
    WAIT_PAYLOAD,
    WAIT_CHECKSUM,
    WAIT_FOOTER_1,
    WAIT_FOOTER_2
} parse_state_t;

/* Module state */
static UART_HandleTypeDef *huart_esp;   /* ESP32 UART (UART4) */
static UART_HandleTypeDef *huart_dbg;   /* Debug UART (UART7) */

static volatile parse_state_t parse_state = WAIT_HEADER_1;
static uint8_t  rx_byte;                /* Single byte for IT reception */
static uint8_t  rx_command;
static uint16_t rx_payload_len;
static uint8_t  rx_payload[PROTO_MAX_PAYLOAD];
static uint16_t rx_payload_idx;
static uint8_t  rx_checksum;

static ds_config_t current_config;
static volatile uint8_t new_config_flag = 0;
static volatile uint32_t rx_frame_count = 0;
static volatile uint32_t rx_error_count = 0;
static volatile uint32_t rx_byte_count = 0;  // total bytes received on UART4 RX

/* Remote-control requests (Fase 4): set by the RX ISR, polled by main loop */
static volatile uint8_t  mode_req_flag = 0;
static volatile uint8_t  mode_req_idx = 0;
static ds_beam_req_t     beam_req;
static volatile uint8_t  beam_req_flag = 0;
static volatile uint8_t  status_req_flag = 0;
static volatile uint8_t  selftest_req_flag = 0;  /* CMD_SELFTEST: el I2C es del main loop */

/* Captura remota del ADC. Igual que el status: el parser sólo levanta la
 * bandera, la respuesta la arma y la manda el main loop. */
static volatile uint8_t  cap_start_flag = 0;
static volatile uint16_t cap_start_n = 0;
static volatile uint8_t  cap_read_flag = 0;
static volatile uint32_t cap_read_off = 0;
static volatile uint8_t  cal_req_flag = 0;   /* CMD_GET_CAL: la trama la arma el main loop */
static volatile uint8_t  cal_req_first = 0;

/* IP del ESP32 (CMD_ESP_INFO). Escrita en contexto ISR, leída en el main loop:
 * el NUL se pone último, así un lector nunca ve una cadena sin terminar. */
static char esp_ip[ESP_IP_MAXLEN + 1] = {0};

/* CMD_SET_VOLUME: el ISR setea el flag+valor, el main loop hace la escritura I2C
 * (no se toca I2C desde el ISR). */
static volatile int8_t  vol_req_db = 0;
static volatile uint8_t vol_req_flag = 0;

/* CMD_SET_INGAIN: ganancia de la etapa de ENTRADA (distinta del volumen, que es
 * el atenuador del DAC a la SALIDA). El ISR setea flag+valor; el main loop la
 * aplica. Ver in_gain_q8 en main.c. */
static volatile uint8_t ingain_req_db = 0;
static volatile uint8_t ingain_req_flag = 0;

/* CMD_SET_AMPS: el ISR setea flag+valor, el main loop toca el GPIO. */
static volatile uint8_t amps_req_on = 0;
static volatile uint8_t amps_req_flag = 0;
/* CMD_FACTORY_RESET: sin payload. El trabajo (I2C del volumen, EEPROM) es del
 * main loop; el ISR sólo levanta la bandera. */
static volatile uint8_t factory_reset_flag = 0;

/* CMD_FW_* — un único pedido pendiente, sin ACK desde el ISR. Ver el comentario
 * de UART_Protocol_TakeFwRequest en el header: el ACK diferido ES el control de
 * flujo de la transferencia. `fw_req_cmd == 0` significa "libre". */
static volatile uint8_t  fw_req_cmd = 0;
static volatile uint16_t fw_req_len = 0;
static uint8_t           fw_req_payload[PROTO_MAX_PAYLOAD];
static volatile uint8_t  fw_abort_flag = 0;

/* Pre-formatted print buffer for non-blocking IT transmission */
/* Una línea por canal (~45 chars) + ~200 de encabezado y cierre. Con 32 canales
 * son ~1640, y el buffer estaba en 1200 desde que la config tenía 16 — pasar a
 * 32 lo desbordaba. El bucle de abajo además chequea el espacio restante, así
 * que un CFG_NUM_CHANNELS mayor trunca en vez de pisar memoria. */
#define PRINT_BUF_SIZE 2048
static char print_buf[PRINT_BUF_SIZE];

/* Forward declarations */
static void send_ack(void);
static void send_nack(void);
static void process_byte(uint8_t byte);
static void dbg_print(const char *str);

/* ============================================================ */

void UART_Protocol_Init(UART_HandleTypeDef *h_esp, UART_HandleTypeDef *h_dbg)
{
    huart_esp = h_esp;
    huart_dbg = h_dbg;

    memset(&current_config, 0, sizeof(current_config));
    parse_state = WAIT_HEADER_1;
    new_config_flag = 0;
    rx_frame_count = 0;
    rx_error_count = 0;

    /* Clear any overrun / error flags set during boot before arming RX */
    __HAL_UART_CLEAR_OREFLAG(huart_esp);
    __HAL_UART_CLEAR_FLAG(huart_esp, UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);

    /* Arm first byte reception */
    HAL_UART_Receive_IT(huart_esp, &rx_byte, 1);

    dbg_print("UART4: ESP32 link ready (115200 8N1)\r\n");
}

/* ===== Regla del módulo: NUNCA se entrega un puntero a memoria que escribe el
 * ISR. El que quiera los datos se lleva una COPIA, tomada con la interrupción
 * apagada.
 *
 * Antes esto era HasNewConfig() + GetConfig(), y el segundo devolvía un puntero
 * al buffer vivo. El main loop leía de ahí durante decenas de microsegundos —
 * 102 bytes de config, y encima PrintConfig() formateando 32 líneas— mientras el
 * ISR podía estar haciendo memcpy de una trama nueva ENCIMA. El resultado sería
 * una config mezclada de dos tramas: delays de unos canales y ganancias de
 * otros, todos en rango, ninguno el pedido. Improbable (hay que mandar dos
 * configs casi juntas) pero perfectamente posible desde la app o un script, y de
 * las que no se reproducen cuando uno las busca.
 *
 * La copia son ~100 ciclos con la IRQ apagada: irrelevante para el audio. */
uint8_t UART_Protocol_TakeConfig(ds_config_t *dst)
{
    if (!new_config_flag) return 0;
    __disable_irq();
    memcpy(dst, &current_config, sizeof(ds_config_t));
    new_config_flag = 0;
    __enable_irq();
    return 1;
}

void UART_Protocol_PrintConfig(const ds_config_t *cfg)
{
    int pos = 0;

    /* Format entire config dump into print_buf, then send non-blocking */
    pos += sprintf(&print_buf[pos], "\r\n========== CONFIG RECEIVED ==========\r\n");
    pos += sprintf(&print_buf[pos], "Frame #%lu (errors: %lu)\r\n", rx_frame_count, rx_error_count);

    uint32_t gain_pct = ((uint32_t)cfg->master_gain * 100) / 255;
    pos += sprintf(&print_buf[pos], "Master gain: %u (%lu%%)\r\n", cfg->master_gain, gain_pct);
    pos += sprintf(&print_buf[pos], "Global mute: %s\r\n", cfg->mute_global ? "YES" : "no");
    pos += sprintf(&print_buf[pos], "Mutes bitfield: 0x%08lX\r\n",
                   (unsigned long)cfg->mutes);
    pos += sprintf(&print_buf[pos], "--- Channels ---\r\n");

    for (uint8_t ch = 0; ch < CFG_NUM_CHANNELS; ch++) {
        /* 64 = holgura para la línea más larga posible. Truncar es preferible a
         * escribir fuera del buffer si alguien sube CFG_NUM_CHANNELS. */
        if (pos > PRINT_BUF_SIZE - 64) break;
        uint32_t ch_gain_pct = ((uint32_t)cfg->gains[ch] * 100) / 255;
        uint8_t ch_muted = (cfg->mutes >> ch) & 1;
        pos += sprintf(&print_buf[pos], "Ch%02u: delay=%5u  gain=%3u(%2lu%%)  mute=%u\r\n",
                ch, cfg->delays[ch], cfg->gains[ch], ch_gain_pct, ch_muted);
    }
    pos += sprintf(&print_buf[pos], "=====================================\r\n\r\n");

    /* Non-blocking: TX via interrupts, main loop continues immediately.
     * If previous IT transmission still in progress, HAL returns BUSY — acceptable. */
    HAL_UART_Transmit_IT(huart_dbg, (uint8_t*)print_buf, (uint16_t)pos);
}

/* ============================================================
 * Parser state machine (called from RX interrupt context)
 * ============================================================ */

static void process_byte(uint8_t byte)
{
    switch (parse_state) {

    case WAIT_HEADER_1:
        if (byte == PROTO_HEADER_1) parse_state = WAIT_HEADER_2;
        break;

    case WAIT_HEADER_2:
        parse_state = (byte == PROTO_HEADER_2) ? WAIT_COMMAND : WAIT_HEADER_1;
        break;

    case WAIT_COMMAND:
        rx_command = byte;
        parse_state = WAIT_LENGTH_L;
        break;

    case WAIT_LENGTH_L:
        rx_payload_len = byte;
        parse_state = WAIT_LENGTH_H;
        break;

    case WAIT_LENGTH_H:
        rx_payload_len |= ((uint16_t)byte << 8);
        rx_payload_idx = 0;
        if (rx_payload_len == 0) {
            parse_state = WAIT_CHECKSUM;   // e.g. CMD_GET_STATUS (checksum = 0)
        } else if (rx_payload_len <= PROTO_MAX_PAYLOAD) {
            parse_state = WAIT_PAYLOAD;
        } else {
            rx_error_count++;
            parse_state = WAIT_HEADER_1;
        }
        break;

    case WAIT_PAYLOAD:
        rx_payload[rx_payload_idx++] = byte;
        if (rx_payload_idx >= rx_payload_len) {
            parse_state = WAIT_CHECKSUM;
        }
        break;

    case WAIT_CHECKSUM:
        rx_checksum = byte;
        parse_state = WAIT_FOOTER_1;
        break;

    case WAIT_FOOTER_1:
        if (byte == PROTO_FOOTER_1) {
            parse_state = WAIT_FOOTER_2;
        } else {
            rx_error_count++;
            parse_state = WAIT_HEADER_1;
        }
        break;

    case WAIT_FOOTER_2:
        parse_state = WAIT_HEADER_1;
        if (byte != PROTO_FOOTER_2) {
            rx_error_count++;
            break;
        }

        /* Verify checksum (XOR of all payload bytes) */
        uint8_t calc_xor = 0;
        for (uint16_t i = 0; i < rx_payload_len; i++) {
            calc_xor ^= rx_payload[i];
        }

        if (calc_xor != rx_checksum) {
            rx_error_count++;
            send_nack();
            break;
        }

        /* Dispatch command */
        if (rx_command == CMD_UPDATE_CFG && rx_payload_len == CFG_PAYLOAD_SIZE) {
            memcpy(&current_config, rx_payload, sizeof(ds_config_t));
            rx_frame_count++;
            new_config_flag = 1;
            /* Note: PB0/PB1 are TIM3 PWM (AF2) on DS10 — GPIO toggle has no effect.
             * Frame activity is visible via rx_frame_count in the heartbeat. */
            send_ack();
        } else if (rx_command == CMD_SET_MODE && rx_payload_len == 1) {
            mode_req_idx = rx_payload[0];
            mode_req_flag = 1;
            rx_frame_count++;
            send_ack();
        } else if (rx_command == CMD_SET_BEAM && rx_payload_len == sizeof(ds_beam_req_t)) {
            memcpy(&beam_req, rx_payload, sizeof(ds_beam_req_t));
            beam_req_flag = 1;
            rx_frame_count++;
            send_ack();
        } else if (rx_command == CMD_CAP_START && rx_payload_len == 2) {
            cap_start_n = (uint16_t)rx_payload[0] | ((uint16_t)rx_payload[1] << 8);
            cap_start_flag = 1;
            rx_frame_count++;
            /* Sin ACK: la trama CMD_CAP_INFO es la respuesta, y llega recién
             * cuando la captura terminó de llenarse. */
        } else if (rx_command == CMD_CAP_READ && rx_payload_len == 4) {
            cap_read_off = (uint32_t)rx_payload[0]        | ((uint32_t)rx_payload[1] << 8)
                         | ((uint32_t)rx_payload[2] << 16) | ((uint32_t)rx_payload[3] << 24);
            cap_read_flag = 1;
            rx_frame_count++;
        } else if (rx_command == CMD_GET_CAL && rx_payload_len == 1) {
            cal_req_first = rx_payload[0];
            cal_req_flag = 1;
            rx_frame_count++;
            /* Sin ACK: la trama CMD_CAL_DATA es la respuesta, igual que el status */
        } else if (rx_command == CMD_SELFTEST && rx_payload_len == 0) {
            selftest_req_flag = 1;   /* el main loop lee I2C y manda CMD_HEALTH */
            rx_frame_count++;
            /* Sin ACK: la trama CMD_HEALTH es la respuesta, igual que el status */
        } else if (rx_command == CMD_GET_STATUS && rx_payload_len == 0) {
            status_req_flag = 1;   /* main loop builds + sends CMD_STATUS */
            rx_frame_count++;
            /* No ACK: the CMD_STATUS frame itself is the response */
        } else if (rx_command == CMD_ESP_INFO &&
                   rx_payload_len >= 7 && rx_payload_len <= ESP_IP_MAXLEN) {
            /* IP en ASCII, push del ESP32. Sin ACK (ver uart_protocol.h). */
            memcpy(esp_ip, rx_payload, rx_payload_len);
            esp_ip[rx_payload_len] = '\0';   /* NUL último: nunca se lee a medias */
            rx_frame_count++;
        } else if (rx_command == CMD_SET_INGAIN && rx_payload_len == 1) {
            ingain_req_db = rx_payload[0];        /* dB, 0..36 (clamp en main) */
            ingain_req_flag = 1;
            rx_frame_count++;
            send_ack();
        } else if (rx_command == CMD_FACTORY_RESET && rx_payload_len == 0) {
            factory_reset_flag = 1;               /* el main loop hace el trabajo */
            rx_frame_count++;
            send_ack();
        } else if (rx_command == CMD_SET_AMPS && rx_payload_len == 1) {
            amps_req_on = rx_payload[0] ? 1 : 0;
            amps_req_flag = 1;
            rx_frame_count++;
            send_ack();
        } else if (rx_command == CMD_SET_VOLUME && rx_payload_len == 1) {
            vol_req_db = (int8_t)rx_payload[0];   /* dB, negativo = atenúa */
            vol_req_flag = 1;
            rx_frame_count++;
            send_ack();
        } else if (rx_command == CMD_FW_BEGIN ||
                   rx_command == CMD_FW_DATA  ||
                   rx_command == CMD_FW_END) {
            /* Sin ACK acá: contesta el main loop cuando terminó con la flash.
             * Si todavía no contestó el pedido anterior, esto es el otro extremo
             * mandando sin esperar — se corta con un NACK antes de que la carga
             * se desordene. */
            if (fw_req_cmd != 0) {
                rx_error_count++;
                send_nack();
            } else {
                memcpy(fw_req_payload, rx_payload, rx_payload_len);
                fw_req_len = rx_payload_len;
                fw_req_cmd = rx_command;     /* último: publica el pedido ya completo */
                rx_frame_count++;
            }
        } else if (rx_command == CMD_FW_ABORT && rx_payload_len == 0) {
            fw_abort_flag = 1;               /* cancelar no escribe nada: ACK inmediato */
            rx_frame_count++;
            send_ack();
        } else {
            rx_error_count++;
            send_nack();
        }
        break;
    }
}

/* ============================================================
 * ACK / NACK responses
 * ============================================================ */

/* Default no-op; main.c la redefine para servir los fills de audio mientras
 * SendStatus espera al UART. Ver el comentario en UART_Protocol_SendStatus. */
__weak void UART_Protocol_Yield(void) { }

/* ===== Serialización del TX hacia el ESP32 =====
 *
 * Hay DOS escritores sobre el mismo UART4, en contextos distintos:
 *   - el main loop, que manda la trama de status escribiendo TDR byte a byte y
 *     sirviendo los fills de audio entre medio (cientos de ms en el modo 15);
 *   - el ISR del parser, que contesta ACK/NACK apenas termina de leer una trama.
 *
 * Sin coordinación, un comando que llegue mientras se manda un status mete sus
 * 5 bytes EN MEDIO de la trama. Consecuencias, las dos silenciosas:
 *   - el status queda corrupto → el ESP32 lo descarta → "stm32: timeout";
 *   - los bytes del ACK se los come el status → el ESP32 no ve su ACK → el POST
 *     contesta "timeout" aunque el STM32 haya ejecutado el comando.
 * Es exactamente el escenario de arrastrar el pad de MULTIFOCUS (un POST /beam
 * cada 150 ms) con la página polleando /status una vez por segundo.
 *
 * Solución: mientras el main loop está mandando una trama, el ISR ENCOLA en vez
 * de transmitir, y el main loop drena la cola al terminar. El ACK sale unos ms
 * más tarde, muy dentro de los 300-800 ms que el ESP32 espera. */
static volatile uint8_t tx_frame_busy = 0;   /* main loop escribiendo una trama */
static volatile uint8_t pending_ack   = 0;
static volatile uint8_t pending_nack  = 0;

/* 5 bytes = 434 µs a 115200. El timeout era de 50 ms: si el TX se trabara, el
 * ISR bloquearía 50 ms = diez deadlines de fill perdidos = un pop grande. Con
 * 5 ms (10× el tiempo real) el peor caso es un solo deadline. */
static void send_ack_now(void)
{
    uint8_t ack[] = {PROTO_HEADER_1, PROTO_HEADER_2, CMD_ACK, PROTO_FOOTER_1, PROTO_FOOTER_2};
    HAL_UART_Transmit(huart_esp, ack, sizeof(ack), 5);
}

static void send_nack_now(void)
{
    uint8_t nack[] = {PROTO_HEADER_1, PROTO_HEADER_2, CMD_NACK, PROTO_FOOTER_1, PROTO_FOOTER_2};
    HAL_UART_Transmit(huart_esp, nack, sizeof(nack), 5);
}

static void send_ack(void)
{
    if (tx_frame_busy) { pending_ack++; return; }
    send_ack_now();
}

static void send_nack(void)
{
    if (tx_frame_busy) { pending_nack++; return; }
    send_nack_now();
}

static void dbg_print(const char *str)
{
    HAL_UART_Transmit(huart_dbg, (uint8_t*)str, strlen(str), 100);
}

void UART_Protocol_CopyEspIp(char *dst, uint32_t dst_size)
{
    if (!dst || dst_size == 0) return;
    __disable_irq();
    uint32_t i = 0;
    while (i < dst_size - 1 && esp_ip[i]) { dst[i] = esp_ip[i]; i++; }
    dst[i] = '\0';
    __enable_irq();
}

uint8_t UART_Protocol_GetVolumeRequest(int8_t *db)
{
    if (!vol_req_flag) return 0;
    vol_req_flag = 0;
    *db = vol_req_db;
    return 1;
}

uint8_t UART_Protocol_GetInGainRequest(uint8_t *db)
{
    if (!ingain_req_flag) return 0;
    ingain_req_flag = 0;
    *db = ingain_req_db;
    return 1;
}

uint8_t UART_Protocol_GetAmpsRequest(uint8_t *on)
{
    if (!amps_req_flag) return 0;
    amps_req_flag = 0;
    *on = amps_req_on;
    return 1;
}

uint8_t UART_Protocol_GetFactoryResetRequest(void)
{
    if (!factory_reset_flag) return 0;
    factory_reset_flag = 0;
    return 1;
}

uint8_t UART_Protocol_TakeFwRequest(uint8_t *cmd, uint8_t *payload, uint16_t *len)
{
    if (!fw_req_cmd) return 0;
    /* Copia con la IRQ apagada, misma regla que TakeConfig: fw_req_payload lo
     * escribe el ISR. Acá el riesgo es menor (el ISR no acepta otro pedido
     * mientras éste esté sin contestar) pero la regla del módulo es la regla. */
    __disable_irq();
    *cmd = fw_req_cmd;
    *len = fw_req_len;
    memcpy(payload, fw_req_payload, fw_req_len);
    __enable_irq();
    return 1;
}

void UART_Protocol_FwReply(uint8_t ok)
{
    /* Liberar ANTES de contestar. Al revés hay una ventana —chica pero real— en
     * la que el otro extremo ya vio el ACK y manda el tramo siguiente mientras
     * el lugar sigue ocupado: el parser lo rechazaría y la carga se cortaría
     * sola en un punto al azar. */
    fw_req_cmd = 0;
    fw_req_len = 0;
    if (ok) send_ack();
    else    send_nack();
}

uint8_t UART_Protocol_GetFwAbort(void)
{
    if (!fw_abort_flag) return 0;
    fw_abort_flag = 0;
    return 1;
}

/* ============================================================
 * HAL UART RX Complete callback
 * Called when 1 byte received on any UART with active IT
 * ============================================================ */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == huart_esp->Instance) {
        rx_byte_count++;
        process_byte(rx_byte);
        /* Re-arm for next byte */
        HAL_UART_Receive_IT(huart_esp, &rx_byte, 1);
    }
}

/* ============================================================
 * HAL UART Error callback
 * Critical for link reliability: an overrun (ORE) — likely while the
 * main loop is doing a long blocking print or EEPROM save — aborts the
 * RX-IT chain. Without re-arming, the ESP32 link goes permanently deaf.
 * Clear the error flags, resync the parser, and re-arm reception.
 * ============================================================ */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == huart_esp->Instance) {
        __HAL_UART_CLEAR_OREFLAG(huart_esp);
        __HAL_UART_CLEAR_FLAG(huart_esp, UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
        rx_error_count++;
        parse_state = WAIT_HEADER_1;   /* drop partial frame, resync */
        /* Re-arm RX — HAL_UART_Receive_IT resets the RX state machine */
        HAL_UART_Receive_IT(huart_esp, &rx_byte, 1);
    }
}

uint32_t UART_Protocol_GetRxByteCount(void)
{
    return rx_byte_count;
}

/* ============================================================
 * Remote-control polling API (Fase 4) — main-loop side
 * ============================================================ */

int32_t UART_Protocol_GetModeRequest(void)
{
    if (mode_req_flag) {
        mode_req_flag = 0;
        return (int32_t)mode_req_idx;
    }
    return -1;
}

uint8_t UART_Protocol_GetBeamRequest(ds_beam_req_t *req)
{
    if (!beam_req_flag) return 0;
    /* Copia y limpieza del flag con la IRQ apagada: la trama siguiente puede
     * llegar justo en medio de la copia de estos 9 bytes y dejar un pedido
     * mezclado — por ejemplo el src de una fuente con el foco de otra. */
    __disable_irq();
    *req = beam_req;
    beam_req_flag = 0;
    __enable_irq();
    return 1;
}

uint8_t UART_Protocol_StatusRequested(void)
{
    if (status_req_flag) {
        status_req_flag = 0;
        return 1;
    }
    return 0;
}

uint8_t UART_Protocol_SelfTestRequested(void)
{
    if (selftest_req_flag) {
        selftest_req_flag = 0;
        return 1;
    }
    return 0;
}

/* El envio de una trama larga con servicio de fills entre byte y byte. Lo usan
 * SendStatus y SendHealth: es exactamente el mismo problema (decenas de bytes a
 * 115200 desde el main loop sin perder deadlines de audio) y no habia razon
 * para tener dos copias. */
static void send_frame_yielding(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    uint8_t frame[8 + PROTO_MAX_PAYLOAD];
    uint32_t fi = 0;
    frame[fi++] = PROTO_HEADER_1;
    frame[fi++] = PROTO_HEADER_2;
    frame[fi++] = cmd;
    frame[fi++] = len & 0xFF;
    frame[fi++] = (len >> 8) & 0xFF;
    memcpy(&frame[fi], payload, len);
    uint8_t x = 0;
    for (uint16_t i = 0; i < len; i++) x ^= frame[fi + i];
    fi += len;
    frame[fi++] = x;
    frame[fi++] = PROTO_FOOTER_1;
    frame[fi++] = PROTO_FOOTER_2;

    tx_frame_busy = 1;          /* el ISR encola sus ACK en vez de intercalarlos */
    for (uint16_t i = 0; i < fi; i++) {
        while (!(huart_esp->Instance->ISR & USART_ISR_TXE_TXFNF)) {
            UART_Protocol_Yield();
        }
        huart_esp->Instance->TDR = frame[i];
    }

    /* Drenar los ACK que llegaron durante la trama. La bandera se baja recién
     * cuando la cola quedó vacía, y eso se comprueba con la IRQ apagada: si se
     * bajara antes, un ACK que entrara justo en el medio del drenaje volvería a
     * intercalarse, que es el problema que esto viene a resolver. */
    for (;;) {
        __disable_irq();
        uint8_t na = pending_ack, nn = pending_nack;
        pending_ack = 0;
        pending_nack = 0;
        if (!na && !nn) { tx_frame_busy = 0; __enable_irq(); break; }
        __enable_irq();
        while (na--) send_ack_now();
        while (nn--) send_nack_now();
    }
}

void UART_Protocol_SendHealth(const ds_health_t *h)
{
    send_frame_yielding(CMD_HEALTH, (const uint8_t *)h, sizeof(ds_health_t));
}

uint8_t UART_Protocol_GetCapStart(uint16_t *n)
{
    if (!cap_start_flag) return 0;
    cap_start_flag = 0;
    *n = cap_start_n;
    return 1;
}

uint8_t UART_Protocol_GetCapRead(uint32_t *off)
{
    if (!cap_read_flag) return 0;
    cap_read_flag = 0;
    *off = cap_read_off;
    return 1;
}

void UART_Protocol_SendCapInfo(const ds_cap_info_t *info)
{
    send_frame_yielding(CMD_CAP_INFO, (const uint8_t *)info, sizeof(ds_cap_info_t));
}

void UART_Protocol_SendCapData(uint32_t offset, const uint8_t *data, uint16_t len)
{
    uint8_t pl[PROTO_MAX_PAYLOAD];
    if (len > CAP_CHUNK_BYTES) len = CAP_CHUNK_BYTES;
    pl[0] = (uint8_t)(offset);        pl[1] = (uint8_t)(offset >> 8);
    pl[2] = (uint8_t)(offset >> 16);  pl[3] = (uint8_t)(offset >> 24);
    memcpy(&pl[4], data, len);
    send_frame_yielding(CMD_CAP_DATA, pl, (uint16_t)(4 + len));
}

uint8_t UART_Protocol_GetCalReq(uint8_t *first)
{
    if (!cal_req_flag) return 0;
    cal_req_flag = 0;
    *first = cal_req_first;
    return 1;
}

void UART_Protocol_SendCalData(const ds_cal_hdr_t *hdr, const ds_cal_ch_t *chs)
{
    uint8_t pl[PROTO_MAX_PAYLOAD];
    uint8_t n = hdr->count;
    if (n > CAL_CHUNK_CH) n = CAL_CHUNK_CH;
    memcpy(pl, hdr, sizeof(ds_cal_hdr_t));
    /* Canal por canal y no de un memcpy: ds_cal_ch_t es packed, asi que el
     * arreglo del llamador ya viene contiguo, pero copiarlo con el tamano
     * declarado deja el bug de un cambio de struct fuera del alcance. */
    for (uint8_t i = 0; i < n; i++) {
        memcpy(&pl[sizeof(ds_cal_hdr_t) + i * sizeof(ds_cal_ch_t)],
               &chs[i], sizeof(ds_cal_ch_t));
    }
    send_frame_yielding(CMD_CAL_DATA, pl,
                        (uint16_t)(sizeof(ds_cal_hdr_t) + n * sizeof(ds_cal_ch_t)));
}

void UART_Protocol_SendStatus(const ds_status_t *st)
{
    /* ~5,2 ms a 115200, pero sin bloquear el pipeline: send_frame_yielding
     * sirve los fills de audio entre byte y byte. Ver el comentario del
     * helper y el del header. */
    send_frame_yielding(CMD_STATUS, (const uint8_t *)st, sizeof(ds_status_t));
}
