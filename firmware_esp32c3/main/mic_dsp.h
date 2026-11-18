/**
 * @file mic_dsp.h
 * @brief Las cuentas del micrófono del ESP32 (DS12), sin ESP-IDF: se prueban en
 *        la Mac con test/test_mic_dsp.c.
 *
 * QUÉ MIDE. Para saber si un parlante suena no hace falta el audio entero: el
 * STM32, en el modo 1 (channel walker), pone un tono de 1 kHz en un solo canal
 * por segundo. Alcanza con medir cuánta energía hay EXACTAMENTE en 1 kHz durante
 * cada segundo. Eso es un Goertzel: un filtro que mira una sola frecuencia, con
 * un costo de una multiplicación por muestra.
 *
 * CÓMO DECIDE. Un número solo no alcanza (la regla del repo: preguntarle a la
 * herramienta qué haría si no hubiera nada que medir). Un parlante "suena" si se
 * cumplen las dos:
 *   - RELATIVO: 1 kHz está al menos MIC_MARGEN_REL_DB por encima de dos
 *     frecuencias vecinas (800 Hz y 1250 Hz) medidas en la MISMA ventana. Un
 *     ruido de banda ancha sube las tres juntas y no pasa.
 *   - ABSOLUTO: 1 kHz está por encima de MIC_PISO_ABS_DBFS. Con silencio total
 *     (micrófono ausente, todo ceros) el relativo es indefinido; esto lo corta.
 * Y si el bloque es constante (el pull-down de 100 kΩ sin micrófono soldado da
 * ceros), el veredicto es "sin micrófono", no "no suena".
 *
 * LOS UMBRALES NO ESTÁN VALIDADOS EN UNA PLACA: son cuentas sobre la hoja del
 * MSM261S4030H0R (SNR 61 dB, −26 dBFS a 94 dB SPL). Ver DS12_MICROFONO_DNP.md.
 */

#ifndef MIC_DSP_H
#define MIC_DSP_H

#include <stdint.h>

/* Con #ifndef sólo para que el test de host compile mutantes. */
#ifndef MIC_MARGEN_REL_DB
#define MIC_MARGEN_REL_DB   10.0f    /* 1 kHz sobre la vecina más alta */
#endif
#ifndef MIC_PISO_ABS_DBFS
#define MIC_PISO_ABS_DBFS  -100.0f   /* el ruido propio del micrófono en un bin de
                                       ~1,7 Hz da ≈ −125 dBFS: 25 dB de margen */
#endif

#define MIC_FS_HZ        24000u      /* SCK = 24 kHz × 64 = 1,536 MHz (hoja: 1–4 MHz) */
#define MIC_TONO_HZ      1000.0f     /* el tono del channel walker (modo 1) */
#define MIC_VECINA1_HZ    800.0f
#define MIC_VECINA2_HZ   1250.0f

/** Un Goertzel: acumula muestras y al final da el nivel de una frecuencia. */
typedef struct {
  float coef, s1, s2;
  uint32_t n;
} mic_goertzel_t;

void  mic_goertzel_init(mic_goertzel_t *g, float hz, float fs);
void  mic_goertzel_push(mic_goertzel_t *g, int32_t x);   /* x en escala de 24 bits */
/** Nivel de pico de una senoidal en esa frecuencia, en dBFS (0 = pico a fondo
 *  de escala). −200 si no hubo muestras o la energía es 0. */
float mic_goertzel_dbfs(const mic_goertzel_t *g);

/** Las tres frecuencias de una ventana, y si el bloque varió alguna vez. */
typedef struct {
  mic_goertzel_t tono, v1, v2;
  int32_t min, max;
} mic_ventana_t;

void mic_ventana_init(mic_ventana_t *w, float fs);
void mic_ventana_push(mic_ventana_t *w, int32_t x);

typedef enum {
  MIC_SUENA = 0,
  MIC_NO_SUENA,
  MIC_SIN_MICROFONO,       /* todas las muestras iguales: no hay nada soldado */
} mic_veredicto_t;

typedef struct {
  float tono_dbfs, vecina_dbfs;   /* la vecina más alta */
  mic_veredicto_t veredicto;
} mic_resultado_t;

mic_resultado_t mic_ventana_veredicto(const mic_ventana_t *w);
const char *mic_veredicto_txt(mic_veredicto_t v);

/** RMS en dBFS (0 = senoidal a fondo de escala da −3,01). */
float mic_rms_dbfs(double suma_cuadrados, uint32_t n);

#endif /* MIC_DSP_H */
