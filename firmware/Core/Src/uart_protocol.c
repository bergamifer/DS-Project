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

/* Pre-formatted print buffer for non-blocking IT transmission */
#define PRINT_BUF_SIZE 1200
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

uint8_t UART_Protocol_HasNewConfig(void)
{
    if (new_config_flag) {
        new_config_flag = 0;
        return 1;
    }
    return 0;
}

const ds_config_t* UART_Protocol_GetConfig(void)
{
    return &current_config;
}

void UART_Protocol_PrintConfig(void)
{
    const ds_config_t *cfg = &current_config;
    int pos = 0;

    /* Format entire config dump into print_buf, then send non-blocking */
    pos += sprintf(&print_buf[pos], "\r\n========== CONFIG RECEIVED ==========\r\n");
    pos += sprintf(&print_buf[pos], "Frame #%lu (errors: %lu)\r\n", rx_frame_count, rx_error_count);

    uint32_t gain_pct = ((uint32_t)cfg->master_gain * 100) / 255;
    pos += sprintf(&print_buf[pos], "Master gain: %u (%lu%%)\r\n", cfg->master_gain, gain_pct);
    pos += sprintf(&print_buf[pos], "Global mute: %s\r\n", cfg->mute_global ? "YES" : "no");
    pos += sprintf(&print_buf[pos], "Mutes bitfield: 0x%04X\r\n", cfg->mutes);
    pos += sprintf(&print_buf[pos], "--- Channels ---\r\n");

    for (uint8_t ch = 0; ch < CFG_NUM_CHANNELS; ch++) {
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
        } else if (rx_command == CMD_GET_STATUS && rx_payload_len == 0) {
            status_req_flag = 1;   /* main loop builds + sends CMD_STATUS */
            rx_frame_count++;
            /* No ACK: the CMD_STATUS frame itself is the response */
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

static void send_ack(void)
{
    uint8_t ack[] = {PROTO_HEADER_1, PROTO_HEADER_2, CMD_ACK, PROTO_FOOTER_1, PROTO_FOOTER_2};
    HAL_UART_Transmit(huart_esp, ack, sizeof(ack), 50);
}

static void send_nack(void)
{
    uint8_t nack[] = {PROTO_HEADER_1, PROTO_HEADER_2, CMD_NACK, PROTO_FOOTER_1, PROTO_FOOTER_2};
    HAL_UART_Transmit(huart_esp, nack, sizeof(nack), 50);
}

static void dbg_print(const char *str)
{
    HAL_UART_Transmit(huart_dbg, (uint8_t*)str, strlen(str), 100);
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
    if (beam_req_flag) {
        beam_req_flag = 0;
        *req = beam_req;    /* struct copy; ISR only re-writes on next frame */
        return 1;
    }
    return 0;
}

uint8_t UART_Protocol_StatusRequested(void)
{
    if (status_req_flag) {
        status_req_flag = 0;
        return 1;
    }
    return 0;
}

void UART_Protocol_SendStatus(const ds_status_t *st)
{
    /* Full framing (header + cmd + len + payload + checksum + footer) so the
     * ESP32 parses it with the same state machine as any frame. */
    uint8_t frame[8 + sizeof(ds_status_t)];   /* hdr2+cmd1+len2+payload+ck1+ftr2 */
    uint16_t len = sizeof(ds_status_t);
    uint32_t fi = 0;
    frame[fi++] = PROTO_HEADER_1;
    frame[fi++] = PROTO_HEADER_2;
    frame[fi++] = CMD_STATUS;
    frame[fi++] = len & 0xFF;
    frame[fi++] = (len >> 8) & 0xFF;
    memcpy(&frame[fi], st, len);
    uint8_t x = 0;
    for (uint16_t i = 0; i < len; i++) x ^= frame[fi + i];
    fi += len;
    frame[fi++] = x;
    frame[fi++] = PROTO_FOOTER_1;
    frame[fi++] = PROTO_FOOTER_2;
    HAL_UART_Transmit(huart_esp, frame, fi, 50);
}
