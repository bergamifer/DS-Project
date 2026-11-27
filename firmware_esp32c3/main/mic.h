/**
 * @file mic.h
 * @brief El micrófono del ESP32 (DS12): MSM261S4030H0R por I2S, DNP en la placa.
 *
 * Para qué: escuchar la obra sin tocar los buses de audio del STM32. Tres rutas:
 *   GET  /mic/nivel     nivel RMS y pico de los últimos 100 ms, y si hay micrófono
 *   GET  /mic/escuchar  ?s=10 — WAV de 24 kHz, 16 bits, para oír desde la central
 *   POST /mic/chequeo   arranca el chequeo de los 32 parlantes (≈34 s)
 *   GET  /mic/chequeo   el avance y el resultado, canal por canal
 *
 * El chequeo usa el modo 1 del STM32 (channel walker: 1 kHz en un canal por
 * segundo, empezando por ch0 al entrar al modo) y vuelve al modo en que estaba.
 * NO ENCIENDE AMPLIFICADORES: si están apagados, se niega y lo dice.
 *
 * Sólo arranca en una DS12 (lo pregunta al STM32: board_id del /status). En una
 * DS11 nunca configura el I2S, así que no maneja GPIO3/6/7.
 *
 * Las patas salen de DS12_MICROFONO_DNP.md, verificadas sobre el netlist.
 */

#ifndef MIC_H
#define MIC_H

#include <stdint.h>
#include "esp_http_server.h"

#define MIC_PIN_SCK   6   /* GPIO6 (U5 pata 20) — también J1.3, el JTAG externo */
#define MIC_PIN_WS    7   /* GPIO7 (U5 pata 21) — también J1.4 */
#define MIC_PIN_SD    3   /* GPIO3 (U5 pata 6), con 100 kΩ a GND en la placa */

/** Lo que el micrófono necesita de main.c (el transporte con el STM32). */
typedef struct {
  /** Manda un comando y espera ACK. 1 = ACK, 0 = NACK, -1 = timeout. */
  int (*enviar)(uint8_t cmd, const uint8_t *payload, uint16_t len, int timeout_ms);
  /** Pide el /status al STM32. 1 si contestó; llena placa (11/12), modo y amps. */
  int (*estado)(uint8_t *placa, uint8_t *modo, uint8_t *amps);
  uint8_t cmd_set_mode;
} mic_host_t;

/** Arranca la tarea del micrófono. No bloquea: espera a saber la placa. */
void mic_init(const mic_host_t *host);

esp_err_t mic_nivel_handler(httpd_req_t *req);
esp_err_t mic_escuchar_handler(httpd_req_t *req);
esp_err_t mic_chequeo_post_handler(httpd_req_t *req);
esp_err_t mic_chequeo_get_handler(httpd_req_t *req);

#endif /* MIC_H */
