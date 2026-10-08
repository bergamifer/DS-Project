#!/usr/bin/env python3
"""A1' — re-corre el caso combinado de r_a1_comb.py (sin modificarlo; se importa r_a1.py) y agrega:
  1) qué fila corresponde a los números citados (6,00 / 5,80 / 5,67): ¿con o sin ×0,9?
  2) efecto de U2 sobre el golpe de J8: hoy U9 tiene el pin 5 en VBUS (clamp ~6,1 V, rd 0,5–2 Ω);
     con U2 el clamp desaparece. El caso combinado ya es SIN clamp: se verifica que no cambia.
  3) alternativa: C55 = 2,2 µF (C23630, ya en el BOM) en vez de 4,7 µF.
Autoprueba: la de r_a1 (reproduce a1_ncaps: n = 3 peor sin clamp = 5,766 V) + prueba negativa propia:
con clamp el pico NO puede ser mayor que sin clamp.
"""
import sys
sys.path.insert(0, "/Users/bergamifer/STM32CubeIDE/DS-Firmware/.ds12_audit/r2/refutacion")
from multiprocessing import Pool
from r_a1 import peor, autoprueba, caso

if __name__ == "__main__":
    hi = 0.808 * (1 + 40.2 * 1.01 / (7.5 * 0.99))
    base = dict(v5va=hi, I_ldo=0.00019, R_dren=1000.0, c55_nl=True)
    with Pool(12) as p:
        assert autoprueba(p)
        neg = caso(3, rd=0.5, **base, C55=5.17e-6) <= caso(3, rd=0.0, **base, C55=5.17e-6) + 1e-9
        print("prueba negativa (clamp no puede empeorar):", "OK" if neg else "FALLA"); assert neg
        filas = [("combinado (sin ×0,9)", base, (5.17e-6,)),
                 ("combinado ×0,9", dict(base, extra=0.9), (5.17e-6,)),
                 ("combinado ×0,9, clamp U9 0,5 Ω (hoy)", dict(base, extra=0.9, rd=0.5), (5.17e-6,)),
                 ("combinado ×0,9, clamp U9 2 Ω (hoy)", dict(base, extra=0.9, rd=2.0), (5.17e-6,)),
                 ("combinado ×0,9, C55 = 2,2 µF +10 %", dict(base, extra=0.9), (2.42e-6,)),
                 ("combinado ×0,9, VBUS 5,5 V", dict(base, extra=0.9, Vh=5.5), (5.17e-6,))]
        print(f"\n {'caso':40s} |  n=3  |  n=4  |  n=5")
        for nom, kw, c55s in filas:
            v = [peor(p, n, c55s=c55s, **kw) for n in (3, 4, 5)]
            print(f" {nom:40s} | {v[0]:.3f} | {v[1]:.3f} | {v[2]:.3f}")
