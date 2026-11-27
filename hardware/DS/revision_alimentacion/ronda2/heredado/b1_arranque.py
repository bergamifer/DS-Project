#!/usr/bin/env python3
"""Fila 1 del Bloque B: ¿el buck A (IC4, AP63356) entra en protección al cargar su capacidad de salida en el arranque?

Mecanismo (DS41949 Rev 3-2): durante el arranque suave (tSS = 4 ms TÍPICO, p.6, sin mín/máx) la referencia sube en rampa y
la salida la sigue; la bobina tiene que entregar la corriente que carga los capacitores, C*VOUT/tSS, más la de la carga.
El pico de corriente de la bobina es ese promedio más medio rizado. Si el pico pasa IPEAK_LIMIT (4,0/5,0/6,0 A, p.6), el
chip corta el ciclo (ciclo a ciclo); si eso pasa 512 ciclos seguidos entra en «hiccup»: se apaga 8192 ciclos y rearranca
(p.16, §7 Overcurrent Protection).

Capacidad (netlist .ds12_audit, verificada): +5VA: 5 x 220 uF C25503905 (polímero, ±20 %) + 2 x 22 uF C45783 + 8 x 10 uF C1691
+ 16 x 1 uF + 100 nF; detrás de FB3 (+5VA_DAC): 4 x 10 uF C134745 (±20 %) + 8 x 1 uF; detrás de FB69+D19 (+5V_AUX): C57 1 uF + C59
100 nF (+ 3 x C45783 si se adopta A1).
Cargas durante el arranque:
  PAM8302A con SD en bajo: ISHDN <= 1 uA c/u (DS41333 p.4) -> 32 uA en total.
  PCM1690 VCC: 170 uA en power-down con relojes parados; 74 típ / 110 máx mA con fs = 48 kHz (SBAS448B, tabla de alimentación).
  AK5538 AVDD+VREFH: 38 típ / 50 máx mA (AK5538VN p.~12), por U10 desde +5V_AUX.
  Expansión: F6/F7 = C883126, 500 mA de hold c/u (description del netlist) -> hasta ~1 A.
Rizado: dIL = (VIN-VOUT)*VOUT/(VIN*L*fsw); L1 6,8 uH ±20 % (Sunlord p.8), fsw 400–500 kHz (DS41949 p.6), VIN 15–21 V.

python3 -I b1_arranque.py
"""
import itertools

ILIM_MIN, ILIM_TIP, ILIM_MAX = 4.0, 5.0, 6.0

def rizado(vin, vout, L, fsw):
    return (vin - vout) * vout / (vin * L * fsw)

def pico(C, vout, tss, iload, vin, L, fsw):
    icarga = C * vout / tss
    return icarga, icarga + iload + rizado(vin, vout, L, fsw) / 2

def peor_rizado():
    return max(rizado(v, vo, L, f) for v, vo, L, f in itertools.product((15, 21), (4.953, 5.226), (6.8e-6 * .8, 6.8e-6 * 1.2), (400e3, 500e3)))

# ---- capacidades (uF): (mín, nominal, máx) ----
C = {
    "polímero 5x220 (C25503905, ±20 %)": (880, 1100, 1320),
    "electrolíticos +5VA_DAC 4x10 (C134745, ±20 %)": (32, 40, 48),
    "2x22 C45783 (a 5 V conserva 10,7 c/u; máx = nominal)": (21.4, 44, 44),
    "8x10 C1691 0603 (con DC bias; máx = nominal)": (30, 80, 80),
    "24x1 uF C15849 + 100 nF (máx = nominal)": (12, 24.1, 24.1),
    "+5V_AUX hoy: C57 + C59": (1.0, 1.1, 1.1),
}
A1 = (32.1, 66, 66)  # 3 x C45783 propuestos en +5V_AUX (10,7 uF c/u a 5 V)
cmin = sum(v[0] for v in C.values()); cnom = sum(v[1] for v in C.values()); cmax = sum(v[2] for v in C.values())
cmaxA1 = cmax + A1[2]

# ---- autoprueba: la cuenta heredada ----
i_her, _ = pico(1.15e-3, 5.088, 4e-3, 0, 20, 6.8e-6, 450e3)
assert abs(i_her - 1.46) < 0.01, i_her          # «1,15 mF en 4 ms con unos 1,5 A»
# el netlist da 1240 uF en +5VA (sin +5VA_DAC ni +5V_AUX)
assert abs((1100 + 44 + 80 + 16 + 0.1) - 1240.1) < 0.01
# rizado nominal (20 V, 6,8 uH, 450 kHz) = 1,24 App
assert abs(rizado(20, 5.088, 6.8e-6, 450e3) - 1.24) < 0.01

# ---- prueba negativa: con un arranque suave de 1 ms el mismo circuito SÍ tiene que pasar el límite ----
_, p_neg = pico(cmax * 1e-6, 5.226, 1e-3, 0.0, 21, 6.8e-6 * .8, 400e3)
assert p_neg > ILIM_MIN, p_neg
print(f"autoprueba OK (heredado: {i_her:.2f} A); prueba negativa OK: con tSS = 1 ms el pico sería {p_neg:.1f} A > {ILIM_MIN} A\n")

dil = peor_rizado()
print(f"Capacidad que carga el buck A: mín {cmin:.0f} / nominal {cnom:.0f} / máx {cmax:.0f} uF  (+ A1: máx {cmaxA1:.0f} uF)")
print(f"Rizado de L1: nominal {rizado(20, 5.088, 6.8e-6, 450e3):.2f} App; peor {dil:.2f} App (21 V, 5,44 uH, 400 kHz)\n")

escenarios = [
    ("nominal: firmware sin correr (DAC en power-down), sin expansión", cnom, 5.088, 0.000032 + 4 * 0.00017 + 0.038, False),
    ("peor C y VOUT, DAC en power-down, sin expansión, con A1", cmaxA1, 5.226, 0.000032 + 4 * 0.00017 + 0.050, True),
    ("peor + 4 DAC con reloj (4x110 mA)  [rearranque tras UVLO con firmware andando]", cmaxA1, 5.226, 0.000032 + 0.44 + 0.050, True),
    ("ídem + 32 PAM con SD en alto, sin audio (32x8 mA máx)", cmaxA1, 5.226, 0.44 + 0.050 + 0.256, True),
    ("peor, DAC en power-down, expansión tomando 1 A", cmaxA1, 5.226, 0.000032 + 0.0007 + 0.050 + 1.0, True),
]
print(f"{'escenario':78s} {'Icarga':>7s} {'Ipico':>7s}  margen a 4,0 A   tSS que lo lleva a 4,0 A")
for nombre, c_uF, vo, il, peor in escenarios:
    rz = dil if peor else rizado(20, 5.088, 6.8e-6, 450e3)
    ic = c_uF * 1e-6 * vo / 4e-3
    ip = ic + il + rz / 2
    tss_crit = c_uF * 1e-6 * vo / (ILIM_MIN - il - rz / 2)
    print(f"{nombre:78s} {ic:6.2f}A {ip:6.2f}A  {ILIM_MIN - ip:+5.2f} A        {tss_crit * 1e3:4.2f} ms")
print("\nSin protección si Ipico < 4,0 A (mínimo de IPEAK_LIMIT). La hoja no da tolerancia de tSS: la última columna dice")
print("cuánto podría acortarse antes de llegar al límite.")
