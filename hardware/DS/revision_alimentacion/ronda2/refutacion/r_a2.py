#!/usr/bin/env python3
"""Refutación de A2 — reusa a2_drenaje.py (Fig.2 del SS34 de MDD) sin modificarlo.

1) ¿Es "máximo especificado" la envolvente de a2 a alta temperatura? La tabla de MDD da 5 mA
   a 125 °C / 40 V; la Fig.2 (TÍPICA) ya da ~3 mA a 100 °C / 40 V. Se compara.
2) Temperatura a la que la fuga TÍPICA de D23 alcanza lo que toma 1 kΩ y 10 kΩ a 5 V.
3) Problema nuevo: la fuga de D8 (sin cable en J8) sale de +5V_AUX hacia VBUS y sólo encuentra
   R161 + R160 = 162 kΩ a GND. VBUS sube y PA5 (divisor 100/162) puede leer "hay host".
   Modelo de la fuga a baja tensión inversa: I = I_fig2(T, VR) · (1 − e^(−VR/(n·VT))), n = 1,1.
Autoprueba: reproduce la tabla de a2 (1,6 / 97 / 630 µA a 14,5 V) y, sin fuga, PA5 = 0 V.
Prueba negativa: con 10 veces más fuga, PA5 NO puede bajar.
"""
import sys, math
RA = "/Users/bergamifer/STM32CubeIDE/DS-Firmware/hardware/DS/revision_alimentacion"
sys.path.insert(0, RA)
from a2_drenaje import fuga_tipica_uA, fuga_max_uA, FIG2, VR_RATED

def fuga_baja_vr_uA(T, VR, escala=1.0):
    if VR <= 0: return 0.0
    return escala * fuga_tipica_uA(T, max(VR, 1e-3)) * (1 - math.exp(-VR / (1.1 * 0.02585 * (273.15 + T) / 300)))

def nodo_vbus(T, v_aux=4.85, R=162e3, escala=1.0):
    """V del nodo VBUS de J8 sin cable: fuga(v_aux − V) · R = V (bisección)."""
    lo, hi = 0.0, v_aux
    for _ in range(80):
        m = 0.5 * (lo + hi)
        if fuga_baja_vr_uA(T, v_aux - m, escala) * 1e-6 * R > m: lo = m
        else: hi = m
    return 0.5 * (lo + hi)

def cruce(i_uA, VR):
    for T10 in range(250, 1600):
        T = T10 / 10
        if fuga_tipica_uA(T, VR) >= i_uA: return T
    return None

if __name__ == "__main__":
    ok = True
    ok &= abs(fuga_tipica_uA(25, 14.5) - 1.6) < 0.06 and abs(fuga_tipica_uA(75, 14.5) - 97) < 1.5
    ok &= abs(fuga_tipica_uA(100, 14.5) - 630) < 10
    ok &= nodo_vbus(60, escala=0.0) < 1e-6
    ok &= nodo_vbus(50, escala=10.0) > nodo_vbus(50)
    print("autoprueba:", "OK" if ok else "FALLA")

    print("\n1) ¿La envolvente de 'máximo' de a2 está por encima de la típica? (VR 14,5 V)")
    for T in (25, 60, 85, 100, 110, 125):
        print(f"   {T:3d} °C: típica {fuga_tipica_uA(T, 14.5):8.0f} µA | 'máx esp.' de a2 {fuga_max_uA(T, 14.5):8.0f} µA")
    print(f"   Fig.2 típica a 100 °C y 40 V: {FIG2[100][-1][1]} µA; tabla MDD máx a 125 °C y 40 V: 5000 µA;"
          f" típica extrapolada a 125 °C y 40 V: {fuga_tipica_uA(125, 40):.0f} µA")

    print("\n2) Temperatura a la que la fuga TÍPICA de D23 iguala lo que toma la R a 5,0 V")
    for VR in (14.5, 15.5):
        print(f"   VR {VR} V: 1 kΩ (5 mA) → {cruce(5000, VR)} °C · 10 kΩ (0,5 mA) → {cruce(500, VR)} °C")

    print("\n3) J8 sin cable, 20 V puestos, +5V_AUX 4,85 V: nodo VBUS y PA5 por la fuga de D8")
    print("   VIL máx = 0,99 V, VIH mín = 2,31 V (DS12110 Tabla 60, VDD 3,3 V)")
    for T in (25, 40, 50, 60, 70, 85):
        v = nodo_vbus(T); vm = nodo_vbus(T, escala=fuga_max_uA(25, 40) / fuga_tipica_uA(25, 40))
        print(f"   {T:3d} °C: típica VBUS {v:4.2f} V → PA5 {v*100/162:4.2f} V | con la fuga máx de tabla (×{fuga_max_uA(25,40)/fuga_tipica_uA(25,40):.0f}) PA5 {vm*100/162:4.2f} V")
