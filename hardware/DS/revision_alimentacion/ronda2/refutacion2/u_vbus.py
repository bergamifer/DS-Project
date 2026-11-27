#!/usr/bin/env python3
"""U1/U2/U3 — ¿en cuánto queda el VBUS de J3 sin cable, con 20 V puestos?

Dos fuentes de corriente pueden cargar la red /esp32-mini/VBUS (C31, F2, J3, U6.5):
  (a) la fuga inversa de D2 (SS34, cátodo en +5V_AUX ≈ 4,8 V, ánodo en VBUS vía F2);
  (b) el pull-up interno de D+ del ESP32-C3 (a +3V3), que pasa por el diodo "alto" de U6
      (I/O → pin 5) hacia VBUS — SÓLO si el pin 5 de U6 está en VBUS (hoy). Con U2 (pin 5 a +3V3)
      ese camino desaparece.
Lo que la descarga: R_dren (U1), la carga de 15 kΩ de la prueba Back Voltage (USB-IF FS/LS
Test Procedure 1.3, F.1), la fuga del zener de U6 (10 nA típ., DS USBLC6 Tabla 2).

Fuentes:
  - fuga del SS34: a2_drenaje.py (Fig. 2 de MDD y máximos de p.1), sin modificar; se le agrega
    el factor (1 − e^(−VR/Vt)) porque a VR = 0 la corriente inversa tiene que ser 0.
  - pull-up del ESP32-C3: TRM v1.4, CONF0_REG: «PULLUP_VALUE 0: 2.2 K; 1: 1.1 K», DP_PULLUP = 1 al reset.
  - diodo alto del USBLC6: VF ≤ 1,1 V a 10 mA (Tabla 2). No hay IS publicado (el modelo PSpice no lo
    da): se barre la VF a 10 mA de 0,65 a 1,1 V con n = 1 (más VF baja = más corriente = peor para VBUS).
"""
import math, sys
sys.path.insert(0, "/Users/bergamifer/STM32CubeIDE/DS-Firmware/hardware/DS/revision_alimentacion")
from a2_drenaje import fuga_tipica_uA, fuga_max_uA

VT = 0.02585

def i_d2(vbus, v5, T, modo):
    vr = v5 - vbus
    if vr <= 0:
        return 0.0
    f = fuga_tipica_uA(T, vr) if modo == "tip" else (fuga_max_uA(T, vr) if modo == "max" else 0.0)
    return f * 1e-6 * (1 - math.exp(-vr / VT))

def i_dhigh(v, vf10):
    """Diodo alto del USBLC6 (ánodo I/O, cátodo pin 5), n = 1, calibrado por VF a 10 mA."""
    IS = 0.01 / math.exp(vf10 / VT)
    return IS * (math.exp(min(v, 1.5) / VT) - 1) if v > 0 else IS * (math.exp(max(v, -1.0) / VT) - 1)

def bisect(f, lo, hi, n=200):
    flo = f(lo)
    for _ in range(n):
        mid = 0.5 * (lo + hi); fm = f(mid)
        if (fm > 0) == (flo > 0): lo, flo = mid, fm
        else: hi = mid
    return 0.5 * (lo + hi)

def resolver(pin5_vbus=True, T=25, modo="tip", v5=4.8, v33=3.3, rpu=2200.0, vf10=0.9,
             r_dren=None, r_test=None, i_zener=10e-9, pullup=True):
    """Devuelve (VBUS, D+). r_test = 15 kΩ en VBUS y en D+ (prueba Back Voltage) o None."""
    def dplus(vbus):
        if not pullup:
            # sin pull-up, D+ sólo puede bajar; con carga de test queda en ~0
            return 0.0
        def kcl(vd):
            i = (v33 - vd) / rpu
            i -= vd / r_test if r_test else 0.0
            i -= i_dhigh(vd - vbus, vf10) if pin5_vbus else 0.0
            return i
        return bisect(kcl, -0.5, v33 + 0.5)
    def neta(vbus):
        i = i_d2(vbus, v5, T, modo) - i_zener
        if pin5_vbus and pullup:
            i += i_dhigh(dplus(vbus) - vbus, vf10)
        i -= vbus / r_dren if r_dren else 0.0
        i -= vbus / r_test if r_test else 0.0
        return i
    vb = 0.0 if neta(0.0) <= 0 else bisect(neta, 0.0, v5)
    return vb, dplus(vb)

def autoprueba():
    ok = True
    # 1) sin fuga y sin pull-up, VBUS tiene que quedar en 0
    vb, _ = resolver(pin5_vbus=True, modo="cero", pullup=False)
    ok &= vb < 1e-3
    # 2) sólo la fuga de D2, sin descarga: VBUS sube hasta cerca de +5V_AUX (a 0,3 V o menos)
    vb, _ = resolver(pin5_vbus=False, modo="tip", T=25)
    ok &= 4.5 < vb < 4.8
    # 3) sólo el pull-up (sin fuga): VBUS NO puede pasar de +3V3 (cota física)
    vb, _ = resolver(pin5_vbus=True, modo="cero", vf10=0.65)
    ok &= vb < 3.3
    # 4) prueba negativa: si el pull-up fuera a 5 V, ese camino SÍ pasaría de 4 V
    vb, _ = resolver(pin5_vbus=True, modo="cero", v33=5.0, vf10=0.65)
    ok &= vb > 4.0
    # 5) con 1 kΩ y sin pull-up, VBUS = R · fuga (ley de Ohm) dentro de 1 %
    vb, _ = resolver(pin5_vbus=False, modo="tip", T=85, r_dren=1000.0, i_zener=0)
    ok &= abs(vb - 1000 * i_d2(vb, 4.8, 85, "tip")) < 0.01 * vb
    print("autoprueba:", "OK" if ok else "FALLA")
    return ok

if __name__ == "__main__":
    assert autoprueba()
    print("\n== U3: VBUS de J3 sin cable y SIN descarga (sólo 10 nA del zener de U6) ==")
    for T in (25, 85):
        a, _ = resolver(pin5_vbus=False, T=T)
        b = max(resolver(pin5_vbus=True, modo="cero", vf10=v, rpu=r)[0] for v in (0.65, 0.9, 1.1) for r in (1100, 2200))
        c, _ = resolver(pin5_vbus=True, T=T)
        print(f"  {T:3d} °C | sólo fuga de D2: {a:.2f} V | sólo pull-up ESP32 por U6: ≤ {b:.2f} V | los dos: {c:.2f} V")

    print("\n== VBUS de J3 sin cable, con descarga R (fuga típica / máxima; peor de pull-up 1,1–2,2 k y VF 0,65–1,1 V) ==")
    print("   R      | pin 5 en +3V3 (U2)          | pin 5 en VBUS (hoy)")
    for R in (100e3, 10e3, 5.1e3, 1e3):
        fila = []
        for p5 in (False, True):
            for T in (25, 85):
                for modo in ("tip", "max"):
                    v = max(resolver(pin5_vbus=p5, T=T, modo=modo, r_dren=R, vf10=vf, rpu=rp)[0]
                            for vf in (0.65, 0.9, 1.1) for rp in (1100, 2200))
                    fila.append(v)
        print(f"  {R/1e3:6.1f}k | 25°C {fila[0]:.2f}/{fila[1]:.2f}  85°C {fila[2]:.2f}/{fila[3]:.2f} |"
              f" 25°C {fila[4]:.2f}/{fila[5]:.2f}  85°C {fila[6]:.2f}/{fila[7]:.2f}")

    print("\n== Prueba Back Voltage (15 kΩ en VBUS y en D+; límite 0,40 V), 25 °C, fuga típica ==")
    for p5, et in ((True, "hoy (pin 5 en VBUS)"), (False, "con U2 (pin 5 en +3V3)")):
        for R in (None, 1000.0):
            vals = [resolver(pin5_vbus=p5, r_test=15e3, r_dren=R, vf10=vf, rpu=rp) for vf in (0.65, 1.1) for rp in (1100, 2200)]
            vb = [v[0] for v in vals]; dp = [v[1] for v in vals]
            print(f"  {et:24s} R_dren={'—' if R is None else '1k'}: VBUS {min(vb):.2f}–{max(vb):.2f} V | D+ {min(dp):.2f}–{max(dp):.2f} V")
