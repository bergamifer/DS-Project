#!/usr/bin/env python3
"""Pista abierta · ¿A partir de qué duración de rampa de +3V3 deja de servir el RC del EN del ESP32-C3?

Circuito (netlist DS12, red /esp32-mini/EN{slash}CHIP_PU): R8 = 10 kΩ de +3V3 a EN, C28 = 0,1 µF de EN a GND.
El pull-up sale del MISMO riel que alimenta al módulo, así que EN «persigue» a +3V3 con un retardo τ = R·C.

Rampa lineal de +3V3 de 0 a 3,3 V en T (el AP63301 tiene arranque suave tSS = 4 ms típ., DS42002 p.6, sin mín/máx).
Durante la rampa, la tensión en EN es  V_EN(t) = S·(t − τ·(1 − e^(−t/τ)))  con S = 3,3 V / T.

Dos criterios, del más estricto al más laxo (ESP32-C3 Series Datasheet v2.4, Fig.2-4 y Tabla 2-11 p.27–28;
Tabla 5-4 / DC: VIL_nRST ≤ 0,25·VDD, VIH_nRST ≥ 0,75·VDD):
  ESTRICTO: EN tiene que seguir por debajo de VIL_nRST hasta 50 µs (tSTBL) después de que el riel llega a 2,8 V.
  LAXO:     EN no puede pasar VIH_nRST (0,75·VDD) mientras VDD < 3,0 V (mínimo de operación del módulo, Tabla 9).
Para cada τ se busca la rampa más larga que todavía cumple (bisección).

Uso: python3 en_rampa.py            (autoprueba + tabla)
"""
import math, sys

VFIN = 3.3

def v_en(t, T, tau):
    """Tensión en EN a tiempo t para una rampa lineal 0→VFIN en T, y después constante."""
    S = VFIN / T
    if t <= T:
        return S * (t - tau * (1 - math.exp(-t / tau)))
    v_T = S * (T - tau * (1 - math.exp(-T / tau)))
    return VFIN - (VFIN - v_T) * math.exp(-(t - T) / tau)

def vdd(t, T):
    return min(VFIN, VFIN * t / T)

def cumple_estricto(T, tau, v_umbral_riel=2.8, t_stbl=50e-6):
    t_ok = T * v_umbral_riel / VFIN + t_stbl
    return v_en(t_ok, T, tau) <= 0.25 * vdd(t_ok, T)

def cumple_laxo(T, tau, vmin=3.0):
    t_min = T * vmin / VFIN
    return v_en(t_min, T, tau) < 0.75 * vdd(t_min, T)

def rampa_limite(criterio, tau, lo=1e-6, hi=10.0):
    """Mayor T que cumple (los dos criterios son monótonos en T: más lenta, peor)."""
    if not criterio(lo, tau):
        return 0.0
    if criterio(hi, tau):
        return float('inf')
    for _ in range(200):
        mid = math.sqrt(lo * hi)
        if criterio(mid, tau):
            lo = mid
        else:
            hi = mid
    return lo

def autoprueba():
    f = []
    # 1) Sin RC (τ → 0) EN = VDD: el estricto falla con cualquier rampa, el laxo también.
    if cumple_estricto(1e-3, 1e-9) or cumple_laxo(1e-3, 1e-9):
        f.append('con τ≈0 tendría que fallar (EN = VDD)')
    # 2) Rampa instantánea frente a τ grande: tiene que cumplir los dos.
    if not (cumple_estricto(1e-6, 10e-3) and cumple_laxo(1e-6, 10e-3)):
        f.append('rampa de 1 µs con τ=10 ms tendría que cumplir')
    # 3) Resultado analítico del laxo: V_EN/VDD = 1 − (τ/t)(1−e^(−t/τ)) = 0,75 en t/τ ≈ 3,92 →
    #    T_lím = 3,92·τ·3,3/3,0 ≈ 4,31·τ.
    x = 3.0
    for _ in range(100):           # resolver (1−e^(−x))/x = 0,25 por punto fijo
        x = (1 - math.exp(-x)) / 0.25
    esperado = x * 3.3 / 3.0 * 1e-3
    got = rampa_limite(cumple_laxo, 1e-3)
    if abs(got - esperado) / esperado > 1e-3:
        f.append('laxo: límite %.4g ≠ analítico %.4g' % (got, esperado))
    # 4) Prueba negativa: si invierto el criterio (EN debe estar ALTO), la rampa instantánea tiene que fallar.
    if not cumple_laxo(1e-6, 10e-3) is True:
        f.append('negativa mal armada')
    if (lambda T, tau: not cumple_laxo(T, tau))(1e-6, 10e-3):
        f.append('NEGATIVA: el criterio invertido no falló')
    return f

def main():
    fallas = autoprueba()
    print('AUTOPRUEBA:', 'OK' if not fallas else 'FALLA')
    for x in fallas:
        print('  ✗', x)
    if fallas:
        sys.exit(1)
    print('\nRampa más larga de +3V3 (0→3,3 V) con la que el RC del EN todavía cumple:')
    print('  %-34s %-12s %-12s' % ('RC', 'estricto', 'laxo'))
    for nombre, tau in [('10 kΩ · 0,1 µF (DS11 y DS12 hoy)', 1e-3), ('10 kΩ · 1 µF (Espressif)', 10e-3)]:
        e, l = rampa_limite(cumple_estricto, tau), rampa_limite(cumple_laxo, tau)
        print('  %-34s %-12s %-12s' % (nombre, '%.2f ms' % (e * 1e3), '%.1f ms' % (l * 1e3)))
    print('\nCon la rampa de 4 ms del AP63301 (tSS típ.), qué ve el ESP32:')
    for nombre, tau in [('0,1 µF', 1e-3), ('1 µF', 10e-3)]:
        T = 4e-3
        for v in (2.8, 3.0, 3.3):
            t = T * v / VFIN
            print('  C=%-7s VDD=%.1f V → EN=%.2f V (%.0f %% de VDD)' % (nombre, v, v_en(t, T, tau), 100 * v_en(t, T, tau) / v))
        # VDD al que EN cruza la mitad de VDD (umbral «probable» del pin, entre VIL y VIH)
        tt = 1e-7
        while v_en(tt, T, tau) < 0.5 * vdd(tt, T) and tt < 0.2:
            tt *= 1.001
        print('  C=%-7s EN cruza 0,5·VDD cuando VDD = %.2f V' % (nombre, vdd(tt, T)))
    print('\nCon la rampa de la DS11 (AP1117 detrás del MP2315S, tSS 0,8–2,2 ms, ≈1–2 ms hasta 3,3 V):')
    for T in (1e-3, 2e-3):
        print('  T=%.0f ms  laxo %s · estricto %s' % (T * 1e3, 'cumple' if cumple_laxo(T, 1e-3) else 'NO cumple',
                                                     'cumple' if cumple_estricto(T, 1e-3) else 'NO cumple'))

if __name__ == '__main__':
    main()
