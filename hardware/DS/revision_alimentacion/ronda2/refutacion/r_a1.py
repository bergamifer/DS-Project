#!/usr/bin/env python3
"""Refutación de A1 — ataca los supuestos de a1_ncaps.py sin modificarlo.

Reusa el modelo del SS34 (s4_hotplug) y la curva de Samsung (a1_ncaps). Cambia, uno por vez:
  1) +5VA en su máximo de tolerancia (VFB 0,808 V, divisor al 1 %): 5,22 V en vez de 5,088 V.
     Eso sube el punto de partida de +5V_AUX antes del golpe.
  2) carga del AP2125 casi nula (AK5538 en power-down: el caso de A2) + 1 kΩ de drenaje (5 mA).
  3) C55 NO lineal: arranca descargado, y a 0 V un 4,7 µF ±10 % vale hasta 5,17 µF. Se modela
     con la forma de la curva del C45783 (cota) o como 5,17 µF lineal (cota de carga).
  4) derrateo extra de los 22 µF: frecuencia (la curva es a 120 Hz; el golpe dura µs) y
     envejecimiento de un X5R (−2,5 %/década·h, típico de clase II). Se barre ×0,9 y ×0,8.
  5) cables más largos (2,5 µH) y fuente más dura (Rhost 0,02 Ω).
Autoprueba: con los parámetros de a1_ncaps tiene que reproducir sus números (n=3 peor: 5,766 V
sin clamp); prueba negativa: con +5VA más alta el pico NO puede bajar.
"""
import sys, os, math
from multiprocessing import Pool
RA = "/Users/bergamifer/STM32CubeIDE/DS-Firmware/hardware/DS/revision_alimentacion"
sys.path.insert(0, RA)
from s4_hotplug import IS, RS, NVT, i_diodo
from a1_ncaps import dC_pct, caso as caso_orig

def caso(n, Lc=0.7e-6, Rc=0.15, Rhost=0.05, Vh=5.25, C55=4.0e-6, Cfijo=0.9e-6,
         v5va=5.088, Rfb=0.22, I_ldo=0.040, rd=0.0, vbr=6.1, tol=-0.20, temp=-0.08,
         extra=1.0, c55_nl=False, R_dren=None, t_fin=60e-6, dt=1e-9, Rf3=0.06):
    vj = NVT * math.log1p((I_ldo + (5.0 / R_dren if R_dren else 0)) / IS)
    v2 = v5va - (I_ldo + (5.0 / R_dren if R_dren else 0)) * (Rfb + RS) - vj
    iL = 0.0; v1 = 0.0; a = b = 0.0; pk = v2; t = 0.0
    while t < t_fin:
        iL += (Vh - iL * (Rhost + Rc) - v1) / Lc * dt
        icl = max(0.0, (v1 - vbr) / rd) if rd > 0 else 0.0
        i8, a = i_diodo(v1 - v2, RS + Rf3, a)
        i19, b = i_diodo(v5va - v2, RS + Rfb, b)
        Cn = n * 22e-6 * (1 + tol) * (1 + temp) * extra * (1 + dC_pct(max(v2, 0.0)) / 100)
        c55 = C55 * (1 + dC_pct(max(v1, 0.0)) / 100) if c55_nl else C55
        idr = v2 / R_dren if R_dren else 0.0
        v1 += (iL - i8 - icl) * dt / c55
        v2 += (i8 + i19 - I_ldo - idr) * dt / (Cfijo + Cn)
        pk = max(pk, v2); t += dt
    return pk

CABLES = [(L, R) for L in (0.3e-6, 0.7e-6, 1.5e-6) for R in (0.08, 0.15, 0.30)]
C55S = (2e-6, 3e-6, 4e-6)

def _w(args):
    n, L, R, c, kw = args
    return caso(n, Lc=L, Rc=R, C55=c, **kw)

def peor(pool, n, cables=CABLES, c55s=C55S, **kw):
    jobs = [(n, L, R, c, kw) for (L, R) in cables for c in c55s]
    return max(pool.map(_w, jobs))

def autoprueba(pool):
    ok = True
    # 1) mis parámetros por defecto (sin clamp, pieza peor) = a1_ncaps, columna "sin clamp", n = 3
    r = peor(pool, 3)
    ok &= abs(r - 5.766) < 0.003
    # 2) misma función que la original en un punto
    ok &= abs(caso(2, rd=0.5, tol=0, temp=0) - caso_orig(2, rd=0.5)[0]) < 1e-9
    # 3) prueba negativa: subir +5VA NO puede bajar el pico
    ok &= caso(3, v5va=5.22) > caso(3, v5va=5.088)
    print(f"autoprueba: {'OK' if ok else 'FALLA'} (n=3 peor sin clamp = {r:.3f} V; a1_ncaps dio 5,766)")
    return ok

if __name__ == "__main__":
    with Pool(12) as pool:
        assert autoprueba(pool)
        hi5va = 0.808 * (1 + 40.2 * 1.01 / (7.5 * 0.99))
        print(f"\n+5VA máx de tolerancia = {hi5va:.3f} V (VFB 0,808 V, DS41949 p.6; R150/R153 al 1 %)")
        print("Pico de +5V_AUX, peor caso sobre 9 cables × C55 2/3/4 µF, pieza −20 %/85 °C, SIN clamp\n")
        filas = [
            ("a1_ncaps tal cual (5,25 V)", dict()),
            ("+5VA 5,22 V", dict(v5va=hi5va)),
            ("+5VA 5,22 V + AK en PDN + 1 kΩ", dict(v5va=hi5va, I_ldo=0.00019, R_dren=1000.0)),
            ("22 µF ×0,9 (frecuencia/envejec.)", dict(extra=0.9)),
            ("22 µF ×0,8", dict(extra=0.8)),
            ("+5VA 5,22 V y 22 µF ×0,9", dict(v5va=hi5va, extra=0.9)),
            ("VBUS 5,5 V", dict(Vh=5.5)),
            ("VBUS 5,5 V y +5VA 5,22 V", dict(Vh=5.5, v5va=hi5va)),
        ]
        print(f" {'escenario':38s} |  n=2   |  n=3   |  n=4")
        for nom, kw in filas:
            v = [peor(pool, n, **kw) for n in (2, 3, 4)]
            print(f" {nom:38s} | {v[0]:.3f} | {v[1]:.3f} | {v[2]:.3f}")
        print("\nC55: ¿el peor caso es el más grande? (n = 3, pieza peor, sin clamp, +5VA 5,088)")
        for c in (1e-6, 2e-6, 3e-6, 4e-6, 4.7e-6, 5.17e-6):
            print(f"  C55 lineal {c*1e6:4.2f} µF → {peor(pool, 3, c55s=(c,)):.3f} V")
        r = peor(pool, 3, c55s=(5.17e-6,), c55_nl=True)
        print(f"  C55 no lineal (5,17 µF a 0 V, forma del C45783) → {r:.3f} V")
        print("\nCables fuera del barrido (n = 3, pieza peor, sin clamp)")
        cab2 = [(L, R) for L in (2.5e-6,) for R in (0.08, 0.15, 0.30)] + [(0.7e-6, 0.05)]
        print(f"  2,5 µH y 0,7 µH/0,05 Ω: {peor(pool, 3, cables=cab2):.3f} V")
        print(f"  idem con Rhost 0,02 Ω:  {peor(pool, 3, cables=cab2, Rhost=0.02):.3f} V")
