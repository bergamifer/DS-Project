/**
 * @file    uart_protocol.h
 * @brief   Binary UART protocol for ESP32-S2 Mini -> STM32H743 communication
 *
 * Protocol: Header(2) + Cmd(1) + Length(2,LE) + Payload(N) + Checksum(1) + Footer(2)
 * See uart-stm32-protocol.md for full specification.
 */

#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

/* Protocol framing */
#define PROTO_HEADER_1      0xAA
#define PROTO_HEADER_2      0x55
#define PROTO_FOOTER_1      0x0D
#define PROTO_FOOTER_2      0x0A

/* Commands */
#define CMD_UPDATE_CFG      0x01
#define CMD_ACK             0x06
#define CMD_NACK            0x15

/* Payload constraints */
#define PROTO_MAX_PAYLOAD   64
#define CFG_PAYLOAD_SIZE    52
#define CFG_NUM_CHANNELS    16

/**
 * @brief Device configuration received from ESP32
 *
 * 52 bytes, little-endian, packed — can be cast directly from payload
 * since both STM32 (Cortex-M7) and ESP32 (Xtensa/RISC-V) are LE.
 */
typedef struct __attribute__((packed)) {
    uint8_t  master_gain;       /* 0-255 maps to 0.0-1.0 */
    uint8_t  mute_global;       /* 0=unmuted, 1=muted */
    uint16_t delays[CFG_NUM_CHANNELS];  /* delay_samples per channel (LE) */
    uint8_t  gains[CFG_NUM_CHANNELS];   /* gain per channel (0-255) */
    uint16_t mutes;             /* bitfield: bit N = channel N muted */
} ds_config_t;

_Static_assert(sizeof(ds_config_t) == CFG_PAYLOAD_SIZE,
               "ds_config_t must be exactly 52 bytes");

/**
 * @brief Initialize UART protocol module
 * @param huart_esp  UART handle for ESP32 communication (UART4)
 * @param huart_dbg  UART handle for debug output (UART7)
 */
void UART_Protocol_Init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_dbg);

/**
 * @brief Check if a new valid config has been received
 * @return 1 if new config available (clears flag), 0 otherwise
 */
uint8_t UART_Protocol_HasNewConfig(void);

/**
 * @brief Get pointer to the current received config
 * @return Pointer to ds_config_t (valid after HasNewConfig returns 1)
 */
const ds_config_t* UART_Protocol_GetConfig(void);

/**
 * @brief Print received config to debug UART (minicom)
 */
void UART_Protocol_PrintConfig(void);

/**
 * @brief Get total bytes received on UART4 RX (diagnostic)
 */
uint32_t UART_Protocol_GetRxByteCount(void);

#endif /* UART_PROTOCOL_H */
