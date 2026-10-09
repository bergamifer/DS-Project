/**
 * @file vbus_detect.h
 * @brief ¿Hay una computadora enchufada en el USB del STM32 (J8)? — DS12, PA5.
 *
 * EL CIRCUITO. VBUS de J8 → R161 62k → PA5 → R160 100k → GND, con C50 100 nF.
 * PA5 = VBUS × 100/162: con 5 V lee 3,09 V. Además R33 (1 kΩ) descarga VBUS
 * del lado del conector, para que la fuga de D8 no lo deje flotando en 4,8 V
 * sin cable (N2 de la revisión de alimentación).
 *
 * Se lee en DIGITAL, como entrada SIN pull: un pull interno de 40 kΩ contra
 * los 100 kΩ de R160 rompería el divisor. El STM32 lee 1 desde 0,7 × 3,3 =
 * 2,31 V en el pin (VBUS ≥ 3,74 V) y 0 hasta ~1 V (VBUS ≤ 1,6 V). La fuga
 * con R33 deja 0,17–0,40 V: un 0 sin dudas.
 *
 * PARA QUÉ. El USB del STM32 (fase 5) sólo puede conectar su pull-up de D+
 * con VBUS presente: sin computadora, ese pull-up empujaría corriente hacia un
 * VBUS muerto. La regla (DS12_REVISION_ALIMENTACION.md §9.2-3):
 *   - CONECTAR sólo después de 20 ms seguidos con PA5 en alto. Un cable que se
 *     está enchufando rebota, y conectar y desconectar en ráfaga confunde al
 *     host;
 *   - DESCONECTAR apenas baja, sin esperar nada.
 *
 * Sin HAL, para el test de host (tests/test_vbus_detect.c).
 */

#ifndef __VBUS_DETECT_H
#define __VBUS_DETECT_H

#include <stdint.h>

#ifndef VBUS_ESTABLE_MS
#define VBUS_ESTABLE_MS  20u
#endif

typedef struct {
  uint8_t  presente;   /* el resultado: hay VBUS, con antirrebote */
  uint8_t  contando;   /* PA5 en alto, todavía sin los 20 ms */
  uint32_t t_alto;     /* cuándo subió */
  uint16_t conexiones; /* veces que pasó a presente, para la telemetría */
} vbus_t;

static inline void vbus_init(vbus_t *v)
{
  v->presente = 0; v->contando = 0; v->t_alto = 0; v->conexiones = 0;
}

/** Una pasada. Devuelve 1 si `presente` cambió en esta llamada. */
static inline int vbus_tick(vbus_t *v, uint32_t ahora, int pa5)
{
  if (!pa5) {                       /* bajó: afuera ya, sin antirrebote */
    v->contando = 0;
    if (v->presente) { v->presente = 0; return 1; }
    return 0;
  }
  if (v->presente) return 0;
  if (!v->contando) { v->contando = 1; v->t_alto = ahora; }
  if ((uint32_t)(ahora - v->t_alto) >= VBUS_ESTABLE_MS) {
    v->presente = 1;
    v->contando = 0;
    if (v->conexiones < 0xFFFFu) v->conexiones++;
    return 1;
  }
  return 0;
}

#endif /* __VBUS_DETECT_H */
