#!/usr/bin/env python3
"""Fila 2 del Bloque B: ¿dónde cruza el lazo del AP63356 (IC4) con ~1,2–1,6 mF de salida, y con qué margen de fase?

LA HOJA NO DA LA RED DE COMPENSACIÓN INTERNA (COMP a GND, DS41949 p.3: «Connect the COMP pin to GND to use internal loop
compensation»). Sí da, para la compensación EXTERNA (p.20–23): gm = 0,3 mS, RT = 0,2 V/A, VFB = 0,8 V, Eq.16
R5 = 2*pi*fc*VOUT*COUT*RT/(gm*VFB), Eq.17 C5 = VOUT*COUT/(IOUT*R5), y un ejemplo (5 V, 3,5 A, COUT efectiva 36 uF, fc 45 kHz)
cuya Tabla 2 lista R5 = 38 k y C5 = 3,3 nF. Este script SUPONE que la red interna se parece a alguna de esas, y lo dice.

Modelo (modo corriente pico, primer orden, el de la Fig.41 de la hoja):
  T(s) = K(s) * gm*Zc(s) * (1/RT) * Zout(s) * Hs(s)
  K(s)   divisor R150/R153 con Cff C318 (220 pF):  R2/(R1+R2) * (1+s*R1*Cff)/(1+s*(R1||R2)*Cff)
  Zc(s)  compensador tipo II sin C6:                (1+s*R5*C5)/(s*C5)
  Zout   Ro || (ESR + 1/(s*C))  por cada grupo de capacitores
  Hs(s)  muestreo a fsw/2 (Ridley): 1/(1 + s/(wn*Q) + s^2/wn^2), wn = pi*fsw, Q = 2/pi (supuesto)
Capacidad de +5VA: polímero 5x220 uF C25503905, ESR 24 mOhm a 100 kHz c/u (hoja ChuangHui, «not to exceed»), más cerámicos
(~140 uF nominales en +5VA, ~60–100 uF efectivos) y los 48 uF de +5VA_DAC (FB3 es transparente a 1 kHz).

python3 -I b2_lazo.py
"""
import cmath, math

GM, RT, VFB, FSW = 0.3e-3, 0.2, 0.8, 450e3
R1, R2, CFF = 40.2e3, 7.5e3, 220e-12

def T(f, comp, grupos, Ro, R1=R1, R2=R2, Cff=CFF, vout=None):
    s = 2j * math.pi * f
    R5, C5 = comp
    K = R2 / (R1 + R2) * (1 + s * R1 * Cff) / (1 + s * (R1 * R2 / (R1 + R2)) * Cff) if Cff else R2 / (R1 + R2)
    Zc = (1 + s * R5 * C5) / (s * C5)
    Y = 1 / Ro
    for C, esr in grupos:
        Y += 1 / (esr + 1 / (s * C))
    wn, Q = math.pi * FSW, 2 / math.pi
    Hs = 1 / (1 + s / (wn * Q) + (s / wn) ** 2)
    return K * GM * Zc * (1 / RT) * (1 / Y) * Hs

def cruce(comp, grupos, Ro, **kw):
    lo, hi = 10.0, 2e5
    for _ in range(200):
        m = math.sqrt(lo * hi)
        if abs(T(m, comp, grupos, Ro, **kw)) > 1: lo = m
        else: hi = m
    fc = math.sqrt(lo * hi)
    pm = 180 + math.degrees(cmath.phase(T(fc, comp, grupos, Ro, **kw)))
    return fc, pm

# ---- autoprueba: el ejemplo de la propia hoja (p.23) tiene que cruzar cerca de 45 kHz ----
ej_R5 = 2 * math.pi * 45e3 * 5 * 36e-6 * RT / (GM * VFB)       # Eq.16
ej_C5 = 5 * 36e-6 / (3.5 * ej_R5)                              # Eq.17
assert abs(ej_R5 - 42.4e3) < 0.3e3, ej_R5
fc_ej, pm_ej = cruce((ej_R5, ej_C5), [(36e-6, 1e-3)], 5 / 3.5, R1=157e3, R2=30e3, Cff=0)
assert 38e3 < fc_ej < 52e3, fc_ej
print(f"autoprueba OK: ejemplo de la hoja (R5 {ej_R5/1e3:.1f} k, C5 {ej_C5*1e9:.2f} nF, 36 uF) cruza en {fc_ej/1e3:.1f} kHz, MF {pm_ej:.0f}°")
# la cifra heredada sale de invertir Eq.16 (cruce en la zona plana del compensador) con 1,15 mF:
fc_her = GM * ej_R5 * VFB / (2 * math.pi * RT * 5 * 1.15e-3)
assert 1.35e3 < fc_her < 1.45e3, fc_her
fc_her_full, pm_her_full = cruce((ej_R5, ej_C5), [(1.15e-3, 1e-3)], 5 / 3.5, R1=157e3, R2=30e3, Cff=0)
print(f"              la cifra heredada (1,4 kHz) es Eq.16 invertida con 1,15 mF: {fc_her/1e3:.2f} kHz; pero ese cruce cae")
print(f"              DEBAJO del cero del compensador (3,1 kHz) y el modelo completo da {fc_her_full/1e3:.2f} kHz, MF {pm_her_full:.0f}°")
# ---- prueba negativa: un cero del compensador 10 veces por encima del cruce TIENE que dar poco margen ----
_, pm_neg = cruce((42.4e3, 1 / (2 * math.pi * 42.4e3 * 50e3)), [(1.29e-3, 1e-4)], 11.0, Cff=0)
assert pm_neg < 20, pm_neg
print(f"prueba negativa OK: con el cero del compensador en 50 kHz, sin ESR ni Cff, el margen cae a {pm_neg:.0f}°\n")

hip = {
    "H1 Tabla 2 (R5 38 k, C5 3,3 nF, cero 1,27 kHz)": (38e3, 3.3e-9),
    "H2 Eq.16/17 del ejemplo (R5 42,4 k, C5 1,21 nF, cero 3,1 kHz)": (ej_R5, ej_C5),
    "H3 diseñada para la Tabla 1 (2x22 uF ~ 30 uF efectivos, fc 45 kHz)": (
        2 * math.pi * 45e3 * 5 * 30e-6 * RT / (GM * VFB), 5 * 30e-6 / (3.5 * 2 * math.pi * 45e3 * 5 * 30e-6 * RT / (GM * VFB))),
}
salidas = {
    "C mín (976 uF)":     [(880e-6, 24e-3 / 5), (96e-6, 5e-3)],
    "C nominal (1289 uF)": [(1100e-6, 24e-3 / 5), (189e-6, 5e-3)],
    "C máx (1583 uF)":    [(1320e-6, 24e-3 / 5), (263e-6, 5e-3)],
}
for hn, comp in hip.items():
    fz = 1 / (2 * math.pi * comp[0] * comp[1])
    print(f"{hn}   [cero del compensador {fz/1e3:.2f} kHz]")
    for sn, g in salidas.items():
        for Ro, rn in ((11.0, "reposo ~0,47 A"), (1.7, "3 A")):
            fc, pm = cruce(comp, g, Ro)
            print(f"   {sn:20s} {rn:15s} cruce {fc/1e3:5.2f} kHz   margen de fase {pm:4.0f}°")
# Efecto de la ESR del polímero: cero en 1/(2*pi*ESR*C)
print(f"\nCero de ESR del polímero: 1/(2*pi*24 mOhm*220 uF) = {1/(2*math.pi*24e-3*220e-6)/1e3:.1f} kHz -> a 1,2 kHz aporta "
      f"{math.degrees(math.atan(1.2/30.1)):.1f}°")
comp = hip["H2 Eq.16/17 del ejemplo (R5 42,4 k, C5 1,21 nF, cero 3,1 kHz)"]
for esr in (24e-3, 100e-3, 300e-3):
    fc, pm = cruce(comp, [(1100e-6, esr / 5), (189e-6, 5e-3)], 11.0)
    print(f"  H2, nominal, ESR por capacitor {esr*1e3:3.0f} mOhm: cruce {fc/1e3:.2f} kHz, margen {pm:.0f}°")
# Alternativa que la hoja SÍ permite calcular: compensación externa en COMP (Eq.16/17) para la capacidad real
for fc_obj in (5e3, 10e3):
    r5 = 2 * math.pi * fc_obj * 5.088 * 1.29e-3 * RT / (GM * VFB); c5 = 5.088 * 1.29e-3 / (3.0 * r5)
    fc, pm = cruce((r5, c5), salidas["C nominal (1289 uF)"], 11.0)
    fc2, pm2 = cruce((r5, c5), salidas["C máx (1583 uF)"], 1.7)
    print(f"\nCOMP externo para fc {fc_obj/1e3:.0f} kHz: R5 {r5/1e3:.0f} k, C5 {c5*1e9:.1f} nF -> nominal/reposo {fc/1e3:.1f} kHz {pm:.0f}°;"
          f" máx/3 A {fc2/1e3:.1f} kHz {pm2:.0f}°")
