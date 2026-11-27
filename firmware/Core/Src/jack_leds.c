/**
 * @file jack_leds.c
 * @brief Patrón de los LEDs de J9. Ver jack_leds.h: qué dicen y por qué.
 */

#include "jack_leds.h"

/* 1 durante la primera mitad de cada período de 2*medio, 0 en la segunda. */
static uint8_t parpadeo(uint32_t ms, uint32_t medio) {
  return (uint8_t)(((ms / medio) & 1u) == 0u);
}

void jack_leds_patron(const jl_entrada_t *e, uint32_t ms, uint8_t *led1, uint8_t *led2) {
  /* LED 1: vivo. En modo seguro, parpadeo rápido; si no, destello corto. */
  if (e->mismatch) *led1 = parpadeo(ms, JL_RAPIDO_MEDIO_MS);
  else             *led1 = (uint8_t)((ms % JL_LATIDO_MS) < JL_LATIDO_ON_MS);

  /* LED 2: amplificadores. Con la placa equivocada están bloqueados, así que el
   * LED no puede decir otra cosa que "apagados" (o el aviso de la guarda). */
  if (e->bloqueado)                            *led2 = parpadeo(ms, JL_RAPIDO_MEDIO_MS);
  else if (JL_EXIGE_GUARDIA && !e->guardia_ok) *led2 = parpadeo(ms, JL_LENTO_MEDIO_MS);
  else if (e->amps_on && !e->mismatch)         *led2 = 1;
  else                                         *led2 = 0;
}
