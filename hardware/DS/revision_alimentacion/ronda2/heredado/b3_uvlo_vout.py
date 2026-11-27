#!/usr/bin/env python3
"""Fila 3 del Bloque B: UVLO del buck A (IC4, AP63356) y tensiones de +5VA y +3V3, en peor caso.

Modelo (DS41949 Rev 3-2, p.15, Fig.37 y Eq.2/Eq.3; verificado contra las ecuaciones de la hoja en la autoprueba):
  apagado -> encendido: en EN entra I1 (1,5 uA). Enciende cuando V(EN) llega a VEN_H.
      (VON - VEN_H)/R3 + I1 = VEN_H/R4   ->   VON  = VEN_H*(1 + R3/R4) - I1*R3
  encendido -> apagado: entra I1 + I2 (5,5 uA). Apaga cuando V(EN) cae a VEN_L.
      VOFF = VEN_L*(1 + R3/R4) - (I1+I2)*R3
  Salida: VOUT = VFB*(1 + Rarriba/Rabajo).

Datos (todos con cita):
  R151 = R3 = 470 k (C23178, UNI-ROYAL 0603WAF4703T5E, +-1 %), R156 = R4 = 40,2 k (C2933097, FOJAN FRC0402F4022TS, +-1 %)
  R150 = 40,2 k (C2933097, +-1 %), R153 = 7,5 k (C23234, +-1 %)        -> +5VA
  R155 = 62 k (C37825, +-1 %),    R154 = 20 k (C25765, +-1 %)          -> +3V3
  (valores: netlist .ds12_audit/componentes.tsv; tolerancias: título de lcsc.com/product-detail/<LCSC>.html)
  AP63356 (DS41949 p.6): VEN_H 1,15/1,18/1,21 V; VEN_L 1,02/1,08/1,14 V; VFB 0,792/0,800/0,808 V;
                         IEN 1,5 uA típ @1 V (= I1) y 5,5 uA típ @1,5 V (= I1+I2): SIN mín/máx.
  AP63301 (DS42002 p.5): VFB 792/800/808 mV; IEN @1 V 1/1,5/2 uA (mín/típ/máx) -> único dato de tolerancia de I1 en la familia.

Uso:  python3 -I b3_uvlo_vout.py          (corre autoprueba + prueba negativa + resultados)
"""
import itertools

# ---------- modelo ----------
def von(R3, R4, VH, I1):
    return VH * (1 + R3 / R4) - I1 * R3

def voff(R3, R4, VL, Itot):
    return VL * (1 + R3 / R4) - Itot * R3

def vout(Rt, Rb, VFB):
    return VFB * (1 + Rt / Rb)

# Ecuaciones de la hoja (Eq.2 y Eq.3), sólo para la autoprueba
def eq2_R3(VON, VOFF):
    return (0.915 * VON - VOFF) / 4.127e-6

def eq3_R4(R3, VOFF):
    return 1.08 * R3 / (VOFF - 1.08 + 5.5e-6 * R3)

def extremos(f, rangos):
    """min y max de f sobre todas las esquinas de los rangos (f es monótona en cada variable)."""
    vals = [f(*c) for c in itertools.product(*rangos)]
    return min(vals), max(vals)

def tol(x, t):
    return (x * (1 - t), x * (1 + t))

R3, R4 = 470e3, 40.2e3
I1, I12 = 1.5e-6, 5.5e-6

# ---------- autoprueba: reproducir los nominales del documento ----------
n_on, n_off = von(R3, R4, 1.18, I1), voff(R3, R4, 1.08, I12)
n_5va, n_3v3 = vout(40.2e3, 7.5e3, 0.800), vout(62e3, 20e3, 0.800)
assert round(n_on, 1) == 14.3 and round(n_off, 1) == 11.1, (n_on, n_off)
assert abs(n_5va - 5.088) < 0.0005 and abs(n_3v3 - 3.280) < 0.0005, (n_5va, n_3v3)
# el modelo coincide con las ecuaciones de la hoja: Eq.2/Eq.3 con mis VON/VOFF devuelven 470 k / 40,2 k
assert abs(eq2_R3(n_on, n_off) - R3) / R3 < 0.002, eq2_R3(n_on, n_off)
assert abs(eq3_R4(R3, n_off) - R4) / R4 < 0.002, eq3_R4(R3, n_off)
# y los rangos que heredó el artifact salen de mover SÓLO los umbrales de EN
a_on = extremos(lambda vh: von(R3, R4, vh, I1), [(1.15, 1.21)])
a_off = extremos(lambda vl: voff(R3, R4, vl, I12), [(1.02, 1.14)])
assert (round(a_on[0], 1), round(a_on[1], 1)) == (13.9, 14.7), a_on
assert (round(a_off[0], 1), round(a_off[1], 1)) == (10.4, 11.9), a_off

# ---------- prueba negativa: cosas que TIENEN que dar distinto ----------
# (a) sin las corrientes de EN, el modelo ya no reproduce 14,3 V (da 14,98): la autoprueba no es tautológica
assert round(von(R3, R4, 1.18, 0.0), 1) != 14.3
# (b) divisor de +3V3 invertido (20 k arriba, 62 k abajo): 1,06 V, como dice el artifact
assert round(vout(20e3, 62e3, 0.8), 2) == 1.06
# (c) si alguien usara 0,791 V (el MP2315S de la DS11) +5VA daría 5,031 V, no 5,088
assert round(vout(40.2e3, 7.5e3, 0.791), 3) == 5.031

print("autoprueba OK: nominales 14,27 / 11,12 V, 5,088 V, 3,280 V; Eq.2/Eq.3 devuelven 470 k / 40,2 k;")
print("               el rango heredado 13,9–14,7 / 10,4–11,9 V sale de mover sólo VEN_H/VEN_L.\n")

# ---------- peor caso ----------
R1p = 0.01
casos = {
    "B: umbrales EN + R ±1 % (I1, I2 típicas)": (tol(I1, 0), tol(I12, 0)),
    "C: + I1 ±33 % e I2 ±33 % (supuesto: la tolerancia de IEN del AP63301)": (tol(I1, 1/3), None),
}
print("UVLO del buck A (IC4):")
print(f"  A: sólo umbrales de EN           enciende {a_on[0]:.2f}–{a_on[1]:.2f} V   apaga {a_off[0]:.2f}–{a_off[1]:.2f} V")
b_on = extremos(lambda r3, r4, vh: von(r3, r4, vh, I1), [tol(R3, R1p), tol(R4, R1p), (1.15, 1.21)])
b_off = extremos(lambda r3, r4, vl: voff(r3, r4, vl, I12), [tol(R3, R1p), tol(R4, R1p), (1.02, 1.14)])
print(f"  B: + R ±1 %                       enciende {b_on[0]:.2f}–{b_on[1]:.2f} V   apaga {b_off[0]:.2f}–{b_off[1]:.2f} V")
# I1 en 1..2 uA (DS42002), I2 = 4 uA ±33 % (sin dato: supuesto)
c_on = extremos(lambda r3, r4, vh, i1: von(r3, r4, vh, i1),
                [tol(R3, R1p), tol(R4, R1p), (1.15, 1.21), (1.0e-6, 2.0e-6)])
c_off = extremos(lambda r3, r4, vl, i1, i2: voff(r3, r4, vl, i1 + i2),
                 [tol(R3, R1p), tol(R4, R1p), (1.02, 1.14), (1.0e-6, 2.0e-6), (4e-6 * 2/3, 4e-6 * 4/3)])
print(f"  C: + I1 1–2 µA, I2 2,7–5,3 µA      enciende {c_on[0]:.2f}–{c_on[1]:.2f} V   apaga {c_off[0]:.2f}–{c_off[1]:.2f} V")
print("     (C usa para I1 el rango que DS42002 da para el AP63301 y ±33 % para I2, que ninguna hoja acota)")

v5 = extremos(lambda rt, rb, vfb: vout(rt, rb, vfb), [tol(40.2e3, R1p), tol(7.5e3, R1p), (0.792, 0.808)])
v3 = extremos(lambda rt, rb, vfb: vout(rt, rb, vfb), [tol(62e3, R1p), tol(20e3, R1p), (0.792, 0.808)])
print(f"\n+5VA: {n_5va:.3f} V típ, {v5[0]:.3f}–{v5[1]:.3f} V (VFB ±1 % en −40..85 °C, R ±1 %)")
print(f"+3V3: {n_3v3:.3f} V típ, {v3[0]:.3f}–{v3[1]:.3f} V")
# márgenes pedidos
print(f"\n+5VA máx {v5[1]:.3f} V contra VDD de los PAM8302A 5,5 V (DS41333 'Supply Voltage Range 2.0 to 5.5'): margen {5.5 - v5[1]:.2f} V")
print(f"+5VA máx {v5[1]:.3f} V contra VIN del AP2125 6,0 V (DS38144, VIN máx de operación; 6,5 V absoluto): margen {6.0 - v5[1]:.2f} V")
print("  (el AP2125 cuelga de +5V_AUX, que está una caída de D19 por debajo de +5VA: el margen real es mayor)")
# sensibilidad: TCR de dos fabricantes distintos, ±100 ppm/°C c/u, +40 °C sobre 25 °C, en direcciones opuestas
tcr = 2 * 100e-6 * 40
v5t = (vout(40.2e3 * (1 + R1p), 7.5e3 * (1 - R1p) * (1 - tcr), 0.808))
print(f"\nSensibilidad: si además los dos resistores de +5VA derivan en direcciones opuestas (±100 ppm/°C, +40 °C): +5VA máx {v5t:.3f} V")
