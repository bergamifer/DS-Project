/**
 * @file jack_leds.h
 * @brief Los LEDs de los jacks de J9 (DS12): qué dicen y cuándo.
 *
 * CÓMO ESTÁN CABLEADOS. Cada jack de J9 trae dos LEDs. En la DS12 los cuatro
 * LED 1 comparten PE12 (a través de R111–R114) y los cuatro LED 2 comparten
 * PE13 (R164–R167). Son DOS señales, no ocho: los cuatro jacks se prenden y se
 * apagan juntos. Alto = encendido (ánodo del lado de la resistencia).
 *
 * QUÉ DICEN.
 *   LED 1 (PE12) — "el firmware está vivo":
 *       destello corto una vez por segundo.
 *       Parpadeo rápido (5 Hz): la placa NO es la que el firmware cree
 *       (board_mismatch). Es el modo seguro.
 *   LED 2 (PE13) — "amplificadores":
 *       apagado         = amplificadores apagados (lo normal al arrancar);
 *       FIJO            = amplificadores encendidos Y la guarda de +5VA en OK;
 *       parpadeo lento  = la guarda dice que no hay +5VA (o está volviendo);
 *       parpadeo rápido = la guarda se bloqueó: se cayó demasiadas veces y los
 *                         amplificadores no vuelven solos.
 *
 * LA PROPIEDAD QUE IMPORTA: el LED 2 sólo está FIJO si hay amplificadores
 * encendidos y +5VA sano. Un LED que dice "encendido" con el riel caído es peor
 * que no tener LED. Eso lo comprueba tests/test_jack_leds.c en cada ms.
 *
 * LÍMITE: el LED 2 muestra lo que el FIRMWARE cree (amps_enabled y la guarda),
 * no una medida del riel de los amplificadores. Es una ayuda para quien está
 * parado frente a la placa, no un enclavamiento. Tampoco se sabe todavía el
 * sentido de montaje del LED 2 (prueba M13b): si prende al revés, el LED no
 * enciende nunca, y eso se arregla en el PCB, no acá.
 *
 * Sin HAL: la función es pura, para probarla en la Mac.
 */

#ifndef __JACK_LEDS_H
#define __JACK_LEDS_H

#include <stdint.h>

/* Con #ifndef sólo para que el test de host pueda compilar versiones rotas a
 * propósito (mutantes). El firmware no los toca. */
#ifndef JL_LATIDO_MS
#define JL_LATIDO_MS        1000u   /* período del latido del LED 1 */
#endif
#ifndef JL_LATIDO_ON_MS
#define JL_LATIDO_ON_MS     80u     /* cuánto dura el destello */
#endif
#ifndef JL_LENTO_MEDIO_MS
#define JL_LENTO_MEDIO_MS   500u    /* medio período del parpadeo lento (1 Hz) */
#endif
#ifndef JL_RAPIDO_MEDIO_MS
#define JL_RAPIDO_MEDIO_MS  100u    /* medio período del parpadeo rápido (5 Hz) */
#endif
#ifndef JL_EXIGE_GUARDIA
#define JL_EXIGE_GUARDIA    1u      /* 0 = mutante: fijo aunque la guarda no esté OK */
#endif

typedef struct {
  uint8_t mismatch;     /* board_mismatch: modo seguro */
  uint8_t amps_on;      /* amps_enabled */
  uint8_t guardia_ok;   /* la guarda permite amplificadores (pgd_permitido y sin disparo) */
  uint8_t bloqueado;    /* la guarda se bloqueó (v5a_guard.bloqueado) */
} jl_entrada_t;

/** Qué deben mostrar los dos LEDs en el instante @p ms (1 = encendido). */
void jack_leds_patron(const jl_entrada_t *e, uint32_t ms, uint8_t *led1, uint8_t *led2);

#endif /* __JACK_LEDS_H */
