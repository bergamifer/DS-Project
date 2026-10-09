/**
 * @file tests/test_vbus_detect.c
 * @brief Test de host del detector de VBUS (Core/Inc/vbus_detect.h).
 *
 * La propiedad: `presente` sólo con PA5 en alto los últimos VBUS_ESTABLE_MS
 * seguidos, y nunca con PA5 en bajo. Se revisa en cada milisegundo, contra una
 * historia independiente del pin. Mutante: -DVBUS_ESTABLE_MS=0u.
 */

#include <stdio.h>
#include <stdint.h>
#include "../Core/Inc/vbus_detect.h"

/* El requisito, escrito acá y NO tomado de vbus_detect.h: si el test usara la
 * macro del código, un mutante que la cambia se aprobaría a sí mismo. */
#define REQ_MS 20u

static int fallas = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fallas++; \
    printf("  FALLA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* El pin y su historia: cuántos ms seguidos lleva en alto. */
typedef struct { vbus_t v; uint32_t ahora; int pa5; uint32_t alto_ms; int cambios; } sim_t;

static void sim_init(sim_t *s, uint32_t t0) { *s = (sim_t){0}; s->ahora = t0; vbus_init(&s->v); }

static void paso(sim_t *s)
{
  s->alto_ms = s->pa5 ? s->alto_ms + 1 : 0;
  s->cambios += vbus_tick(&s->v, s->ahora, s->pa5);
  CHECK(!(s->v.presente && !s->pa5), "presente con PA5 en bajo (t = %u)", (unsigned)s->ahora);
  CHECK(!(s->v.presente && s->alto_ms <= REQ_MS),
        "presente con PA5 en alto hace sólo %u ms (t = %u)", (unsigned)s->alto_ms, (unsigned)s->ahora);
  s->ahora++;
}

static void correr(sim_t *s, int pa5, uint32_t ms) { s->pa5 = pa5; while (ms--) paso(s); }

int main(void)
{
  sim_t s;

  /* Sin cable: nunca. */
  sim_init(&s, 1000); correr(&s, 0, 5000);
  CHECK(!s.v.presente && s.cambios == 0, "sin cable no tenía que aparecer");

  /* Cable firme: aparece una vez, después de los 20 ms. */
  sim_init(&s, 1000); correr(&s, 1, 200);
  CHECK(s.v.presente && s.cambios == 1 && s.v.conexiones == 1, "con cable tenía que aparecer una vez");

  /* Cable que rebota al enchufarse: pulsos de 5 ms durante 100 ms. */
  sim_init(&s, 1000);
  for (int i = 0; i < 10; i++) { correr(&s, 1, 5); correr(&s, 0, 5); }
  CHECK(!s.v.presente && s.cambios == 0, "un rebote no puede conectar");
  correr(&s, 1, 100);
  CHECK(s.v.presente && s.v.conexiones == 1, "asentado el cable, una sola conexión");

  /* Desenchufar: afuera en el mismo milisegundo. */
  correr(&s, 0, 1);
  CHECK(!s.v.presente, "al bajar PA5 tenía que desconectar enseguida");

  /* La vuelta del tick. */
  sim_init(&s, 0xFFFFFFF0u); correr(&s, 1, 100);
  CHECK(s.v.presente, "falló al dar la vuelta el tick");

  printf("vbus_detect: %s (%d fallas)\n", fallas ? "FALLA" : "ok", fallas);
  return fallas ? 1 : 0;
}
