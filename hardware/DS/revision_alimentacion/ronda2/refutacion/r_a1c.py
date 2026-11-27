#!/usr/bin/env python3
"""Refutación de A1.c — caída del VBUS del hub al enchufar en modo sólo-USB (sin 20 V),
con 3 × C45783 agregados en +5V_AUX.

Es s4_hotplug.simular (host "hub": 120 µF sin regulador, 50 µs) con UN cambio: la capacidad
de +5V_AUX puede ser NO lineal. Al enchufar, los 22 µF arrancan en 0 V, donde valen 22 µF
enteros (la curva de Samsung recién cae arriba de ~1 V): la carga que piden es mayor que la
de un capacitor lineal de 10,7 µF (su valor a 5 V).
Autoprueba: con Caux lineal 0,8 µF tiene que dar lo mismo que s4_hotplug.simular.
Prueba negativa: el caso no lineal tiene que pedir MÁS carga que el lineal de 3 × 10,7 µF.
"""
import sys, math
RA = "/Users/bergamifer/STM32CubeIDE/DS-Firmware/hardware/DS/revision_alimentacion"
sys.path.insert(0, RA)
from s4_hotplug import IS, RS, NVT, i_diodo, simular
from a1_ncaps import dC_pct

def sim(Caux_f, Vh=5.0, Lc=0.7e-6, Rc=0.15, Rhost=0.05, C31=3.0e-6, Rf2=0.06,
        Cb=18.5e-6, Ce=22e-6, Re=1.0, Cldo=35e-6, Chub=120e-6, t_fin=50e-6, dt=2e-9):
    iL = 0.0; v1 = v2 = v3 = vE = v4 = 0.0; vh = Vh; vj2 = vj23 = 0.0
    droop = 0.0; Q = 0.0; t = 0.0
    while t < t_fin:
        vs = vh
        iL += (vs - iL * (Rhost + Rc) - v1) / Lc * dt
        vh -= iL * dt / Chub
        droop = max(droop, Vh - vh)
        iclamp = max(0.0, (v1 - 6.1) / 0.5)
        i2, vj2 = i_diodo(v1 - v2, RS + Rf2, vj2)
        i23, vj23 = i_diodo(v2 - v3, RS, vj23)
        ilim = 0.05 + (0.36 - 0.05) * min(v4, 3.3) / 3.3
        ildo = ilim if (v4 < 3.3 and v2 - v4 > 0.07) else 0.0
        iE = (v3 - vE) / Re
        v1 += (iL - i2 - iclamp) * dt / C31
        v2 += (i2 - i23 - ildo) * dt / Caux_f(v2)
        v3 += i23 * dt / Cb - iE * dt / Cb
        vE += iE * dt / Ce
        v4 += ildo * dt / Cldo
        Q += iL * dt; t += dt
    return droop, Q

def c45783_tip(v):
    return 22e-6 * (1 + dC_pct(max(v, 0.0)) / 100)

if __name__ == "__main__":
    ok = True
    for Rc in (0.08, 0.15, 0.30):
        d, _ = sim(lambda v: 0.8e-6, Rc=Rc)
        ref = simular(Vh=5.0, host="hub", Rc=Rc, t_fin=50e-6)["droop"]
        ok &= abs(d - ref) < 2e-3
    d_lin, q_lin = sim(lambda v: 0.8e-6 + 3 * 10.7e-6)
    d_nl, q_nl = sim(lambda v: 0.8e-6 + 3 * c45783_tip(v))
    ok &= q_nl > q_lin
    print("autoprueba:", "OK" if ok else "FALLA")
    print("\nCaída del VBUS del hub (120 µF, 50 µs), VBUS 5,0 V, cable 0,7 µH")
    print(" Rc [Ω] | hoy (0,8 µF) | +3×10,7 µF lineal | +3×C45783 no lineal (típ.) | +3×C45783 +20 %")
    for Rc in (0.08, 0.15, 0.30):
        a = sim(lambda v: 0.8e-6, Rc=Rc)[0]
        b = sim(lambda v: 0.8e-6 + 3 * 10.7e-6, Rc=Rc)[0]
        c = sim(lambda v: 0.8e-6 + 3 * c45783_tip(v), Rc=Rc)[0]
        e = sim(lambda v: 0.8e-6 + 3 * 1.2 * c45783_tip(v), Rc=Rc)[0]
        print(f"  {Rc:.2f}  |   {a:.3f} V    |     {b:.3f} V       |        {c:.3f} V            | {e:.3f} V")
    # cuánto vale la carga de 0 a 4,7 V de un C45783 (capacidad promedio de gran señal)
    q = 0.0; v = 0.0; dv = 0.001
    while v < 4.7:
        q += c45783_tip(v) * dv; v += dv
    print(f"\nC45783 de 0 a 4,7 V: carga {q*1e6:.1f} µC → capacidad promedio {q/4.7*1e6:.1f} µF "
          f"(a 5 V la diferencial es {c45783_tip(5.0)*1e6:.1f} µF)")
