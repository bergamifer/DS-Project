/**
 * @file power_guard.c
 * @brief La máquina de estados de la guarda de +5VA. Sin HAL: se compila igual
 *        en la Mac (firmware/tests/test_power_guard.c) y en el STM32.
 *
 * Ver power_guard.h para el porqué. Acá sólo las transiciones.
 *
 * Todos los tiempos son restas de uint32_t: siguen funcionando cuando el tick
 * de milisegundos da la vuelta (cada 49,7 días).
 */

#include "power_guard.h"

static void pasar_a(pgd_t *g, pgd_estado_t e, uint32_t ahora)
{
  g->estado   = e;
  g->t_estado = ahora;
}

void pgd_init(pgd_t *g, uint32_t ahora, int riel_sobre_rearme, int pg_alto)
{
  g->amps_pedidos   = 0;
  g->restauraciones = 0;
  g->t_ventana      = ahora;
  g->bloqueado      = 0;
  g->caidas         = 0;
  pasar_a(g, (riel_sobre_rearme && pg_alto) ? PGD_OK : PGD_CAIDO, ahora);
}

pgd_accion_t pgd_tick(pgd_t *g, uint32_t ahora, int disparo, int riel_sobre_umbral,
                      int riel_sobre_rearme, int pg_alto, int amps_on)
{
  switch (g->estado) {
    case PGD_OK:
      /* La interrupción ya bajó SD. Pero también se mira el nivel: si por lo
       * que sea la interrupción no llegó (estaba deshabilitada, un flanco de PG
       * que se perdió), el main loop lo ve igual, un poco más tarde. */
      if (disparo || !riel_sobre_umbral || !pg_alto) {
        g->amps_pedidos = amps_on ? 1u : 0u;
        if (g->caidas < 0xFFFFu) g->caidas++;
        pasar_a(g, PGD_CAIDO, ahora);
        return PGD_CAYO;
      }
      return PGD_NADA;

    case PGD_CAIDO:
      if (riel_sobre_rearme && pg_alto) pasar_a(g, PGD_VOLVIENDO, ahora);
      return PGD_NADA;

    case PGD_VOLVIENDO:
      /* Cualquier bajón reinicia la cuenta: 50 ms SEGUIDOS. Un riel que rebota
       * en el arranque del buck no tiene que disparar una recuperación a medias. */
      if (!(riel_sobre_rearme && pg_alto)) {
        pasar_a(g, PGD_CAIDO, ahora);
        return PGD_NADA;
      }
      if ((uint32_t)(ahora - g->t_estado) >= PGD_ESTABLE_MS) {
        pasar_a(g, PGD_RECUPERANDO, ahora);
        return PGD_RECUPERAR;
      }
      return PGD_NADA;

    case PGD_RECUPERANDO:
    default:
      /* Lo resuelve pgd_recuperado(). */
      return PGD_NADA;
  }
}

pgd_accion_t pgd_recuperado(pgd_t *g, uint32_t ahora, int codecs_ok,
                            int riel_sobre_rearme, int pg_alto)
{
  if (g->estado != PGD_RECUPERANDO) return PGD_NADA;

  /* Se volvió a caer mientras se reconfiguraba: lo configurado no vale. */
  if (!(riel_sobre_rearme && pg_alto)) {
    pasar_a(g, PGD_CAIDO, ahora);
    return PGD_NADA;
  }

  pasar_a(g, PGD_OK, ahora);
  if (!g->amps_pedidos) return PGD_NADA;
  g->amps_pedidos = 0;

  if (!codecs_ok) {
    g->bloqueado = 1;
    return PGD_BLOQUEADO;
  }
  if (g->bloqueado) return PGD_BLOQUEADO;

  if ((uint32_t)(ahora - g->t_ventana) >= PGD_VENTANA_MS) {
    g->t_ventana      = ahora;
    g->restauraciones = 0;
  }
  if (g->restauraciones >= PGD_MAX_RESTAUR) {
    g->bloqueado = 1;
    return PGD_BLOQUEADO;
  }
  g->restauraciones++;
  return PGD_RESTAURAR;
}

void pgd_pedido_encender(pgd_t *g, uint32_t ahora)
{
  g->bloqueado      = 0;
  g->restauraciones = 0;
  g->t_ventana      = ahora;
}

void pgd_pedido_apagar(pgd_t *g)
{
  g->amps_pedidos = 0;
}
