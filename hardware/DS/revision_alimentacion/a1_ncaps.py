#!/usr/bin/env python3
"""A1 (ronda 2) — ¿cuántos C45783 hacen falta en +5V_AUX para que el golpe de ariete de J8
(S4.f) no pase de 5,8 V?

Mismo circuito que s4f_verif.py (J8 con 20 V presentes, cable con VBUS vivo, C55 descargado,
+5V_AUX sostenido por +5VA → FB69 → D19, D23 en reverso), pero el capacitor agregado es
NO LINEAL: C45783 = Samsung CL21A226MAQNNNE (22 µF, 25 V, X5R, 0805, ±20 %), con su curva
ΔC(V) de Samsung (weblib.samsungsem.com, 25 °C, 120 Hz, 0,5 Vrms; guardada en
C45783_dcbias.json, al lado de este script). Durante el pico la tensión sube y la capacidad BAJA (−51 % a 5 V,
−65 % a 7 V): el capacitor se ablanda justo cuando más se lo necesita. Por eso no alcanza
con dividir 22 µF por la capacidad a 5 V.

Peor caso de la pieza: tolerancia −20 % y −8 % por temperatura (X5R a 85 °C, misma fuente).
"""
import json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from s4_hotplug import IS, RS, NVT, i_diodo          # modelo del SS34 (MDD Fig.3)

_d = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "C45783_dcbias.json")))
_X, _Y = _d["x"], _d["y"]

def dC_pct(v):
    """ΔC % de CL21A226MAQNNN a v voltios (interpolación lineal; fuera de rango, extremos)."""
    if v <= _X[0]: return _Y[0]
    if v >= _X[-1]: return _Y[-1]
    lo, hi = 0, len(_X) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if _X[mid] <= v: lo = mid
        else: hi = mid
    t = (v - _X[lo]) / (_X[lo + 1] - _X[lo])
    return _Y[lo] + t * (_Y[lo + 1] - _Y[lo])

def c45783(v, tol=0.0, temp=0.0):
    """Capacidad diferencial aproximada de UN C45783 (µF→F). tol, temp en fracción (−0,2, −0,08)."""
    return 22e-6 * (1 + tol) * (1 + temp) * (1 + dC_pct(max(v, 0.0)) / 100)

def caso(n, Lc=0.7e-6, Rc=0.15, Rhost=0.05, Vh=5.25, C55=4.0e-6, Cfijo=0.9e-6,
         v5va=5.088, Rfb=0.22, I_ldo=0.040, rd=0.5, vbr=6.1, tol=0.0, temp=0.0,
         t_fin=60e-6, dt=1e-9, Rf3=0.06, lineal_uF=None):
    """Pico de +5V_AUX con n capacitores C45783 (o, si lineal_uF, un capacitor lineal de ese valor)."""
    vj = NVT * math.log1p(I_ldo / IS)
    v2 = v5va - I_ldo * (Rfb + RS) - vj
    v2_0 = v2; iL = 0.0; v1 = 0.0; a = b = 0.0; pk = v2; t = 0.0
    while t < t_fin:
        iL += (Vh - iL * (Rhost + Rc) - v1) / Lc * dt
        icl = max(0.0, (v1 - vbr) / rd) if rd > 0 else 0.0
        i8, a = i_diodo(v1 - v2, RS + Rf3, a)
        i19, b = i_diodo(v5va - v2, RS + Rfb, b)
        Cn = (lineal_uF * 1e-6) if lineal_uF is not None else n * c45783(v2, tol, temp)
        v1 += (iL - i8 - icl) * dt / C55
        v2 += (i8 + i19 - I_ldo) * dt / (Cfijo + Cn)
        pk = max(pk, v2); t += dt
    return pk, v2_0

def autoprueba():
    ok = True
    # 1) la curva de Samsung pasa por los puntos leídos de la fuente (0 V → 0 %, 5 V → −51,3 %)
    ok &= abs(dC_pct(0.0)) < 1e-9 and abs(dC_pct(5.0) + 51.3) < 0.2
    # 2) con n = 0 tiene que reproducir s4f_verif.py (0,7 µH / 0,15 Ω / clamp 0,5 Ω → 6,592 V)
    pk, _ = caso(0)
    ok &= abs(pk - 6.592) < 0.01
    # 3) con 22 µF LINEALES en total (sin los 0,9 µF fijos) tiene que reproducir s4f_mitig (5,334 V)
    pk, _ = caso(0, lineal_uF=22.0, Cfijo=0.0)
    ok &= abs(pk - 5.334) < 0.01
    # 4) prueba negativa: la versión no lineal de 1 × 22 µF TIENE que dar un pico mayor que la lineal de 22 µF
    pk_nl, _ = caso(1)
    ok &= pk_nl > 5.334 + 0.1
    print("autoprueba:", "OK" if ok else "FALLA")
    return ok

if __name__ == "__main__":
    assert autoprueba()
    cables = [(L, R) for L in (0.3e-6, 0.7e-6, 1.5e-6) for R in (0.08, 0.15, 0.30)]
    print("\nPico de +5V_AUX (peor caso sobre 9 cables y C55 efectivo 2/3/4 µF), VBUS 5,25 V")
    print(" n  pieza                 | clamp U9 0,5 Ω | clamp 2 Ω | sin clamp")
    for n in (1, 2, 3, 4, 5):
        for etiqueta, tol, temp in (("típica, 25 °C        ", 0.0, 0.0),
                                    ("−20 %, 85 °C (peor)  ", -0.20, -0.08)):
            fila = []
            for rd in (0.5, 2.0, 0.0):
                peor = max(caso(n, Lc=L, Rc=R, C55=c, rd=rd, tol=tol, temp=temp)[0]
                           for (L, R) in cables for c in (2e-6, 3e-6, 4e-6))
                fila.append(peor)
            print(f" {n}  {etiqueta} |    {fila[0]:.3f} V    |  {fila[1]:.3f} V  |  {fila[2]:.3f} V")
    print("\nSensibilidad: VBUS 5,5 V (máximo de vSafe5V), pieza en el peor caso, clamp 2 Ω")
    for n in (2, 3, 4):
        peor = max(caso(n, Lc=L, Rc=R, C55=c, rd=2.0, tol=-0.20, temp=-0.08, Vh=5.5)[0]
                   for (L, R) in cables for c in (2e-6, 3e-6, 4e-6))
        print(f"  n = {n}: {peor:.3f} V")
