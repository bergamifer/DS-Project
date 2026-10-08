#!/usr/bin/env python3
"""Fila 8 del Bloque B: en modo sólo-USB, ¿un pico de TX del ESP32 baja VIN_B (red /DS_Power/IN) del umbral de caída del AP63301?

Camino (netlist): VBUS de J3 -> F2 (C523825, PTC 1,1 A) -> D2 (SS34) -> +5V_AUX -> D23 (SS34) -> IN = VIN del AP63301 (U11).
Capacidad en IN: C319 + C325 (10 uF 50 V X7R 1210, C138687) + C320 100 nF + C60 (22 uF 50 V electrolítico, C72505).

Mecanismo: el pico de TX (~350 mA más en +3V3) se convierte en ~0,33 A más en la entrada del buck. Esa corriente extra pasa por
el cable, F2, D2 y D23, y cada uno cae un poco más. Los capacitores de IN sólo tapan los primeros microsegundos (constante de
tiempo de decenas de us); un paquete de WiFi dura de cientos de us a ms, así que manda la caída en régimen.

Datos:
  SS34: curva SS32~SS35 de la Fig.3 de la hoja MDD (p.2), digitalizada con digit_ss34.py sobre img/ss34_fig3.png
        (típica, 25 °C). Puntos (A, V) abajo.
  F2: 60 mOhm a 25 °C, 210 mOhm máx después de disparar (DS12_ALIMENTACION.md §3.4, cierre del 2026-09-28).
  Cable: USB 2.0 §7.2.2: hasta 125 mV en VBUS y 125 mV en GND a la corriente nominal del cable -> se barre 0–0,5 Ohm ida y vuelta.
  AP63301 (DS42002 Rev 3-2): UVLO subida 3,5 típ / 3,7 máx, histéresis 440 mV típ (p.5); «disables if the input voltage falls
        below 3.06V» (p.15, §5). RDS(on) lado alto 75 mOhm típ (p.5). L2 DCR 33 mOhm máx (Sunlord p.6).
  Base: VIN_B = 3,84–3,98 V (hecho establecido, CONTEXTO.md).

python3 -I b8_solo_usb_tx.py
"""
import bisect

SS34 = [(0.12, 0.345), (0.15, 0.350), (0.20, 0.361), (0.30, 0.375), (0.40, 0.392), (0.50, 0.400),
        (0.70, 0.416), (0.90, 0.433), (1.00, 0.440), (1.30, 0.458), (2.00, 0.497), (3.00, 0.544)]

def vf(i):
    """interpolación lineal en log(I) de la curva típica digitalizada"""
    import math
    xs = [math.log10(a) for a, _ in SS34]; ys = [v for _, v in SS34]
    x = math.log10(i)
    if x <= xs[0]: return ys[0]
    if x >= xs[-1]: return ys[-1]
    k = bisect.bisect(xs, x)
    return ys[k - 1] + (ys[k] - ys[k - 1]) * (x - xs[k - 1]) / (xs[k] - xs[k - 1])

def iin(iout, vin, eta=0.9, vout=3.28):
    return iout * vout / (vin * eta)

# ---- autoprueba ----
assert abs(vf(1.0) - 0.440) < 1e-9 and abs(vf(0.3) - 0.375) < 1e-9          # devuelve los puntos digitalizados
assert vf(0.2) < vf(0.5) < vf(0.9) < vf(3.0)                                  # monótona
assert 0.40 <= vf(3.0) <= 0.55                                                # no supera el VF máx de la tabla (0,55 V a 3 A, p.1)
# la base del documento usa 0,36 V por diodo: en la curva eso corresponde a ~0,2 A
assert abs(vf(0.2) - 0.36) < 0.005
# ---- prueba negativa: si el pico fuera de 2 A extra con el peor cable, SÍ tendría que cruzar el umbral pesimista ----
neg = 3.84 - ((vf(0.57 + 2.0) - vf(0.57)) + (vf(0.52 + 2.0) - vf(0.52)) + 2.0 * (0.21 + 0.5))
assert neg < 3.26, neg
print(f"autoprueba OK; prueba negativa OK (un escalón de 2 A con el peor cable deja VIN_B en {neg:.2f} V < 3,26 V)\n")

UV_TIP, UV_PES = 3.5 - 0.44, 3.7 - 0.44
print(f"Umbral de caída del AP63301: {UV_TIP:.2f} V típico (= 3,06 V de DS42002 p.15); {UV_PES:.2f} V 'pesimista' (subida máx − histéresis típ;")
print("  la histéresis no tiene mínimo en la hoja, así que 3,26 V es una estimación, no un límite garantizado)\n")

for vbase in (3.84, 3.98):
    for iout0, rot in ((0.55, "carga típica 550 mA"), (0.65, "base algo más alta 650 mA")):
        i23 = iin(iout0, vbase); i2 = i23 + 0.05           # + AP2125/AK5538 por +5V_AUX
        di = iin(0.35, vbase)                                 # pico de TX
        dd = (vf(i2 + di) - vf(i2)) + (vf(i23 + di) - vf(i23))
        for rf2 in (0.06, 0.21):
            for rcab in (0.0, 0.5):
                caida = dd + di * (rf2 + rcab)
                v = vbase - caida
                print(f"base {vbase:.2f} V, {rot:26s} F2 {rf2*1e3:3.0f} mOhm, cable +{rcab:.1f} Ohm: "
                      f"ΔI {di:.2f} A, caída {caida*1e3:4.0f} mV -> VIN_B {v:.2f} V  "
                      f"(margen {v - UV_TIP:+.2f} / {v - UV_PES:+.2f} V)")
# regulación de +3V3 en baja caída: VOUT ~ VIN - IOUT*(RDS_HS + DCR)
for io in (0.55, 0.9):
    print(f"\nPara sostener +3V3 >= 3,20 V con {io:.2f} A hace falta VIN_B >= {3.20 + io * (0.075 + 0.033):.2f} V; "
          f"para 3,28 V, {3.28 + io * (0.075 + 0.033):.2f} V")

# Constante de tiempo de la entrada (para ver si los capacitores tapan el pico)
r_inc = ((vf(0.8) - vf(0.5)) / 0.3) * 2 + 0.06
for c_uF in (20, 42):
    print(f"tau = {r_inc:.2f} Ohm x {c_uF} uF = {r_inc * c_uF:.0f} us  (un paquete WiFi dura 0,1–5 ms: manda el régimen)")

# Chequeo de la BASE (no se re-deriva: se señala una inconsistencia)
for i_pk in (0.52, 0.9):
    vb = 4.75 - i_pk * 0.06 - vf(i_pk + 0.05) - vf(i_pk)
    print(f"\nBase con la Fig.3: 4,75 V − F2(60 mOhm) − D2 − D23 a {i_pk:.2f} A = {vb:.2f} V  (el documento usa 0,36 V por diodo)")
