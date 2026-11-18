/**
 * @file test/test_mic_dsp.c
 * @brief Test de host de las cuentas del micrófono (main/mic_dsp.c).
 *
 * Corre en la Mac: `make -C firmware_esp32c3/test`. Arma señales sintéticas con
 * respuesta conocida y comprueba el número Y el veredicto. Incluye lo que pide la
 * regla del repo: qué dice la herramienta cuando NO hay nada que medir (ceros,
 * ruido solo, un tono por debajo del piso). Los mutantes del Makefile rompen los
 * umbrales y este test tiene que fallar con cada uno.
 *
 * Los requisitos están escritos acá y no tomados de mic_dsp.h.
 */

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "../main/mic_dsp.h"
#include "../main/mic_dsp.c"

static int fallas = 0;
#define CHECK(c, ...) do { if (!(c)) { fallas++; printf("  FALLA %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

#define N_VENTANA 14400u            /* 0,6 s a 24 kHz: lo que mira el chequeo */
#define FS 24000.0f
#define FONDO 8388608.0

static uint32_t semilla = 12345u;
/* Ruido casi gaussiano de RMS 1: suma de 12 uniformes menos 6. */
static double ruido(void) {
  double s = 0;
  for (int i = 0; i < 12; i++) { semilla = semilla * 1664525u + 1013904223u; s += semilla / 4294967296.0; }
  return s - 6.0;
}

typedef struct { double tono_dbfs, f2_hz, f2_dbfs, ruido_rms_dbfs; } senal_t;

static mic_resultado_t medir(senal_t s) {
  mic_ventana_t w;
  mic_ventana_init(&w, FS);
  double a1 = s.tono_dbfs > -300 ? FONDO * pow(10.0, s.tono_dbfs / 20.0) : 0;
  double a2 = s.f2_dbfs > -300 ? FONDO * pow(10.0, s.f2_dbfs / 20.0) : 0;
  double rn = s.ruido_rms_dbfs > -300 ? FONDO * pow(10.0, s.ruido_rms_dbfs / 20.0) : 0;
  for (uint32_t i = 0; i < N_VENTANA; i++) {
    double t = i / (double)FS;
    double x = a1 * sin(2 * M_PI * 1000.0 * t + 0.3) + a2 * sin(2 * M_PI * s.f2_hz * t) + rn * ruido();
    mic_ventana_push(&w, (int32_t)lrint(x));
  }
  return mic_ventana_veredicto(&w);
}

#define NADA -400.0

int main(void) {
  mic_resultado_t r;

  /* 1. El número: una senoidal de −20 dBFS de pico tiene que dar −20. */
  r = medir((senal_t){ -20.0, 0, NADA, NADA });
  CHECK(fabs(r.tono_dbfs + 20.0) < 0.2, "tono de −20 dBFS medido en %.2f", r.tono_dbfs);
  CHECK(r.veredicto == MIC_SUENA, "tono limpio: %s", mic_veredicto_txt(r.veredicto));

  /* 2. Un parlante normal: tono a −40 con ruido de sala a −60 RMS. */
  r = medir((senal_t){ -40.0, 0, NADA, -60.0 });
  CHECK(r.veredicto == MIC_SUENA, "tono −40 + ruido −60: %s (%.1f / %.1f)",
        mic_veredicto_txt(r.veredicto), r.tono_dbfs, r.vecina_dbfs);

  /* 3. NADA QUE MEDIR, versión ruido fuerte: sólo ruido de banda ancha a −30 RMS.
   *    El bin de 1 kHz tiene energía (está muy por encima del piso absoluto),
   *    así que lo único que lo frena es el criterio relativo. */
  r = medir((senal_t){ NADA, 0, NADA, -30.0 });
  CHECK(r.veredicto == MIC_NO_SUENA, "ruido solo: %s (%.1f / %.1f)",
        mic_veredicto_txt(r.veredicto), r.tono_dbfs, r.vecina_dbfs);

  /* 4. NADA QUE MEDIR, versión silencio: un tono de −110 dBFS sin ruido. Está
   *    limpio (el relativo lo deja pasar) pero bajo el piso absoluto. */
  r = medir((senal_t){ -110.0, 0, NADA, NADA });
  CHECK(r.veredicto == MIC_NO_SUENA, "tono bajo el piso: %s (%.1f)", mic_veredicto_txt(r.veredicto), r.tono_dbfs);

  /* 5. Sin micrófono soldado: el pull-down da ceros. */
  r = medir((senal_t){ NADA, 0, NADA, NADA });
  CHECK(r.veredicto == MIC_SIN_MICROFONO, "ceros: %s", mic_veredicto_txt(r.veredicto));

  /* 6. Otra cosa más fuerte al lado (música, otro modo): no se puede afirmar. */
  r = medir((senal_t){ -40.0, 800.0, -30.0, NADA });
  CHECK(r.veredicto == MIC_NO_SUENA, "800 Hz más fuerte que el tono: %s", mic_veredicto_txt(r.veredicto));

  /* 7. RMS: una senoidal a fondo de escala da −3,01 dBFS. */
  double sc = 0;
  for (int i = 0; i < 48000; i++) { double x = FONDO * sin(2 * M_PI * 1000.0 * i / 48000.0); sc += x * x; }
  float rms = mic_rms_dbfs(sc, 48000);
  CHECK(fabsf(rms + 3.01f) < 0.05f, "RMS de una senoidal a fondo de escala: %.2f", rms);

  if (fallas) { printf("mic_dsp: %d FALLAS\n", fallas); return 1; }
  printf("mic_dsp: ok (7 escenarios, 3 sin nada que medir)\n");
  return 0;
}
