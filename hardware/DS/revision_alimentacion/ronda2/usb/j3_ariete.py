#!/usr/bin/env python3
"""¿Los 3 × C45783 de A1 también cubren el golpe de ariete en J3, si J3 recibe una resistencia de
descarga (y por lo tanto C31 arranca VACÍO, igual que C55 en J8)?

J3 y J8 son el mismo circuito pieza por pieza (verificado en componentes.tsv):
  C31 = C55 = 4,7 µF 0603 C19666 · F2 = F3 = C523825 · D2 = D8 = SS34 C8678 · U6 = U9 = USBLC6 C15999.
La resistencia de descarga (kΩ) no cambia nada en los µs del golpe (≤ 5 mA contra amperes).
Así que se reusa a1_ncaps.caso() tal cual, y se agrega el caso «sin clamp», que es lo que queda si el
pin 5 del USBLC6 se pasa de VBUS a +3V3 (propuesta de N3).

Autoprueba: reproduce la tabla de a1_ncaps (n = 3, peor pieza, sin clamp → 5,766 V).
Prueba negativa: sin capacitores agregados (n = 0, hoy) el pico TIENE que pasar los 6,0 V.
Uso: python3 -B j3_ariete.py   (tarda ~1 min)
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "..", "hardware", "DS", "revision_alimentacion"))
from a1_ncaps import caso

CABLES = [(L, R) for L in (0.3e-6, 0.7e-6, 1.5e-6) for R in (0.08, 0.15, 0.30)]
CS = (2e-6, 3e-6, 4e-6)          # C31 efectivo con la polarización (igual que C55 en a1_ncaps)

def peor(n, rd, Vh=5.25, tol=-0.20, temp=-0.08):
    return max(caso(n, Lc=L, Rc=R, C55=c, rd=rd, tol=tol, temp=temp, Vh=Vh)[0]
               for (L, R) in CABLES for c in CS)

def autoprueba():
    ok = True
    ok &= abs(peor(3, 0.0) - 5.766) < 0.005
    ok &= peor(0, 0.0) > 6.0
    print("autoprueba j3_ariete:", "OK" if ok else "FALLA")
    return ok

if __name__ == "__main__":
    assert autoprueba()
    print("J3 con C31 vacío (descarga de 1 kΩ), 20 V presentes, cable con VBUS vivo. Peor pieza (−20 %, 85 °C).")
    print(" n | VBUS 5,25 V clamp 0,5 Ω | 5,25 V sin clamp | 5,5 V clamp 2 Ω | 5,5 V sin clamp")
    for n in (0, 3):
        a, b = peor(n, 0.5), peor(n, 0.0)
        c, d = peor(n, 2.0, Vh=5.5), peor(n, 0.0, Vh=5.5)
        print(f" {n} |        {a:.3f} V          |     {b:.3f} V      |     {c:.3f} V     |    {d:.3f} V")
