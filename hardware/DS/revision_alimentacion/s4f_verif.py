#!/usr/bin/env python3
"""S4.f — verificación propia del hallazgo del refutador: con los 20 V PRESENTES (E1),
enchufar un cable con VBUS ya vivo (A→C o cargador) en J8 (USB del STM32).

Diferencias con el estado E3 de s4_hotplug.py — todas del netlist:
  - C55 (4,7 µF, /stm32_custom/VBUS) arranca DESCARGADO: R161 + R160 (62k + 100k) lo
    descargan a GND mientras no hay cable.
  - +5V_AUX arranca en ≈4,8 V, sostenido por +5VA → FB69 → D19.
  - VIN_B está en ≈19,5 V → D23 en reverso: los 42 µF de VIN_B NO participan.
  - +5V_AUX tiene sólo C57 (1 µF) + C59 (0,1 µF), y la carga del AP2125 (≈38–50 mA:
    AK5538 AVDD+VREFH, 015099878-E-02 p.9).
  - D19 (rama interna) se incluye: queda en reverso durante el pico, no ayuda.
  - U9 (USBLC6-2P6) en VBUS: zener a GND, VBR ≥ 6 V. Modelo grueso: 6,1 V + r_d.
"""
import math
from s4_hotplug import IS, RS, NVT, i_diodo

def caso(Lc=0.7e-6, Rc=0.15, Rhost=0.05, Vh=5.25, C55=4.0e-6, v1_0=0.0,
         Caux=0.9e-6, v5va=5.088, Rfb=0.22, I_ldo=0.040, rd_clamp=0.5, vbr=6.1,
         t_fin=60e-6, dt=1e-9, Rf3=0.06):
    iL = 0.0; v1 = v1_0
    # +5V_AUX inicial: equilibrio de la rama interna con la carga del LDO
    # (resuelvo Vf(D19) a I_ldo con el mismo modelo de diodo)
    vj = NVT * math.log1p(I_ldo / IS)
    v2 = v5va - I_ldo * (Rfb + RS) - vj
    v2_0 = v2; vjD8 = vjD19 = 0.0
    pk_v2 = v2; pk_v1 = v1; pk_i = 0.0; pk_clamp = 0.0
    t = 0.0
    while t < t_fin:
        iL += (Vh - iL * (Rhost + Rc) - v1) / Lc * dt
        iclamp = max(0.0, (v1 - vbr) / rd_clamp) if rd_clamp > 0 else 0.0
        iD8, vjD8 = i_diodo(v1 - v2, RS + Rf3, vjD8)
        iD19, vjD19 = i_diodo(v5va - v2, RS + Rfb, vjD19)
        v1 += (iL - iD8 - iclamp) * dt / C55
        v2 += (iD8 + iD19 - I_ldo) * dt / Caux
        pk_v2 = max(pk_v2, v2); pk_v1 = max(pk_v1, v1); pk_i = max(pk_i, iL)
        pk_clamp = max(pk_clamp, iclamp)
        t += dt
    return dict(v2_0=v2_0, v2=pk_v2, v1=pk_v1, i=pk_i, clamp=pk_clamp)

def autoprueba():
    ok = True
    # 1) sin cable (Vh = 0): +5V_AUX no se mueve de su punto de equilibrio
    r = caso(Vh=0.0, t_fin=20e-6)
    ok &= abs(r["v2"] - r["v2_0"]) < 1e-3
    # 2) C55 precargado a VBUS (como el J3, que la fuga de D2 precarga): sin escalón no hay pico
    r = caso(v1_0=5.25, t_fin=40e-6)
    ok &= r["v2"] < 5.3
    # 3) prueba negativa: con un capacitor GRANDE en +5V_AUX el pico TIENE que desaparecer
    r_chico = caso(); r_grande = caso(Caux=100e-6)
    ok &= r_grande["v2"] < r_chico["v2"] - 0.5
    print("autoprueba:", "OK" if ok else "FALLA")
    return ok

if __name__ == "__main__":
    assert autoprueba()
    print("J8 con 20 V presentes, cable A→C con VBUS vivo, C55 descargado")
    print(" Lc[µH] Rc[Ω] clamp U9 | +5V_AUX inicial → pico | VBUS pico | I pico")
    for Lc in (0.3e-6, 0.7e-6, 1.5e-6):
        for Rc in (0.08, 0.15, 0.30):
            for rd in (0.5, 2.0, 0.0):
                r = caso(Lc=Lc, Rc=Rc, rd_clamp=rd)
                cl = {0.5: "0,5 Ω", 2.0: "2 Ω  ", 0.0: "sin  "}[rd]
                print(f"  {Lc*1e6:4.1f}  {Rc:4.2f}  {cl}  | {r['v2_0']:.3f} → {r['v2']:.3f} V      |"
                      f" {r['v1']:6.2f} V  | {r['i']:5.2f} A")
    print("\nCon C55 en su valor nominal (4,7 µF) y VBUS 5,0 V:")
    for Lc in (0.3e-6, 0.7e-6, 1.5e-6):
        r = caso(Lc=Lc, Rc=0.15, C55=4.7e-6, Vh=5.0)
        print(f"  Lc = {Lc*1e6:.1f} µH → +5V_AUX pico {r['v2']:.3f} V")
    print("\nMitigación: capacitor en +5V_AUX (Lc 0,7 µH, Rc 0,15 Ω, clamp 0,5 Ω, VBUS 5,25 V):")
    for Cx in (0.9e-6, 4.7e-6, 10e-6, 22e-6):
        r = caso(Caux=Cx)
        print(f"  C(+5V_AUX) = {Cx*1e6:5.1f} µF → pico {r['v2']:.3f} V")
