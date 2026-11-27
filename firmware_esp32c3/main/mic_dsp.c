/**
 * @file mic_dsp.c
 * @brief Goertzel, ventanas y veredicto. Ver mic_dsp.h.
 */

#include "mic_dsp.h"
#include <math.h>

#define FONDO_ESCALA 8388608.0f   /* 2^23: el micrófono entrega 24 bits */

void mic_goertzel_init(mic_goertzel_t *g, float hz, float fs) {
  g->coef = 2.0f * cosf(2.0f * 3.14159265f * hz / fs);
  g->s1 = g->s2 = 0.0f;
  g->n = 0;
}

void mic_goertzel_push(mic_goertzel_t *g, int32_t x) {
  float s0 = (float)x / FONDO_ESCALA + g->coef * g->s1 - g->s2;
  g->s2 = g->s1;
  g->s1 = s0;
  g->n++;
}

float mic_goertzel_dbfs(const mic_goertzel_t *g) {
  if (g->n == 0) return -200.0f;
  float pot = g->s1 * g->s1 + g->s2 * g->s2 - g->coef * g->s1 * g->s2;
  if (pot <= 0.0f) return -200.0f;
  /* |X| = A·N/2 para una senoidal de pico A: A = 2·|X|/N. */
  float a = 2.0f * sqrtf(pot) / (float)g->n;
  if (a <= 1e-10f) return -200.0f;
  return 20.0f * log10f(a);
}

void mic_ventana_init(mic_ventana_t *w, float fs) {
  mic_goertzel_init(&w->tono, MIC_TONO_HZ, fs);
  mic_goertzel_init(&w->v1, MIC_VECINA1_HZ, fs);
  mic_goertzel_init(&w->v2, MIC_VECINA2_HZ, fs);
  w->min = INT32_MAX;
  w->max = INT32_MIN;
}

void mic_ventana_push(mic_ventana_t *w, int32_t x) {
  mic_goertzel_push(&w->tono, x);
  mic_goertzel_push(&w->v1, x);
  mic_goertzel_push(&w->v2, x);
  if (x < w->min) w->min = x;
  if (x > w->max) w->max = x;
}

mic_resultado_t mic_ventana_veredicto(const mic_ventana_t *w) {
  mic_resultado_t r;
  r.tono_dbfs = mic_goertzel_dbfs(&w->tono);
  float a = mic_goertzel_dbfs(&w->v1), b = mic_goertzel_dbfs(&w->v2);
  r.vecina_dbfs = a > b ? a : b;
  if (w->tono.n == 0 || w->min == w->max)
    r.veredicto = MIC_SIN_MICROFONO;
  else if (r.tono_dbfs >= MIC_PISO_ABS_DBFS &&
           r.tono_dbfs - r.vecina_dbfs >= MIC_MARGEN_REL_DB)
    r.veredicto = MIC_SUENA;
  else
    r.veredicto = MIC_NO_SUENA;
  return r;
}

const char *mic_veredicto_txt(mic_veredicto_t v) {
  switch (v) {
    case MIC_SUENA:         return "suena";
    case MIC_NO_SUENA:      return "no suena";
    case MIC_SIN_MICROFONO: return "sin microfono";
  }
  return "?";
}

float mic_rms_dbfs(double suma_cuadrados, uint32_t n) {
  if (n == 0 || suma_cuadrados <= 0.0) return -200.0f;
  double rms = sqrt(suma_cuadrados / (double)n) / (double)FONDO_ESCALA;
  if (rms <= 1e-10) return -200.0f;
  return (float)(20.0 * log10(rms));
}
