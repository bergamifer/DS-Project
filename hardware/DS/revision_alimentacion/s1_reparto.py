#!/usr/bin/env python3
"""S1 — ¿D2 (USB) queda en reverso con el 20 V presente, o reparte la carga de +5V_AUX con D19?

Dos ramas en OR sobre +5V_AUX, con el MISMO diodo (SS34 MDD, C8678):
  interna:  +5VA ── FB69 (R_FB) ── D19 ──┐
  USB:      VBUS ── cable ── F2 (PTC) ── D2 ──┴── +5V_AUX ── carga I_L

Modelo del diodo: Shockley + resistencia serie, ajustado a la Fig.3 de la hoja de
MDD (Rev 2024A5, "Typical Forward Characteristic", curva SS32-SS35):
  VF(0,1 A) ≈ 0,34 V   ·   VF(1 A) ≈ 0,45 V   (lectura de la figura)
Se barre el factor de idealidad n (1,0 / 1,1 / 1,5): cambia la pendiente a baja
corriente, que es justo donde vive este reparto.
"""
import math

VT = 0.02585  # kT/q a 25 °C

def ajustar(n, p1=(0.1, 0.34), p2=(1.0, 0.45)):
    """Is y Rs que hacen pasar el modelo por los dos puntos de la figura."""
    (i1, v1), (i2, v2) = p1, p2
    nvt = n * VT
    rs = ((v2 - v1) - nvt * math.log(i2 / i1)) / (i2 - i1)
    is_ = i2 / math.exp((v2 - i2 * rs) / nvt)
    return is_, max(rs, 0.0)

def vd(i, is_, rs, n):
    return n * VT * math.log1p(i / is_) + i * rs if i > 0 else 0.0

def reparto(v5va, vbus, il, r_fb, r_usb, n, dT_mV=0.0):
    """Devuelve (I_usb, I_int, V_aux). dT_mV: corrimiento de Vf de D2 respecto de D19
    (p. ej. +20 mV si D2 está 10 °C más frío). Bisección sobre I_usb."""
    is_, rs = ajustar(n)
    def vaux_int(i):  return v5va - i * r_fb - vd(i, is_, rs, n)
    def vaux_usb(i):  return vbus - i * r_usb - vd(i, is_, rs, n) - dT_mV / 1000
    lo, hi = 0.0, il
    # si con TODA la carga en la rama interna el USB igual queda abajo -> I_usb = 0
    if vaux_usb(1e-12) <= vaux_int(il):
        return 0.0, il, vaux_int(il)
    if vaux_int(1e-12) <= vaux_usb(il):
        return il, 0.0, vaux_usb(il)
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        # f(mid) = V_usb(mid) - V_int(il - mid): decreciente en mid
        if vaux_usb(mid) > vaux_int(il - mid): lo = mid
        else: hi = mid
    i_usb = 0.5 * (lo + hi)
    return i_usb, il - i_usb, vaux_usb(i_usb)

# ---------- autoprueba (lo que tiene que dar si no hay nada que medir) ----------
def autoprueba():
    ok = True
    # 1) sin USB (VBUS = 0) la PC no puede entregar nada
    iu, ii, _ = reparto(5.088, 0.0, 0.05, 0.22, 0.2, 1.1)
    ok &= (iu == 0.0 and abs(ii - 0.05) < 1e-12)
    # 2) ramas idénticas -> 50/50
    iu, ii, _ = reparto(5.0, 5.0, 0.10, 0.2, 0.2, 1.1)
    ok &= abs(iu - 0.05) < 1e-6
    # 3) VBUS muy por encima -> todo del USB
    iu, ii, _ = reparto(5.0, 6.0, 0.05, 0.22, 0.2, 1.1)
    ok &= abs(iu - 0.05) < 1e-9
    # 4) el modelo reproduce la figura (tolerancia 0,1 mV: el ajuste usa log y vd usa
    #    log1p; con n = 1,5 difieren en 6 µV, cien veces menos que cualquier efecto acá)
    for n in (1.0, 1.1, 1.5):
        is_, rs = ajustar(n)
        ok &= abs(vd(0.1, is_, rs, n) - 0.34) < 1e-4 and abs(vd(1.0, is_, rs, n) - 0.45) < 1e-4
    # 5) prueba negativa del test: un modelo corrido 20 mV TIENE que fallar el punto 4
    is_, rs = ajustar(1.1)
    ok &= not (abs(vd(0.1, is_, rs, 1.1) + 0.020 - 0.34) < 1e-4)
    print("autoprueba:", "OK" if ok else "FALLA")
    return ok

if __name__ == "__main__":
    assert autoprueba()
    # +5VA en peor caso: VFB 0,8 V ±1 % (DS41949), R150 = 40,2 k y R153 = 7,5 k al ±1 %
    lo = 0.792 * (1 + 40.2 * 0.99 / (7.5 * 1.01))
    hi = 0.808 * (1 + 40.2 * 1.01 / (7.5 * 0.99))
    print(f"+5VA: nominal {0.8*(1+40.2/7.5):.3f} V · peor caso {lo:.3f}–{hi:.3f} V")
    for n in (1.1, 1.0, 1.5):
        is_, rs = ajustar(n)
        print(f"\n== n = {n}  (Is = {is_:.2e} A, Rs = {rs*1000:.0f} mΩ;"
              f" Vf a 1/10/50 mA = {vd(.001,is_,rs,n):.3f}/{vd(.01,is_,rs,n):.3f}/{vd(.05,is_,rs,n):.3f} V)")
        print("carga +5V_AUX    +5VA   " + "  ".join(f"VBUS={v:.2f}" for v in (4.75, 4.90, 5.00, 5.10, 5.25)))
        for il in (0.045, 0.5, 1.0):
            for v5 in (lo, 5.088, hi):
                fila = []
                for vb in (4.75, 4.90, 5.00, 5.10, 5.25):
                    iu, ii, va = reparto(v5, vb, il, r_fb=0.22, r_usb=0.06 + 0.15, n=n)
                    fila.append(f"{iu*1000:6.0f} mA ({100*iu/il:3.0f}%)")
                print(f"  {il*1000:5.0f} mA     {v5:.3f}  " + "  ".join(fila))
    # sensibilidad: diodos a distinta temperatura (±10 °C ≈ ±20 mV)
    print("\nsensibilidad a temperatura (n=1,1, +5VA nominal, 45 mA, VBUS 5,00 V):")
    for dT in (-20, 0, +20):
        iu, ii, va = reparto(5.088, 5.00, 0.045, 0.22, 0.21, 1.1, dT_mV=dT)
        print(f"  ΔVf(D2−D19) = {dT:+d} mV → USB entrega {iu*1000:.1f} mA ({100*iu/0.045:.0f} %)")
    # ¿cuándo queda D2 en reverso de verdad? VBUS < +5V_AUX
    for v5 in (lo, 5.088, hi):
        iu, ii, va = reparto(v5, 0.0, 0.045, 0.22, 0.21, 1.1)
        print(f"  con +5VA = {v5:.3f} V y 45 mA, +5V_AUX = {va:.3f} V → D2 en reverso sólo si VBUS < {va:.3f} V")
