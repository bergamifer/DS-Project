#!/usr/bin/env python3
"""A1 combinado: +5VA 5,226 V (tolerancia), C55 no lineal 5,17 µF a 0 V, AK en PDN + 1 kΩ,
pieza −20 %/85 °C, sin clamp, VBUS 5,25 V; con y sin ×0,9 extra. ¿Cuántos C45783 para ≤ 5,8 V?
Autoprueba heredada de r_a1.py (se corre antes)."""
import sys; sys.path.insert(0, ".")
from multiprocessing import Pool
from r_a1 import peor, autoprueba
if __name__ == "__main__":
    hi = 0.808 * (1 + 40.2 * 1.01 / (7.5 * 0.99))
    with Pool(12) as p:
        assert autoprueba(p)
        base = dict(v5va=hi, I_ldo=0.00019, R_dren=1000.0, c55_nl=True)
        for nom, kw in (("combinado", base), ("combinado ×0,9", dict(base, extra=0.9)),
                        ("combinado, VBUS 5,5 V", dict(base, Vh=5.5))):
            v = [peor(p, n, c55s=(5.17e-6,), **kw) for n in (3, 4, 5, 6)]
            print(f"{nom:24s} n=3 {v[0]:.3f} | n=4 {v[1]:.3f} | n=5 {v[2]:.3f} | n=6 {v[3]:.3f}")
