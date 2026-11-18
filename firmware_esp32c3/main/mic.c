/**
 * @file mic.c
 * @brief El micrófono del ESP32. Ver mic.h para qué hace y mic_dsp.h para cómo decide.
 *
 * UNA SOLA TAREA lee el I2S (bloques de 10 ms), y con cada bloque:
 *   1. actualiza el nivel de los últimos 100 ms (lo que lee /mic/nivel);
 *   2. lo pasa a 16 bits a un anillo de 0,5 s (lo que lee /mic/escuchar);
 *   3. si hay un chequeo corriendo, alimenta la ventana del canal que toca.
 * Los handlers HTTP sólo leen: nunca tocan el I2S.
 *
 * SIN PROBAR EN UNA PLACA: todavía no hay DS12. Lo que sí está probado son las
 * cuentas (test/test_mic_dsp.c) y que compila.
 */

#include "mic.h"
#include "mic_dsp.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/i2s_std.h"

#define BLOQUE        240u                 /* 10 ms a 24 kHz */
#define ANILLO        12000u               /* 0,5 s de audio en 16 bits = 24 kB */
#define N_CANALES     32u                  /* WALKER_NUM_CH del STM32 */
#define MODO_WALKER   1u
#define SLOT_US       1000000LL            /* WALKER_SECONDS_PER_CH = 1 s */
#define VENT_INI_US   250000LL             /* se mira de 0,25 a 0,85 s de cada slot: */
#define VENT_FIN_US   850000LL             /* el margen absorbe la latencia UART+DMA */

static mic_host_t host;
static i2s_chan_handle_t rx;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

/* --- Estado que leen los handlers (protegido por mux) --- */
static volatile int  activo = 0;           /* 1 = I2S andando */
static const char   *motivo_inactivo = "arrancando: todavia no se sabe la placa";
static float  nivel_rms = -200.0f, nivel_pico = -200.0f;
static int    hay_senal = 0;               /* el ultimo bloque de 100 ms vario */
static uint32_t bloques = 0;

static int16_t anillo[ANILLO];
static volatile uint32_t escrito = 0;      /* muestras escritas desde el arranque */

/* --- El chequeo --- */
typedef enum { CH_LIBRE = 0, CH_CORRIENDO, CH_TERMINADO, CH_ERROR } ch_estado_t;
static volatile ch_estado_t ch_estado = CH_LIBRE;
static char    ch_error[96] = "";
static int64_t ch_t0 = 0;                  /* cuando el STM32 acepto el modo 1 */
static uint8_t ch_modo_previo = 0;
static mic_ventana_t ch_ventana;
static int     ch_canal = -1;              /* canal cuya ventana se esta llenando */
static mic_resultado_t ch_res[N_CANALES];
static uint8_t ch_hecho[N_CANALES];

/* ===================== La tarea ===================== */

static int i2s_arrancar(void) {
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  if (i2s_new_channel(&cc, NULL, &rx) != ESP_OK) return 0;
  i2s_std_config_t sc = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(MIC_FS_HZ),
    /* Philips, 32 bits por ranura: el micrófono saca 24 bits alineados a la
     * izquierda, con un bit de retardo después de WS (hoja, figura de timing). */
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = MIC_PIN_SCK,
      .ws   = MIC_PIN_WS,
      .dout = I2S_GPIO_UNUSED,
      .din  = MIC_PIN_SD,
      .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
    },
  };
  sc.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;   /* L/R del micrófono a GND */
  if (i2s_channel_init_std_mode(rx, &sc) != ESP_OK) return 0;
  return i2s_channel_enable(rx) == ESP_OK;
}

/* Alimenta el chequeo con una muestra tomada en el instante t (µs). */
static void chequeo_muestra(int32_t x, int64_t t) {
  int64_t dt = t - ch_t0;
  if (dt < 0) return;
  int canal = (int)(dt / SLOT_US);
  int64_t en_slot = dt % SLOT_US;

  /* Se cerró la ventana del canal anterior: guardar el veredicto. */
  if (ch_canal >= 0 && (canal != ch_canal || en_slot >= VENT_FIN_US)) {
    ch_res[ch_canal] = mic_ventana_veredicto(&ch_ventana);
    ch_hecho[ch_canal] = 1;
    ch_canal = -1;
  }
  if (canal >= (int)N_CANALES) {
    /* Terminó la vuelta: volver al modo en que estaba. */
    uint8_t m = ch_modo_previo;
    host.enviar(host.cmd_set_mode, &m, 1, 300);
    ch_estado = CH_TERMINADO;
    printf("[MIC] chequeo terminado; de vuelta al modo %u\n", (unsigned)m);
    return;
  }
  if (en_slot >= VENT_INI_US && en_slot < VENT_FIN_US) {
    if (ch_canal < 0) { mic_ventana_init(&ch_ventana, (float)MIC_FS_HZ); ch_canal = canal; }
    mic_ventana_push(&ch_ventana, x);
  }
}

static void mic_task(void *arg) {
  (void)arg;
  /* 1. ¿Qué placa es? El STM32 tarda en arrancar: preguntar hasta que conteste. */
  uint8_t placa = 0, modo, amps;
  while (!host.estado(&placa, &modo, &amps)) vTaskDelay(pdMS_TO_TICKS(2000));
  if (placa != 12) {
    motivo_inactivo = "placa DS11: no tiene microfono en el ESP32 (GPIO3/6/7 no se tocan)";
    printf("[MIC] placa DS%u: el microfono del ESP32 queda apagado\n", (unsigned)placa);
    vTaskDelete(NULL);
    return;
  }
  if (!i2s_arrancar()) {
    motivo_inactivo = "no se pudo configurar el I2S";
    printf("[MIC] !! no se pudo configurar el I2S\n");
    vTaskDelete(NULL);
    return;
  }
  activo = 1;
  printf("[MIC] I2S andando: SCK=GPIO%d WS=GPIO%d SD=GPIO%d, %u Hz\n",
         MIC_PIN_SCK, MIC_PIN_WS, MIC_PIN_SD, (unsigned)MIC_FS_HZ);

  static int32_t buf[BLOQUE];
  double suma = 0; uint32_t n = 0; int32_t mn = INT32_MAX, mx = INT32_MIN, pico = 0;
  int64_t t_bloque = esp_timer_get_time();

  for (;;) {
    size_t leidos = 0;
    if (i2s_channel_read(rx, buf, sizeof(buf), &leidos, pdMS_TO_TICKS(100)) != ESP_OK || leidos == 0)
      continue;
    uint32_t cuantas = leidos / sizeof(int32_t);
    int64_t t_fin = esp_timer_get_time();
    int64_t t_ini = t_fin - (int64_t)cuantas * 1000000LL / MIC_FS_HZ;

    for (uint32_t i = 0; i < cuantas; i++) {
      int32_t x = buf[i] >> 8;                       /* 24 bits con signo */
      suma += (double)x * x; n++;
      if (x < mn) mn = x;
      if (x > mx) mx = x;
      int32_t ax = x < 0 ? -x : x;
      if (ax > pico) pico = ax;
      anillo[(escrito + i) % ANILLO] = (int16_t)(x >> 8);
      if (ch_estado == CH_CORRIENDO)
        chequeo_muestra(x, t_ini + (int64_t)i * 1000000LL / MIC_FS_HZ);
    }
    escrito += cuantas;

    if (t_fin - t_bloque >= 100000) {                /* cada 100 ms */
      float r = mic_rms_dbfs(suma, n);
      float p = pico > 0 ? mic_rms_dbfs((double)pico * pico, 1) : -200.0f;
      portENTER_CRITICAL(&mux);
      nivel_rms = r; nivel_pico = p; hay_senal = (mn != mx); bloques++;
      portEXIT_CRITICAL(&mux);
      suma = 0; n = 0; mn = INT32_MAX; mx = INT32_MIN; pico = 0;
      t_bloque = t_fin;
    }
  }
}

void mic_init(const mic_host_t *h) {
  host = *h;
  xTaskCreate(mic_task, "mic", 4096, NULL, 4, NULL);
}

/* ===================== HTTP ===================== */

static esp_err_t json(httpd_req_t *req, const char *s) {
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_sendstr(req, s);
}

esp_err_t mic_nivel_handler(httpd_req_t *req) {
  char b[256];
  if (!activo) {
    snprintf(b, sizeof(b), "{\"activo\":false,\"motivo\":\"%s\"}", motivo_inactivo);
    return json(req, b);
  }
  portENTER_CRITICAL(&mux);
  float r = nivel_rms, p = nivel_pico; int s = hay_senal; uint32_t k = bloques;
  portEXIT_CRITICAL(&mux);
  /* Sin señal = muestras todas iguales: es el pull-down de 100 kΩ sin el
   * micrófono soldado, no "silencio". Un micrófono real siempre tiene ruido. */
  snprintf(b, sizeof(b),
           "{\"activo\":true,\"microfono\":%s,\"rms_dbfs\":%.1f,\"pico_dbfs\":%.1f,"
           "\"fs_hz\":%u,\"bloques\":%lu}",
           s ? "true" : "false", r, p, (unsigned)MIC_FS_HZ, (unsigned long)k);
  return json(req, b);
}

static void wav_cabecera(uint8_t *h) {
  /* Largo desconocido (streaming): 0xFFFFFFFF, que VLC y los navegadores aceptan. */
  const uint32_t fs = MIC_FS_HZ, br = MIC_FS_HZ * 2u, largo = 0xFFFFFFFFu;
  memcpy(h, "RIFF", 4); memcpy(h + 4, &largo, 4); memcpy(h + 8, "WAVEfmt ", 8);
  uint32_t v32 = 16; memcpy(h + 16, &v32, 4);
  uint16_t v16 = 1; memcpy(h + 20, &v16, 2);            /* PCM */
  v16 = 1; memcpy(h + 22, &v16, 2);                     /* mono */
  memcpy(h + 24, &fs, 4); memcpy(h + 28, &br, 4);
  v16 = 2; memcpy(h + 32, &v16, 2);
  v16 = 16; memcpy(h + 34, &v16, 2);
  memcpy(h + 36, "data", 4); memcpy(h + 40, &largo, 4);
}

esp_err_t mic_escuchar_handler(httpd_req_t *req) {
  if (!activo) return json(req, "{\"error\":\"el microfono no esta activo: ver GET /mic/nivel\"}");
  int seg = 10;
  char q[32], v[8];
  if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK &&
      httpd_query_key_value(q, "s", v, sizeof(v)) == ESP_OK)
    seg = atoi(v);
  /* Tope de 30 s: el servidor HTTP tiene UNA tarea, y mientras manda audio
   * /status y la página esperan. */
  if (seg < 1) seg = 1;
  if (seg > 30) seg = 30;

  httpd_resp_set_type(req, "audio/wav");
  uint8_t h[44];
  wav_cabecera(h);
  if (httpd_resp_send_chunk(req, (const char *)h, sizeof(h)) != ESP_OK) return ESP_FAIL;

  static int16_t trozo[1200];                            /* 50 ms */
  uint32_t leido = escrito, total = (uint32_t)seg * MIC_FS_HZ, mandado = 0;
  while (mandado < total) {
    uint32_t hay = escrito - leido;
    if (hay > ANILLO - 1200) { leido = escrito - 1200; hay = 1200; }  /* se atrasó: saltar */
    if (hay < 1200) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
    for (uint32_t i = 0; i < 1200; i++) trozo[i] = anillo[(leido + i) % ANILLO];
    leido += 1200; mandado += 1200;
    if (httpd_resp_send_chunk(req, (const char *)trozo, sizeof(trozo)) != ESP_OK) return ESP_FAIL;
  }
  return httpd_resp_send_chunk(req, NULL, 0);
}

esp_err_t mic_chequeo_post_handler(httpd_req_t *req) {
  char b[200];
  if (!activo) return json(req, "{\"ok\":false,\"error\":\"el microfono no esta activo: ver GET /mic/nivel\"}");
  if (ch_estado == CH_CORRIENDO) return json(req, "{\"ok\":false,\"error\":\"ya hay un chequeo corriendo\"}");

  uint8_t placa, modo, amps;
  if (!host.estado(&placa, &modo, &amps))
    return json(req, "{\"ok\":false,\"error\":\"el STM32 no contesta el status\"}");
  if (!amps)
    /* No se encienden desde acá: encender 32 amplificadores lo decide una persona. */
    return json(req, "{\"ok\":false,\"error\":\"los amplificadores estan apagados; "
                     "encenderlos es decision de una persona (POST /amps)\"}");

  memset(ch_hecho, 0, sizeof(ch_hecho));
  ch_canal = -1;
  ch_modo_previo = modo;
  uint8_t m = MODO_WALKER;
  int ack = host.enviar(host.cmd_set_mode, &m, 1, 300);
  if (ack != 1) {
    snprintf(ch_error, sizeof(ch_error), "el STM32 no acepto el modo 1 (%s)", ack == 0 ? "nack" : "timeout");
    ch_estado = CH_ERROR;
    snprintf(b, sizeof(b), "{\"ok\":false,\"error\":\"%s\"}", ch_error);
    return json(req, b);
  }
  /* El walker arranca en ch0 al entrar al modo (walker_frame = 0 en el STM32). */
  ch_t0 = esp_timer_get_time();
  ch_estado = CH_CORRIENDO;
  printf("[MIC] chequeo: modo %u -> 1 (walker), %u canales\n", (unsigned)modo, (unsigned)N_CANALES);
  snprintf(b, sizeof(b), "{\"ok\":true,\"segundos\":%u,\"modo_previo\":%u}",
           (unsigned)N_CANALES + 1u, (unsigned)modo);
  return json(req, b);
}

esp_err_t mic_chequeo_get_handler(httpd_req_t *req) {
  static char b[3072];
  const char *est = ch_estado == CH_LIBRE ? "nunca corrio" : ch_estado == CH_CORRIENDO ? "corriendo"
                  : ch_estado == CH_TERMINADO ? "terminado" : "error";
  int p = snprintf(b, sizeof(b), "{\"estado\":\"%s\"", est);
  if (ch_estado == CH_ERROR) p += snprintf(b + p, sizeof(b) - p, ",\"error\":\"%s\"", ch_error);
  p += snprintf(b + p, sizeof(b) - p,
                ",\"criterio\":{\"tono_hz\":%.0f,\"margen_db\":%.0f,\"piso_dbfs\":%.0f},\"canales\":[",
                MIC_TONO_HZ, MIC_MARGEN_REL_DB, MIC_PISO_ABS_DBFS);
  int suenan = 0, medidos = 0;
  for (unsigned c = 0; c < N_CANALES && p < (int)sizeof(b) - 120; c++) {
    if (!ch_hecho[c]) continue;
    medidos++;
    if (ch_res[c].veredicto == MIC_SUENA) suenan++;
    p += snprintf(b + p, sizeof(b) - p,
                  "%s{\"ch\":%u,\"tono_dbfs\":%.1f,\"vecina_dbfs\":%.1f,\"veredicto\":\"%s\"}",
                  medidos > 1 ? "," : "", c, ch_res[c].tono_dbfs, ch_res[c].vecina_dbfs,
                  mic_veredicto_txt(ch_res[c].veredicto));
  }
  snprintf(b + p, sizeof(b) - p, "],\"medidos\":%d,\"suenan\":%d}", medidos, suenan);
  return json(req, b);
}
