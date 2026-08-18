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

/* Commands (ESP32 → STM32 unless noted) */
#define CMD_UPDATE_CFG      0x01
#define CMD_SET_MODE        0x02    /* payload: uint8 mode index (ui_states) */
#define CMD_GET_STATUS      0x03    /* payload: none (len=0) */
#define CMD_STATUS          0x04    /* STM32 → ESP32 response, ds_status_t */
#define CMD_SET_BEAM        0x07    /* payload: ds_beam_req_t (custom steer) */
#define CMD_ESP_INFO        0x08    /* ESP32 → STM32: IP en ASCII. NO se ACKea a
                                     * propósito: el ESP32 lo empuja sin pedir nada,
                                     * y un ACK espurio le daría el ack_sem a un
                                     * comando HTTP concurrente esperando el suyo. */
#define CMD_SET_VOLUME      0x09    /* payload: int8 dB (0..-63, ≤-64 = mute). Control
                                     * maestro sobre la atenuación del PCM1690 — afecta
                                     * TODOS los modos por igual (walker incluido). */
#define CMD_SET_AMPS        0x0A    /* payload: uint8 (1=on, 0=off). Enable del banco
                                     * de 32 PAM8302A (pin SD, PD3). Permite correr
                                     * todo el pipeline en silencio para aislar si un
                                     * reinicio es por demanda de los amplificadores. */
#define CMD_SET_INGAIN      0x0B    /* payload: uint8 dB (0..36). Ganancia de la
                                     * ETAPA DE ENTRADA, distinta del volumen (que
                                     * es el atenuador del DAC, a la salida). Con
                                     * fuentes de línea va en 0-12; +30 es para
                                     * micrófonos. Ver el comentario de in_gain_q8
                                     * en main.c: amplificar +30 y atenuar −40
                                     * después hacía recortar el clamp digital. */
#define CMD_ACK             0x06
#define CMD_NACK            0x15

/* Payload constraints */
#define PROTO_MAX_PAYLOAD   64
#define CFG_PAYLOAD_SIZE    52
#define CFG_NUM_CHANNELS    16
#define PROTO_VERSION       3   /* v2: + master_vol_db; v3: + in_gain_db y los
                                 * dos contadores de saturación en ds_status_t */
#define ESP_IP_MAXLEN       15      /* "255.255.255.255" */

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
 * @brief Custom beam request (CMD_SET_BEAM) — steer outside the K2 presets.
 * type 0 = RX beam steering (az_deg); type 1 = TX focal point (fx/fy/fz mm
 * relative to the speaker-array center, listener frame);
 * type 2 = MULTIFOCUS: az_deg lleva el ÍNDICE DE FUENTE (0..7) y fx/fy/fz el
 * foco de esa fuente (reuso del struct, sin cambio de tamaño). Aplica al modo
 * MULTIFOCUS activo: tonos (modo 14, fuentes 0-3 audibles) o entradas
 * analógicas (modo 15, fuentes 0-7); si no hay ninguno activo entra al 14.
 */
typedef struct __attribute__((packed)) {
    uint8_t type;
    int16_t az_deg;
    int16_t fx_mm, fy_mm, fz_mm;
} ds_beam_req_t;

_Static_assert(sizeof(ds_beam_req_t) == 9, "ds_beam_req_t must be 9 bytes");

/**
 * @brief Telemetry snapshot (CMD_STATUS response). Little-endian packed —
 * the ESP32 casts it directly. Extend only by APPENDING fields and bumping
 * PROTO_VERSION so old ESP32 firmware keeps parsing its known prefix.
 */
typedef struct __attribute__((packed)) {
    uint8_t  proto_ver;      /* PROTO_VERSION */
    uint8_t  mode_idx;       /* current ui_states index */
    uint8_t  num_modes;
    int16_t  rx_az_deg;      /* current RX beam azimuth */
    int16_t  fx_mm, fy_mm, fz_mm;  /* last TX focal point */
    uint32_t underruns;
    uint32_t sai_errors;
    uint8_t  ovf;            /* AK5538 overflow pin (1 = clipping) */
    uint8_t  cal_state;      /* AUTOFOCUS: 0 idle … 5 done (cal_state_t) */
    int32_t  adc_pk[8];      /* per-channel input peak, 24-bit scale */
    int8_t   master_vol_db;  /* v2: volumen maestro actual (0..-63, ≤-64 mute) */
    uint8_t  in_gain_db;     /* v3: ganancia de ENTRADA actual (0..36) */
    uint32_t adc_ovf_count;  /* v3: medios buffers con OVF alto, MONOTÓNICO */
    uint32_t out_clip_count; /* v3: muestras de salida recortadas, MONOTÓNICO */
} ds_status_t;               /* 54 + 1 + 4 + 4 = 63 bytes */

_Static_assert(sizeof(ds_status_t) == 63, "ds_status_t must be 63 bytes");
/* ⚠️ 63 de 64: el status ya casi llena PROTO_MAX_PAYLOAD. El próximo campo
 * obliga a subir ese límite EN AMBOS LADOS (acá y en firmware_esp32c3), o el
 * ESP32 descarta la trama por larga y el status se queda mudo sin error. */
_Static_assert(sizeof(ds_status_t) <= PROTO_MAX_PAYLOAD,
               "ds_status_t no entra en PROTO_MAX_PAYLOAD");

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

/**
 * @brief Poll for a CMD_SET_MODE request.
 * @return Requested mode index (≥0) once per request, -1 if none pending.
 */
int32_t UART_Protocol_GetModeRequest(void);

/**
 * @brief Poll for a CMD_SET_BEAM request.
 * @return 1 and fills *req once per request, 0 if none pending.
 */
uint8_t UART_Protocol_GetBeamRequest(ds_beam_req_t *req);

/**
 * @brief Poll for a CMD_GET_STATUS request (cleared on read).
 */
uint8_t UART_Protocol_StatusRequested(void);

/**
 * @brief Hook llamado mientras SendStatus espera al UART.
 *
 * Por defecto no hace nada (definición __weak). main.c la redefine para servir
 * los fills de audio pendientes. Sin eso, los ~5,2 ms que tarda la trama de
 * status bloquean el main loop — tolerable con el buffer de 480 muestras
 * (deadline 10 ms), inadmisible si se baja a 240 (deadline 5 ms).
 */
void UART_Protocol_Yield(void);

/**
 * @brief Send a CMD_STATUS frame to the ESP32.
 *
 * ~5,2 ms a 115200, pero NO bloquea el pipeline de audio: sirve los fills
 * pendientes entre byte y byte vía UART_Protocol_Yield(). Llamar SOLO desde el
 * main loop — nunca desde una ISR.
 */
void UART_Protocol_SendStatus(const ds_status_t *st);

/**
 * @brief IP del ESP32, reportada por CMD_ESP_INFO.
 * @return Cadena NUL-terminada; "" mientras no se haya recibido ninguna.
 */
const char* UART_Protocol_GetEspIp(void);

/**
 * @brief Poll for a CMD_SET_VOLUME request.
 * @param db  destino: dB pedidos (0..-63, ≤-64 = mute).
 * @return 1 y llena *db una vez por request, 0 si no hay ninguno pendiente.
 */
uint8_t UART_Protocol_GetVolumeRequest(int8_t *db);

/**
 * @brief Poll for a CMD_SET_INGAIN request.
 * @param db  destino: ganancia de entrada pedida en dB (0..36).
 * @return 1 y llena *db una vez por request, 0 si no hay ninguno pendiente.
 */
uint8_t UART_Protocol_GetInGainRequest(uint8_t *db);

/**
 * @brief Poll for a CMD_SET_AMPS request.
 * @param on  destino: 1 = habilitar los 32 amps, 0 = apagarlos.
 * @return 1 y llena *on una vez por request, 0 si no hay ninguno pendiente.
 */
uint8_t UART_Protocol_GetAmpsRequest(uint8_t *on);

#endif /* UART_PROTOCOL_H */
