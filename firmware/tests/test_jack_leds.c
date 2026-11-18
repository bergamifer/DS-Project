/**
 * @file tests/test_jack_leds.c
 * @brief Test de host de los LEDs de J9 (Core/Src/jack_leds.c).
 *
 * Corre en la Mac: `make test` desde firmware/. Barre TODAS las combinaciones de
 * entrada, milisegundo a milisegundo durante 2 s, y comprueba:
 *   1. SEGURIDAD: el LED 2 sólo está encendido de forma continua si hay
 *      amplificadores encendidos, la guarda en OK, sin bloqueo y sin modo seguro.
 *   2. Que cada patrón ocurre de verdad (un LED siempre apagado pasaría la 1).
 *
 * Los requisitos están escritos acá y NO tomados de jack_leds.h: con las macros
 * del código, un mutante que las cambia se aprobaría a sí mismo.
 */

#include <stdio.h>
#include <stdint.h>
#include "../Core/Inc/jack_leds.h"
#include "../Core/Src/jack_leds.c"

static int fallas = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fallas++; \
    printf("  FALLA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define VENTANA_MS 2000u

/* Cuenta flancos de subida y milisegundos encendidos del LED elegido. */
static void medir(jl_entrada_t e, int cual, unsigned *flancos, unsigned *ms_on) {
  uint8_t a1, a2, prev = 0;
  *flancos = 0; *ms_on = 0;
  for (uint32_t t = 0; t < VENTANA_MS; t++) {
    jack_leds_patron(&e, t, &a1, &a2);
    uint8_t v = cual == 1 ? a1 : a2;
    if (v && !prev && t > 0) (*flancos)++;
    if (v) (*ms_on)++;
    prev = v;
  }
}

int main(void) {
  /* 1. Seguridad: las 16 combinaciones. */
  for (int c = 0; c < 16; c++) {
    jl_entrada_t e = { (uint8_t)(c & 1), (uint8_t)((c >> 1) & 1),
                       (uint8_t)((c >> 2) & 1), (uint8_t)((c >> 3) & 1) };
    int permitido = e.amps_on && e.guardia_ok && !e.bloqueado && !e.mismatch;
    unsigned fl, on;
    medir(e, 2, &fl, &on);
    int fijo = (on == VENTANA_MS);
    CHECK(!fijo || permitido,
          "LED 2 FIJO sin permiso (mismatch=%d amps=%d guardia=%d bloq=%d)",
          e.mismatch, e.amps_on, e.guardia_ok, e.bloqueado);
    CHECK(!permitido || fijo,
          "LED 2 no está fijo con todo en orden (mismatch=%d amps=%d guardia=%d bloq=%d)",
          e.mismatch, e.amps_on, e.guardia_ok, e.bloqueado);
  }

  /* 2. Cada patrón existe. Requisitos: latido 1 por segundo con 80 ms de destello;
   *    lento = 1 Hz (2 flancos en 2 s); rápido = 5 Hz (10 flancos en 2 s). */
  unsigned fl, on;
  jl_entrada_t normal = { 0, 0, 1, 0 };
  medir(normal, 1, &fl, &on);
  CHECK(fl == 1, "latido: %u flancos en 2 s (esperaba 1: el primero cae en t=0)", fl);
  CHECK(on == 160u, "latido: %u ms encendido en 2 s (esperaba 160)", on);

  jl_entrada_t modo_seguro = { 1, 0, 1, 0 };
  medir(modo_seguro, 1, &fl, &on);
  CHECK(fl == 9, "modo seguro: %u flancos del LED 1 (esperaba 9, 5 Hz)", fl);

  jl_entrada_t sin_5va = { 0, 1, 0, 0 };
  medir(sin_5va, 2, &fl, &on);
  CHECK(fl == 1 && on == 1000u, "sin +5VA: %u flancos, %u ms on (esperaba 1 y 1000: 1 Hz)", fl, on);

  jl_entrada_t bloq = { 0, 0, 1, 1 };
  medir(bloq, 2, &fl, &on);
  CHECK(fl == 9 && on == 1000u, "bloqueado: %u flancos, %u ms on (esperaba 9 y 1000: 5 Hz)", fl, on);

  jl_entrada_t apagados = { 0, 0, 1, 0 };
  medir(apagados, 2, &fl, &on);
  CHECK(on == 0, "amplificadores apagados y guarda OK: LED 2 encendido %u ms (esperaba 0)", on);

  if (fallas) { printf("jack_leds: %d FALLAS\n", fallas); return 1; }
  printf("jack_leds: ok (16 combinaciones x 2 s, y los cinco patrones)\n");
  return 0;
}
