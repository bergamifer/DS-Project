#!/usr/bin/env python3
"""E1 — RC del pin EN del ESP32-C3-MINI-1 (R8 10 kΩ a +3V3, C28 a GND) contra la rampa de +3V3.

Criterio de Espressif (ESP32-C3 Series Datasheet v2.4, Fig. 2-4 y Tabla 2-11): tSTBL ≥ 50 µs entre que
los rieles llegan a 2,8 V y que CHIP_EN «is pulled high». Umbrales (MINI-1 v1.6, Tabla de DC):
VIL_nRST ≤ 0,25·VDD, VIH_nRST ≥ 0,75·VDD. Dos lecturas:
  estricta: EN todavía ≤ 0,25·VDD 50 µs después de que VDD cruza 2,8 V;
  laxa:     EN todavía < 0,75·VDD 50 µs después de que VDD cruza 2,8 V.
Rampa de +3V3: lineal 0 → 3,3 V en T (el AP63301 da tSS = 4 ms típ., sin mín/máx: DS42002 p.6).
"""
import math

def en_al_instante(t, T, tau, vf=3.3):
    """EN(t) para rampa lineal de duración T filtrada por un RC de constante tau (solución exacta)."""
    if t <= T:
        return vf / T * (t - tau * (1 - math.exp(-t / tau)))
    e_T = vf / T * (T - tau * (1 - math.exp(-T / tau)))
    return vf - (vf - e_T) * math.exp(-(t - T) / tau)

def cumple(T, tau, laxa):
    t28 = T * 2.8 / 3.3
    t = t28 + 50e-6
    vdd = min(3.3, 3.3 * t / T)
    return en_al_instante(t, T, tau) < (0.75 if laxa else 0.25) * vdd

def t_max(tau, laxa):
    lo, hi = 1e-7, 1.0
    if not cumple(lo, tau, laxa):
        return 0.0
    for _ in range(200):
        m = math.sqrt(lo * hi)
        if cumple(m, tau, laxa): lo = m
        else: hi = m
    return lo

def autoprueba():
    ok = True
    # 1) la solución exacta coincide con una integración numérica (Euler fino)
    T, tau, v, t, dt = 4e-3, 1e-3, 0.0, 0.0, 1e-8
    while t < 3e-3:
        v += ((3.3 * t / T) - v) / tau * dt; t += dt
    ok &= abs(v - en_al_instante(3e-3, T, tau)) < 2e-3
    # 2) prueba negativa: sin capacitor (tau ≈ 0) EN copia a VDD y la lectura estricta falla siempre
    ok &= t_max(1e-12, laxa=False) == 0.0
    # 3) más capacidad nunca puede empeorar el criterio
    ok &= t_max(1e-2, False) > t_max(1e-3, False)
    print("autoprueba:", "OK" if ok else "FALLA")
    return ok

if __name__ == "__main__":
    assert autoprueba()
    for C, et in ((0.1e-6, "0,1 µF (hoy)"), (1e-6, "1 µF (E1)")):
        for tol in (-0.2, 0.0):
            tau = 10e3 * C * (1 + tol)
            print(f"{et:13s} C {tol*100:+.0f} %: rampa máx. estricta {t_max(tau, False)*1e3:6.2f} ms | laxa {t_max(tau, True)*1e3:6.2f} ms"
                  f" | con rampa de 4 ms: estricta {'sí' if cumple(4e-3, tau, False) else 'NO'}, laxa {'sí' if cumple(4e-3, tau, True) else 'NO'}")
    # Pulsar K1 (EN a GND) y soltarlo: cuánto tarda EN en volver a 0,75·VDD
    for C in (0.1e-6, 1e-6):
        print(f"K1 soltado, C = {C*1e6:.1f} µF: EN llega a 0,75·VDD en {10e3*C*math.log(4)*1e3:.1f} ms;"
              f" energía que descarga el pulsador: {0.5*C*3.3**2*1e6:.2f} µJ")
