/**
 * @file tests/test_power_guard.c
 * @brief Test de host de la guarda de +5VA (Core/Src/power_guard.c).
 *
 * Corre en la Mac, sin placa: `make test` desde firmware/.
 *
 * Cada escenario simula el main loop de a 1 ms: el riel (PC1 sobre el umbral de
 * corte y sobre el de rearme), PG, la interrupción y lo que hace main.c con cada
 * acción. La propiedad que importa, y que se revisa en CADA milisegundo de cada
 * escenario, es la de seguridad: **nunca hay amplificadores encendidos con el
 * riel abajo**.
 *
 * ¿Y si el test pasara aunque la guarda estuviera rota? Para eso están los
 * mutantes: `make test` compila además versiones rotas a propósito
 * (-DPGD_ESTABLE_MS=0u, -DPGD_MAX_RESTAUR=200u, -DPGD_VENTANA_MS=1u) y exige que ESTE MISMO test
 * falle con ellas. Un test que no puede fallar no prueba nada.
 */

#include <stdio.h>
#include <stdint.h>
#include "../Core/Inc/power_guard.h"
#include "../Core/Src/power_guard.c"

/* Los requisitos, escritos acá y NO tomados de power_guard.h. Si el test usara
 * las macros del código, un mutante que las cambia se aprobaría a sí mismo:
 * "esperó PGD_ESTABLE_MS" es cierto para cualquier valor de PGD_ESTABLE_MS. */
#define REQ_ESTABLE_MS   50u       /* DS12_ALIMENTACION.md §6.1.b: ≥ 50 ms con riel y PG */
#define REQ_MAX_RESTAUR  3u
#define REQ_VENTANA_MS   600000u

static int fallas = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fallas++; \
    printf("  FALLA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- La placa simulada ---- */
typedef struct {
  pgd_t    g;
  uint32_t ahora;
  int      riel_mv;       /* +5VA en mV */
  int      pg;            /* PA10 */
  int      amps;          /* amps_enabled: lo pedido, según el main loop */
  int      sd;            /* el pin PD3: lo que de verdad tienen los amplificadores */
  int      disparo;       /* la ISR cortó y el main loop todavía no se enteró */
  int      codecs_ok;     /* lo que devolvería la reconfiguración */
  int      n_recuperar, n_restaurar, n_bloqueado, n_cayo;
} placa_t;

#define UMBRAL_MV  4500   /* PC1 = 2,25 V */
#define REARME_MV  4700   /* PC1 = 2,35 V */

static void placa_init(placa_t *p, uint32_t t0, int riel_mv, int pg)
{
  *p = (placa_t){0};
  p->ahora = t0; p->riel_mv = riel_mv; p->pg = pg; p->codecs_ok = 1;
  pgd_init(&p->g, p->ahora, riel_mv >= REARME_MV, pg);
}

/* Lo que haría la ISR: el watchdog del ADC y el flanco de PG. Baja el PIN y
 * nada más: amps_enabled lo actualiza el main loop, y así la guarda sabe qué
 * estaba pedido. */
static void isr(placa_t *p)
{
  if (p->riel_mv < UMBRAL_MV || !p->pg) {
    p->sd = 0;
    p->disparo = 1;
  }
}

/* Una persona aprieta "encender" (Set_Amps(1) en main.c). */
static int persona_enciende(placa_t *p)
{
  if (!pgd_permitido(&p->g) || p->disparo) return 0;
  pgd_pedido_encender(&p->g, p->ahora);
  p->amps = p->sd = 1;
  return 1;
}

static void persona_apaga(placa_t *p) { pgd_pedido_apagar(&p->g); p->amps = p->sd = 0; }

/* Una pasada del main loop, como V5A_Tick() en main.c. */
static void paso(placa_t *p)
{
  isr(p);
  int disp = p->disparo; p->disparo = 0;
  pgd_accion_t a = pgd_tick(&p->g, p->ahora, disp, p->riel_mv >= UMBRAL_MV,
                            p->riel_mv >= REARME_MV, p->pg, p->amps);
  if (a == PGD_CAYO) { p->amps = p->sd = 0; p->n_cayo++; }
  if (a == PGD_RECUPERAR) {
    p->n_recuperar++;
    pgd_accion_t b = pgd_recuperado(&p->g, p->ahora, p->codecs_ok,
                                    p->riel_mv >= REARME_MV, p->pg);
    if (b == PGD_RESTAURAR) { p->amps = p->sd = 1; p->n_restaurar++; }
    if (b == PGD_BLOQUEADO) p->n_bloqueado++;
  }
  /* LA propiedad. */
  CHECK(!(p->sd && (p->riel_mv < UMBRAL_MV || !p->pg)),
        "amplificadores encendidos con +5VA = %d mV, PG = %d (t = %u ms)",
        p->riel_mv, p->pg, (unsigned)p->ahora);
  CHECK(!(p->sd && !pgd_permitido(&p->g)),
        "amplificadores encendidos fuera de OK (estado %d, t = %u ms)",
        (int)p->g.estado, (unsigned)p->ahora);
  p->ahora++;
}

static void correr(placa_t *p, uint32_t ms) { while (ms--) paso(p); }

/* ---- Escenarios ---- */

static void t_arranque_con_riel(void)
{
  placa_t p; placa_init(&p, 1000, 5090, 1);
  CHECK(pgd_permitido(&p.g), "con +5VA al arrancar, tiene que permitir");
  correr(&p, 100);
  CHECK(p.n_recuperar == 0, "no hay nada que recuperar si arrancó bien");
  CHECK(persona_enciende(&p), "una persona tiene que poder encender");
}

static void t_solo_usb(void)
{
  /* Sólo USB: el buck A no tiene VIN, PG queda alto por R159 y +5VA en 0. */
  placa_t p; placa_init(&p, 1000, 0, 1);
  CHECK(!pgd_permitido(&p.g), "sólo USB: no puede permitir");
  for (int i = 0; i < 5000; i++) { persona_enciende(&p); paso(&p); }
  CHECK(p.amps == 0, "sólo USB: los amplificadores nunca encienden");
  CHECK(p.n_recuperar == 0, "sólo USB: sin riel no se recupera");
  /* Llega la fuente: +5VA sube, PG baja durante el soft-start y vuelve. */
  p.riel_mv = 5090; p.pg = 0; correr(&p, 10);
  p.pg = 1;
  correr(&p, REQ_ESTABLE_MS - 1);
  CHECK(p.n_recuperar == 0, "recuperó antes de %u ms estables", REQ_ESTABLE_MS);
  correr(&p, 2);
  CHECK(p.n_recuperar == 1, "tenía que recuperar una vez al llegar la fuente (fueron %d)", p.n_recuperar);
  CHECK(p.amps == 0, "nadie los había pedido: no se encienden solos");
  CHECK(persona_enciende(&p), "después de recuperar, una persona puede encender");
}

static void t_caida_y_vuelta(void)
{
  placa_t p; placa_init(&p, 1000, 5090, 1);
  persona_enciende(&p); correr(&p, 10);
  p.riel_mv = 4300; correr(&p, 1);
  CHECK(p.n_cayo == 1 && !p.amps, "la caída tenía que apagar");
  CHECK(!persona_enciende(&p), "durante la caída no se puede encender");
  correr(&p, 100);
  p.riel_mv = 5090; p.pg = 1;
  correr(&p, REQ_ESTABLE_MS + 2);
  CHECK(p.n_recuperar == 1, "tenía que recuperar (fueron %d)", p.n_recuperar);
  CHECK(p.amps == 1 && p.n_restaurar == 1, "estaban pedidos: tienen que volver solos");
}

static void t_rebote(void)
{
  /* Un riel que vuelve y se cae a los 30 ms: la cuenta de 50 ms empieza de nuevo. */
  placa_t p; placa_init(&p, 1000, 5090, 1);
  persona_enciende(&p);
  p.riel_mv = 0; correr(&p, 20);
  p.riel_mv = 5090; correr(&p, 30);
  p.riel_mv = 4000; correr(&p, 5);
  p.riel_mv = 5090; correr(&p, REQ_ESTABLE_MS - 1);
  CHECK(p.n_recuperar == 0, "recuperó con un riel que rebotaba");
  correr(&p, 2);
  CHECK(p.n_recuperar == 1, "después de 50 ms seguidos tenía que recuperar");
}

static void t_histeresis(void)
{
  /* Entre el umbral (4,5 V) y el rearme (4,7 V) no vuelve. */
  placa_t p; placa_init(&p, 1000, 5090, 1);
  p.riel_mv = 4400; correr(&p, 5);
  p.riel_mv = 4600; correr(&p, 1000);
  CHECK(p.n_recuperar == 0 && !pgd_permitido(&p.g), "volvió debajo del rearme");
}

static void t_caida_con_amps_apagados(void)
{
  placa_t p; placa_init(&p, 1000, 5090, 1);
  p.riel_mv = 0; correr(&p, 10);
  p.riel_mv = 5090; correr(&p, 200);
  CHECK(p.n_recuperar == 1, "tenía que recuperar los codecs igual");
  CHECK(p.amps == 0, "no estaban pedidos: no tienen que encender");
}

static void t_apagan_durante_la_caida(void)
{
  placa_t p; placa_init(&p, 1000, 5090, 1);
  persona_enciende(&p);
  p.riel_mv = 0; correr(&p, 10);
  persona_apaga(&p);
  p.riel_mv = 5090; correr(&p, 200);
  CHECK(p.amps == 0, "una persona los apagó durante la caída: no tienen que volver");
}

static void t_tartamudeo(void)
{
  /* Una fuente que se cae cada vez que suenan. Vuelven solos REQ_MAX_RESTAUR
   * veces; después quedan apagados hasta que encienda una persona. */
  placa_t p; placa_init(&p, 1000, 5090, 1);
  persona_enciende(&p);
  for (int i = 0; i < 6; i++) {
    p.riel_mv = 3000; correr(&p, 5);
    p.riel_mv = 5090; correr(&p, 100);
  }
  CHECK(p.n_restaurar == (int)REQ_MAX_RESTAUR,
        "tenía que restaurar %u veces, restauró %d", REQ_MAX_RESTAUR, p.n_restaurar);
  CHECK(p.amps == 0 && p.g.bloqueado, "pasado el límite tienen que quedar apagados");
  CHECK(persona_enciende(&p) && !p.g.bloqueado, "una persona tiene que poder desbloquear");
  /* Y después de una caída vuelven solos otra vez. */
  p.riel_mv = 3000; correr(&p, 5);
  p.riel_mv = 5090; correr(&p, 100);
  CHECK(p.amps == 1, "después de desbloquear, una caída aislada se restaura");
}

static void t_ventana(void)
{
  /* Caídas espaciadas más de 10 min no se acumulan. */
  placa_t p; placa_init(&p, 1000, 5090, 1);
  persona_enciende(&p);
  for (int i = 0; i < 6; i++) {
    p.riel_mv = 3000; correr(&p, 5);
    p.riel_mv = 5090; correr(&p, 100);
    p.ahora += REQ_VENTANA_MS;   /* pasa el tiempo sin nada que mirar */
  }
  CHECK(p.n_restaurar == 6 && !p.g.bloqueado,
        "caídas espaciadas: tenía que restaurar 6, restauró %d", p.n_restaurar);
}

static void t_codecs_mudos(void)
{
  placa_t p; placa_init(&p, 1000, 5090, 1);
  persona_enciende(&p);
  p.codecs_ok = 0;
  p.riel_mv = 0; correr(&p, 10);
  p.riel_mv = 5090; correr(&p, 200);
  CHECK(p.amps == 0 && p.n_bloqueado == 1, "con un codec mudo no tienen que volver solos");
  CHECK(pgd_permitido(&p.g), "pero el riel está bien: una persona puede encender");
}

static void t_cae_durante_la_recuperacion(void)
{
  placa_t p; placa_init(&p, 1000, 5090, 1);
  persona_enciende(&p);
  p.riel_mv = 0; correr(&p, 10);
  p.riel_mv = 5090; correr(&p, REQ_ESTABLE_MS);
  /* El próximo paso pide RECUPERAR; simulamos que durante la reconfiguración
   * el riel se va de nuevo. */
  CHECK(pgd_tick(&p.g, p.ahora, 0, 1, 1, 1, 0) == PGD_RECUPERAR, "tenía que pedir recuperar");
  CHECK(pgd_recuperado(&p.g, p.ahora, 1, 0, 1) == PGD_NADA, "cayó durante la recuperación");
  CHECK(p.g.estado == PGD_CAIDO, "tenía que volver a CAIDO");
}

static void t_pg_solo(void)
{
  /* PG baja con PC1 todavía bien (la salida cayó un 10 %): también corta. */
  placa_t p; placa_init(&p, 1000, 5090, 1);
  persona_enciende(&p);
  p.pg = 0; correr(&p, 1);
  CHECK(p.n_cayo == 1 && !p.amps, "PG en bajo tenía que cortar");
  correr(&p, 500);
  CHECK(p.n_recuperar == 0, "sin PG no vuelve");
}

static void t_sin_isr(void)
{
  /* La interrupción no llegó: el nivel solo alcanza para cortar (más tarde). */
  placa_t p; placa_init(&p, 1000, 5090, 1);
  p.amps = p.sd = 1;
  CHECK(pgd_tick(&p.g, p.ahora, 0, 0, 0, 1, 1) == PGD_CAYO, "el nivel solo tenía que cortar");
}

static void t_vuelta_del_tick(void)
{
  /* HAL_GetTick() da la vuelta cada 49,7 días. */
  placa_t p; placa_init(&p, 0xFFFFFFF0u, 5090, 1);
  persona_enciende(&p);
  p.riel_mv = 0; correr(&p, 5);
  p.riel_mv = 5090; correr(&p, 200);
  CHECK(p.n_recuperar == 1 && p.amps == 1, "falló al dar la vuelta el tick");
}

int main(void)
{
  t_arranque_con_riel();
  t_solo_usb();
  t_caida_y_vuelta();
  t_rebote();
  t_histeresis();
  t_caida_con_amps_apagados();
  t_apagan_durante_la_caida();
  t_tartamudeo();
  t_ventana();
  t_codecs_mudos();
  t_cae_durante_la_recuperacion();
  t_pg_solo();
  t_sin_isr();
  t_vuelta_del_tick();
  printf("power_guard: %s (%d fallas)\n", fallas ? "FALLA" : "ok", fallas);
  return fallas ? 1 : 0;
}
