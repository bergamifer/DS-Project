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
#define CMD_FACTORY_RESET   0x0C    /* payload: none. Vuelve TODO a fábrica —
                                     * volumen, ganancia de entrada, modo,
                                     * gains/mutes/delays y los focos de ambos
                                     * MULTIFOCUS— y lo persiste. Salida de
                                     * emergencia cuando el instrumento quedó
                                     * en un estado raro a fuerza de ajustes
                                     * en vivo, varios de los cuales se
                                     * guardan solos. */
#define CMD_ACK             0x06
#define CMD_NACK            0x15

/* Payload constraints */
/* Techo del payload. Era 64 y el status ya ocupaba 63 — un campo más y el ESP32
 * empezaba a descartar la trama por larga, sin error visible. 128 deja lugar
 * para el status extendido y para una config de los 32 canales (102 bytes).
 * ⚠️ Cambiarlo obliga a cambiarlo EN AMBOS LADOS (firmware_esp32c3). */
#define PROTO_MAX_PAYLOAD   128
/* Los 32 canales de DS11. Era 16 —la mitad del array— así que gains/mutes/delays
 * de las filas 3 y 4 no se podían tocar desde el ESP32. */
#define CFG_PAYLOAD_SIZE    102
#define CFG_NUM_CHANNELS    32
#define PROTO_VERSION       7   /* v2: + master_vol_db; v3: + in_gain_db y los
                                 * dos contadores de saturación; v4: +
                                 * state_flags, y ds_config_t pasa a 32 canales;
                                 * v5: + uptime_ms DEL STM32;
                                 * v6: + cpu_fills_x10 — el presupuesto de CPU
                                 * del pipeline sólo se veía por el serie, así
                                 * que diagnosticar "este modo no entra" pedía
                                 * un cable. Ahora sale por /status.
                                 * v7: + mic_live_mask / cal_progress /
                                 * cal_ok_count — el AUTOFOCUS se puede correr
                                 * y juzgar desde la página, sin serie. */
#define ESP_IP_MAXLEN       15      /* "255.255.255.255" */

/**
 * @brief Device configuration received from ESP32
 *
 * 102 bytes, little-endian, packed — can be cast directly from payload
 * since both STM32 (Cortex-M7) and ESP32 (Xtensa/RISC-V) are LE.
 */
typedef struct __attribute__((packed)) {
    uint8_t  master_gain;       /* 0-255 maps to 0.0-1.0 */
    uint8_t  mute_global;       /* 0=unmuted, 1=muted */
    uint16_t delays[CFG_NUM_CHANNELS];  /* delay_samples per channel (LE) */
    uint8_t  gains[CFG_NUM_CHANNELS];   /* gain per channel (0-255) */
    uint32_t mutes;             /* bitfield: bit N = channel N muted. 32 bits
                                 * porque con 16 no se podía silenciar más allá
                                 * del canal 15 — la mitad del array. */
} ds_config_t;

_Static_assert(sizeof(ds_config_t) == CFG_PAYLOAD_SIZE,
               "ds_config_t must be exactly CFG_PAYLOAD_SIZE bytes");

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
    uint8_t  state_flags;    /* v4: ver STATE_F_* */
    uint32_t uptime_ms;      /* v5: uptime del STM32 (HAL_GetTick) */
    uint16_t cpu_fills_x10;  /* v6: % de CPU en los 4 fills, ×10 (263 = 26,3%) */
    /* v7 — los tres existen para que el AUTOFOCUS se pueda correr SIN una
     * terminal serie abierta. Hasta ahora el scan sólo se contaba por UART:
     * desde la página no había forma de saber si el micrófono estaba vivo, por
     * qué parlante iba, ni si el resultado servía. */
    uint8_t  mic_live_mask;  /* v7: bit N = entrada N con señal (no sólo ruido) */
    uint8_t  cal_progress;   /* v7: parlantes ya medidos, 0..CAL_NUM_CH */
    uint8_t  cal_ok_count;   /* v7: de esos, cuántos con calidad suficiente */
} ds_status_t;               /* 70 + 3 = 73 bytes */

/* Estado REAL, no el nominal.
 *
 * STATE_F_CUSTOM existe porque `mode_idx` puede mentir: UI_ApplyCustomBeam
 * (POST /beam) cambia lo que suena sin tocar ui_state_idx, a propósito, para
 * que el próximo K2 retome el ciclo donde estaba. La consecuencia es que la
 * página puede mostrar "modo 13" mientras la placa hace otra cosa — eso costó
 * una sesión entera de debug el 2026-08-18. Con este bit la UI dice la verdad.
 * Lo limpia UI_ApplyState, o sea elegir un modo.
 *
 * STATE_F_AMPS: la UI no tenía forma de saber si los amps están encendidos —
 * sólo recordaba el último botón apretado, y eso se vuelve falso con un reboot
 * o con el K2 largo. */
#define STATE_F_AMPS    0x01    /* banco de 32 PAM8302A habilitado */
#define STATE_F_CUSTOM  0x02    /* beam custom: mode_idx NO describe lo que suena */

/* El `uptime_ms` que publicaba /status era el del ESP32, no el del STM32: un
 * reinicio del STM32 era INVISIBLE desde la red. Se descubrió el 2026-08-18
 * midiendo underruns, cuando el contador BAJÓ de 6 a 0 — un contador monótono
 * que retrocede es un reboot, y no había con qué confirmarlo. */
_Static_assert(sizeof(ds_status_t) == 73, "ds_status_t must be 73 bytes");
/* ⚠️ Un payload por encima de PROTO_MAX_PAYLOAD hace que el ESP32 descarte la
 * trama por larga y el endpoint quede mudo SIN dar error — de ahí los asserts. */
_Static_assert(sizeof(ds_status_t) <= PROTO_MAX_PAYLOAD,
               "ds_status_t no entra en PROTO_MAX_PAYLOAD");
_Static_assert(CFG_PAYLOAD_SIZE <= PROTO_MAX_PAYLOAD,
               "ds_config_t no entra en PROTO_MAX_PAYLOAD");

/**
 * @brief Initialize UART protocol module
 * @param huart_esp  UART handle for ESP32 communication (UART4)
 * @param huart_dbg  UART handle for debug output (UART7)
 */
void UART_Protocol_Init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_dbg);

/**
 * @brief Se lleva una COPIA de la última config recibida, si hay una nueva.
 * @param dst  destino; se llena sólo si la función devuelve 1.
 * @return 1 una vez por config nueva, 0 si no hay ninguna pendiente.
 *
 * ⚠️ REGLA DEL MÓDULO: nada devuelve un puntero a memoria que escribe el ISR.
 * Reemplaza al par HasNewConfig()/GetConfig(), donde el segundo entregaba el
 * buffer vivo y el main loop leía de ahí mientras el ISR podía estar
 * escribiéndole encima una trama nueva. La copia va con la IRQ apagada.
 */
uint8_t UART_Protocol_TakeConfig(ds_config_t *dst);

/**
 * @brief Print a config snapshot to debug UART (minicom).
 * @param cfg  la copia obtenida con UART_Protocol_TakeConfig().
 */
void UART_Protocol_PrintConfig(const ds_config_t *cfg);

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
 * @brief Copia la IP del ESP32 (CMD_ESP_INFO) al buffer del llamador.
 * @param dst       destino; queda NUL-terminado siempre.
 * @param dst_size  tamaño de dst (≥ ESP_IP_MAXLEN+1 para no truncar).
 *
 * Copia en vez de devolver el puntero, por la misma regla que TakeConfig: la
 * cadena la escribe el ISR y una trama nueva a mitad de lectura mezclaría dos
 * direcciones. Queda "" mientras no se haya recibido ninguna.
 */
void UART_Protocol_CopyEspIp(char *dst, uint32_t dst_size);

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

/**
 * @brief Poll for a CMD_FACTORY_RESET request.
 * @return 1 una vez por request, 0 si no hay ninguno pendiente.
 */
uint8_t UART_Protocol_GetFactoryResetRequest(void);

#endif /* UART_PROTOCOL_H */
