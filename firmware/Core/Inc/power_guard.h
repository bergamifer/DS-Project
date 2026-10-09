/**
 * @file power_guard.h
 * @brief La guarda de +5VA: cuándo pueden estar encendidos los amplificadores.
 *
 * QUÉ PROBLEMA RESUELVE. En la DS12 los 32 amplificadores (+5VA, buck IC4) y el
 * STM32 (+3V3, buck U11) tienen fuentes separadas. Entonces el STM32 puede
 * seguir vivo con +5VA en cero, y eso pasa en dos situaciones normales:
 *   - la placa enchufada sólo por USB (flasheo, puesta en marcha);
 *   - la fuente de 20 V que se corta o se hunde con la placa andando.
 * En las dos, si SD (PD3) queda en alto, PD3 alimenta a los 32 amplificadores
 * por sus pines SD (DS12_ALIMENTACION.md §6.1.a). Y cuando +5VA vuelve, los
 * codecs perdieron su configuración y nadie se entera (§6.1.b).
 *
 * CÓMO SE REPARTE EL TRABAJO.
 *   - Lo URGENTE lo hace una interrupción (main.c): el watchdog analógico del
 *     ADC1 sobre PC1, o el flanco de bajada de PG en PA10, baja SD y PDN_AK en
 *     microsegundos. Con señal, el riel tarda 0,78 ms en ir de 4,58 V al UVLO
 *     de los PAM: revisarlo cada 1 ms desde el main loop no alcanza.
 *   - Lo que se puede PENSAR va acá, sin HAL, para poder probarlo en la Mac
 *     (firmware/tests/test_power_guard.c): cuándo se considera que volvió,
 *     cuándo reconfigurar los codecs y si los amplificadores vuelven solos.
 *
 * LOS ESTADOS.
 *
 *     CAIDO ──(riel sobre el rearme y PG alto)──► VOLVIENDO
 *       ▲                                            │ 50 ms seguidos
 *       │ cualquier bajón                            ▼
 *       ├────────────────────────────────────── RECUPERANDO  (main.c reconfigura)
 *       │                                            │
 *       └──────────── disparo / bajón ──────────── OK ◄┘
 *
 * Sólo en OK se permiten los amplificadores.
 *
 * LOS AMPLIFICADORES VUELVEN SOLOS sólo si estaban encendidos cuando cayó el
 * riel y nadie los apagó mientras tanto. Y como mucho PGD_MAX_RESTAUR veces cada
 * PGD_VENTANA_MS: una fuente que no da abasto se cae cada vez que suenan, y sin
 * este límite la obra sería un tartamudeo de golpes cada medio segundo. Pasado
 * el límite quedan apagados hasta que una persona los encienda.
 */

#ifndef __POWER_GUARD_H
#define __POWER_GUARD_H

#include <stdint.h>

/* Con #ifndef sólo para que el test de host pueda compilar versiones rotas a
 * propósito (mutantes) y comprobar que las detecta. El firmware no los toca. */
#ifndef PGD_ESTABLE_MS
#define PGD_ESTABLE_MS    50u        /* riel arriba y PG alto, seguidos, antes de recuperar */
#endif
#ifndef PGD_MAX_RESTAUR
#define PGD_MAX_RESTAUR   3u         /* restauraciones automáticas por ventana */
#endif
#ifndef PGD_VENTANA_MS
#define PGD_VENTANA_MS    600000u    /* 10 min */
#endif

typedef enum {
  PGD_CAIDO = 0,      /* +5VA abajo, o todavía no se vio bien: amplificadores prohibidos */
  PGD_VOLVIENDO,      /* riel arriba del rearme y PG alto: contando PGD_ESTABLE_MS */
  PGD_RECUPERANDO,    /* el main loop está reconfigurando los codecs */
  PGD_OK              /* amplificadores permitidos */
} pgd_estado_t;

typedef enum {
  PGD_NADA = 0,
  PGD_CAYO,           /* recién cayó: dejar SD y PDN_AK abajo y amps_enabled en 0 */
  PGD_RECUPERAR,      /* correr la recuperación de los codecs y llamar pgd_recuperado() */
  PGD_RESTAURAR,      /* volvió todo y había amplificadores pedidos: encenderlos */
  PGD_BLOQUEADO       /* volvió todo, pero se cayó demasiadas veces: no se encienden solos */
} pgd_accion_t;

typedef struct {
  pgd_estado_t estado;
  uint32_t t_estado;        /* cuándo entró al estado actual, en ms */
  uint8_t  amps_pedidos;    /* había amplificadores encendidos al caer, y nadie los apagó */
  uint8_t  restauraciones;  /* restauraciones automáticas dentro de la ventana */
  uint32_t t_ventana;       /* comienzo de la ventana */
  uint8_t  bloqueado;       /* pasó el límite: no se restaura sola */
  uint16_t caidas;          /* caídas desde el arranque, para la telemetría */
} pgd_t;

/** Estado inicial, al terminar el arranque. Los codecs se acaban de configurar,
 *  así que si el riel está bien no hace falta recuperar nada. Si no lo está
 *  (sólo USB), se arranca en CAIDO y la recuperación corre cuando llegue. */
void pgd_init(pgd_t *g, uint32_t ahora, int riel_sobre_rearme, int pg_alto);

/** Una pasada del main loop.
 *  @param disparo            la interrupción cortó desde la pasada anterior
 *  @param riel_sobre_umbral  PC1 arriba del umbral de corte (2,25 V)
 *  @param riel_sobre_rearme  PC1 arriba del umbral de vuelta (2,35 V)
 *  @param pg_alto            PA10
 *  @param amps_on            amps_enabled: lo que estaba pedido antes del corte */
pgd_accion_t pgd_tick(pgd_t *g, uint32_t ahora, int disparo, int riel_sobre_umbral,
                      int riel_sobre_rearme, int pg_alto, int amps_on);

/** El main loop terminó la recuperación. @p codecs_ok: los cuatro DAC y el ADC
 *  contestaron. Si no, la placa queda en OK pero sin restaurar sola: un codec
 *  mudo con 32 amplificadores encendidos es ruido o silencio, y que lo decida
 *  una persona, igual que en el arranque. */
pgd_accion_t pgd_recuperado(pgd_t *g, uint32_t ahora, int codecs_ok,
                            int riel_sobre_rearme, int pg_alto);

/** Una persona (o el autoarranque) los encendió: se olvida el bloqueo. */
void pgd_pedido_encender(pgd_t *g, uint32_t ahora);
/** Alguien los apagó: si estaban pedidos durante una caída, ya no lo están. */
void pgd_pedido_apagar(pgd_t *g);

static inline int pgd_permitido(const pgd_t *g) { return g->estado == PGD_OK; }

#endif /* __POWER_GUARD_H */
