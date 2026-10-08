#!/usr/bin/env python3
"""Cluster USB de la DS12 (A3/N4, N2, N3, PA5): tensión de la red VBUS de J3 y J8 SIN cable y con
el 20 V puesto, en función de la temperatura; qué ve PA5; el camino de D+ por el USBLC6; y los arreglos.

MODELO (todo del netlist DS12 del 1-oct y de las hojas citadas):
  +5V_AUX ≈ 4,80 V (S1: +5VA − Vf(D19); rango 4,63–4,90 V).
  D2 / D8 = SS34 (MDD C8678) con el cátodo en +5V_AUX y el ánodo (por F2/F3) en la red VBUS:
    su fuga inversa ENTRA a la red VBUS. Tensión inversa VR = V(+5V_AUX) − V(VBUS): baja a medida que la red sube.
    Fuga típica: Fig. 2 de MDD digitalizada (fig2_ss34.json, de fig2_digit.py), log-lineal en % de VR
      y en temperatura entre 25/75/100 °C; × (1 − e^(−VR/(n·Vt))) con n = 1,1 (el ajuste directo de
      s4_hotplug.py), porque a VR = 0 la corriente tiene que ser cero.
    Fuga máxima especificada (MDD p.1): 0,5 mA a 25 °C y 5 mA a 125 °C, a 40 V; log-lineal en T y
      llevada a VR con la FORMA de la curva típica a esa temperatura (como a2_drenaje.py).
  Cargas de la red VBUS de J3: U6 (USBLC6) IRM 10 nA típ / 150 nA máx a 25 °C y 5,25 V (ST Doc ID
    11265 Rev 5, Tabla 2), × 10^((T−25)/50) (= ×100 a 125 °C, el tope del eje de su Fig. 4: cota, no lectura);
    C31: aislación ≥ 500 MΩ·µF / 4,7 µF = 106 MΩ (cota típica de MLCC; no cambia nada).
  Red VBUS de J8: lo mismo con U9 y C55, más R161 + R160 = 162 kΩ. PA5 = V × 100/162.
  Camino de D+ (N3): pull-up interno del ESP32-C3 a 3,3 V, activo por defecto (TRM v1.4 p.175 y
    USB_SERIAL_JTAG_CONF0 p.775: DP_PULLUP = 1 al reset; PULLUP_VALUE 0 → 2,2 kΩ, 1 → 1,1 kΩ; la norma
    pide 1,5 kΩ) → U6 pata 6 (I/O1) → diodo «Dhigh» I/O→VBUS (ST PSpice: RS 0,52 Ω; IS no dado → 1e-14 A,
    el valor por defecto de SPICE; se barre 1e-15…1e-12) → pata 5 = red VBUS de J3.

AUTOPRUEBA (respuestas conocidas): reproduce puntos de la Fig. 2; sin fuga, la red queda en 0;
  con carga infinita la red sube hasta casi +5V_AUX; el divisor da 4,4 V → 2,716 V.
PRUEBA NEGATIVA: si se invierte el diodo (fuga hacia afuera), la red TIENE que quedar en 0, y una
  resistencia de descarga más grande TIENE que dar una tensión mayor.
"""
import json, math, os

AQUI = os.path.dirname(os.path.abspath(__file__))
FIG2 = {int(k): v for k, v in json.load(open(os.path.join(AQUI, "fig2_ss34.json"))).items()}
VR_NOM = 40.0
V_AUX = 4.80
N_ID = 1.1
K_Q = 8.617e-5

def _interp_pct(pts, pct):
    if pct <= pts[0][0]: return pts[0][1]
    if pct >= pts[-1][0]: return pts[-1][1]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        if x0 <= pct <= x1:
            t = 0.0 if x1 == x0 else (pct - x0) / (x1 - x0)
            return math.exp(math.log(y0) + t * (math.log(y1) - math.log(y0)))

def _fig(T, pct):
    """µA de la Fig. 2 (curva sólida) a T y % de VR, log-lineal en T."""
    if T <= 75: t0, t1 = 25, 75
    else:       t0, t1 = 75, 100
    a, b = _interp_pct(FIG2[t0], pct), _interp_pct(FIG2[t1], pct)
    return a * math.exp((math.log(b) - math.log(a)) * (T - t0) / (t1 - t0))

def _arranque(VR, T):
    if VR <= 0: return 0.0
    return 1.0 - math.exp(-VR / (N_ID * K_Q * (T + 273.15)))

def fuga_typ_uA(VR, T):
    return _fig(T, 100.0 * max(VR, 0.0) / VR_NOM) * _arranque(VR, T)

def fuga_max_uA(VR, T):
    i40 = 500.0 * 10 ** ((T - 25) / 100.0)
    pct = 100.0 * max(VR, 0.0) / VR_NOM
    forma = _fig(T, pct) / _fig(T, 100.0)
    return max(i40 * forma, _fig(T, pct)) * _arranque(VR, T)

def fuga_usblc6_uA(T, maxima=False):
    return (0.150 if maxima else 0.010) * 10 ** ((T - 25) / 50.0)

def red_vbus(T, R_carga=float("inf"), maxima=False, v_aux=V_AUX, i_extra_uA=0.0, invertir=False):
    """Tensión de equilibrio de la red VBUS (V): fuga del diodo = carga resistiva + USBLC6 + aislación de C.
    i_extra_uA: corriente que entra a la red por otro camino (p. ej. el de D+)."""
    R_C = 106e6
    def neta(v):
        vr = v_aux - v
        i_in = (fuga_max_uA if maxima else fuga_typ_uA)(vr, T) * 1e-6
        if invertir: i_in = -i_in
        i_out = (v / R_carga if R_carga != float("inf") else 0.0) + v / R_C + fuga_usblc6_uA(T, maxima) * 1e-6 * (v / 5.25)
        return i_in + i_extra_uA * 1e-6 - i_out
    lo, hi = 0.0, v_aux
    if neta(lo) <= 0: return 0.0
    for _ in range(80):
        mid = 0.5 * (lo + hi)
        if neta(mid) > 0: lo = mid
        else: hi = mid
    return 0.5 * (lo + hi)

# ---- camino de D+ (N3) ----
def i_dhigh(v, IS=1e-14, n=1.0, RS=0.52, VT=0.02585):
    """Diodo I/O→VBUS del USBLC6 (corriente para una caída v), resolviendo la RS por Newton."""
    if v <= 0: return 0.0
    i = 0.0
    for _ in range(60):
        vj = v - i * RS
        f = IS * (math.exp(min(vj / (n * VT), 200)) - 1) - i
        df = -IS * math.exp(min(vj / (n * VT), 200)) * RS / (n * VT) - 1
        i -= f / df
        i = max(i, 0.0)
    return i

def camino_dmas(T, R_vbus, R_pu=1.5e3, V_pu=3.3, R_dp=15e3, IS=1e-14, maxima=False, con_fuga=True):
    """Con el pull-up de D+ activo: (V_VBUS, V_D+, corriente por el diodo en µA).
    R_vbus: todo lo que carga la red VBUS (descarga propia ‖ fijación de 15 kΩ ‖ descarga de la Mac).
    R_dp: lo que carga D+ del otro lado (15 kΩ de host o de la fijación; inf = sin cable)."""
    def vdp_para(vbus):
        lo, hi = 0.0, V_pu
        for _ in range(80):
            m = 0.5 * (lo + hi)
            i = (V_pu - m) / R_pu - (m / R_dp if R_dp != float("inf") else 0) - i_dhigh(m - vbus, IS=IS)
            if i > 0: lo = m
            else: hi = m
        return 0.5 * (lo + hi)
    lo, hi = 0.0, V_AUX
    for _ in range(80):
        vb = 0.5 * (lo + hi)
        vdp = vdp_para(vb)
        i_d = i_dhigh(vdp - vb, IS=IS)
        i_l = (fuga_max_uA if maxima else fuga_typ_uA)(V_AUX - vb, T) * 1e-6 if con_fuga else 0.0
        if i_d + i_l - vb / R_vbus > 0: lo = vb
        else: hi = vb
    vb = 0.5 * (lo + hi); vdp = vdp_para(vb)
    return vb, vdp, i_dhigh(vdp - vb, IS=IS) * 1e6

def par(a, b):
    if a == float("inf"): return b
    if b == float("inf"): return a
    return a * b / (a + b)

def autoprueba():
    ok = True
    ok &= abs(fuga_typ_uA(0.40 * VR_NOM, 75) / 97.9 - 1) < 0.02          # Fig.2 digitalizada, 40 %, 75 °C
    ok &= abs(fuga_typ_uA(0.20 * VR_NOM, 25) / 1.47 - 1) < 0.02
    ok &= abs(fuga_max_uA(VR_NOM, 25) - 500) < 1e-6 and fuga_max_uA(VR_NOM, 125) >= 5000 - 1
    ok &= red_vbus(25, R_carga=1e3, i_extra_uA=0, invertir=True) == 0.0     # sin fuente → 0 V
    v_abierta = red_vbus(25)
    ok &= V_AUX - 0.05 < v_abierta < V_AUX                                  # carga ~nula → casi +5V_AUX
    ok &= abs(4.4 * 100 / 162 - 2.716) < 0.001
    # negativas
    ok &= red_vbus(60, R_carga=10e3) > red_vbus(60, R_carga=1e3)          # más R → más tensión
    ok &= red_vbus(25, R_carga=162e3, invertir=True) == 0.0
    vb, vdp, i = camino_dmas(25, R_vbus=1e3, con_fuga=False)
    ok &= 0.5 < vb < 1.5 and i > 100                                        # el camino de D+ existe
    vb0, _, _ = camino_dmas(25, R_vbus=1e3, V_pu=0.0, con_fuga=False)
    ok &= vb0 < 1e-3                                                         # sin pull-up no hay camino
    print("autoprueba usb_vbus:", "OK" if ok else "FALLA")
    return ok

TEMPS = (25, 40, 60, 75, 85)

if __name__ == "__main__":
    assert autoprueba()
    INF = float("inf")
    print("\n1) Fuga de D2/D8 con +5V_AUX = 4,80 V y la red VBUS cerca de 0 (VR = 4,8 V = 12 % de 40 V), µA")
    print("   T [°C]:      " + "  ".join(f"{T:>7d}" for T in TEMPS))
    print("   típica:      " + "  ".join(f"{fuga_typ_uA(4.8, T):7.1f}" for T in TEMPS))
    print("   máx. espec.: " + "  ".join(f"{fuga_max_uA(4.8, T):7.0f}" for T in TEMPS))

    print("\n2) Red VBUS SIN cable, 20 V puestos (V). Típica / máxima")
    filas = [("J3 hoy (sólo U6 + C31)", INF), ("J3 + multímetro 10 MΩ", 10e6),
             ("J8 hoy (R161+R160 = 162 kΩ)", 162e3), ("J3 o J8 + 1 kΩ (propuesta)", 1e3),
             ("  idem con 5,1 kΩ", 5.1e3), ("  idem con 10 kΩ", 10e3), ("  idem con 100 kΩ (C25741)", 100e3)]
    for nombre, R in filas:
        t = "  ".join(f"{red_vbus(T, R):4.2f}/{red_vbus(T, R, maxima=True):4.2f}" for T in TEMPS)
        print(f"   {nombre:30s} {t}")
    print("   (columnas: " + ", ".join(f"{T} °C" for T in TEMPS) + ")")

    print("\n3) PA5 = V(J8) × 100/162 sin cable (V). Umbrales DS12110 Tabla 60 con VDD = 3,3 V:")
    print("   VIL máx 0,3·VDD = 0,99 (CMOS) / 0,4·VDD−0,1 = 1,22 (diseño); VIH mín 0,47·VDD+0,25 = 1,80 (diseño) / 0,7·VDD = 2,31 (CMOS)")
    for etq, R in (("hoy (162 kΩ)", 162e3), ("con 1 kΩ en paralelo", par(162e3, 1e3))):
        t = "  ".join(f"{red_vbus(T, R)*100/162:4.2f}/{red_vbus(T, R, maxima=True)*100/162:4.2f}" for T in TEMPS)
        print(f"   {etq:22s} {t}")
    for etq, mx in (("típica", False), ("máxima", True)):
        cruces = {}
        for u in (0.99, 1.22, 1.80, 2.31):
            cruces[u] = next((T for T in range(0, 126) if red_vbus(T, 162e3, maxima=mx) * 100 / 162 >= u), None)
        print(f"   hoy, fuga {etq}: PA5 pasa 0,99 V a {cruces[0.99]} °C, 1,22 V a {cruces[1.22]} °C, "
              f"1,80 V a {cruces[1.80]} °C, 2,31 V a {cruces[2.31]} °C")

    print("\n4) Prueba «Back Voltage» de USB-IF (15 kΩ en VBUS y en D+; límite 400 mV), J3, 25 °C")
    for etq, Rp in (("pull-up 1,1 kΩ", 1.1e3), ("1,5 kΩ", 1.5e3), ("2,2 kΩ", 2.2e3)):
        vb, vdp, i = camino_dmas(25, R_vbus=15e3, R_pu=Rp)
        print(f"   hoy, D+ activo ({etq:14s}): VBUS {vb:.2f} V, D+ {vdp:.2f} V, {i:.0f} µA por el diodo de U6")
    for mx in (False, True):
        print(f"   sólo fuga de D2 ({'máx' if mx else 'típ'}), sin pull-up: VBUS {red_vbus(25, 15e3, maxima=mx):.3f} V;"
              f" con 1 kΩ propio: {red_vbus(25, par(15e3, 1e3), maxima=mx):.3f} V")

    print("\n5) J3 contra la descarga de la Mac en AttachWait.SRC (R_src desconocida; D+ de la Mac con 15 kΩ), 25 °C")
    print("   R_src  | hoy (pin 5 en VBUS, D+ activo 1,5 k / 2,2 k) | +1 kΩ, pin 5 en VBUS | +1 kΩ, pin 5 en +3V3 (sólo fuga típ/máx, 85 °C)")
    for Rs in (100, 500, 1e3, 2e3, 5e3, 10e3, 100e3):
        a = camino_dmas(25, R_vbus=Rs, R_pu=1.5e3)[0]; b = camino_dmas(25, R_vbus=Rs, R_pu=2.2e3)[0]
        c = camino_dmas(25, R_vbus=par(Rs, 1e3), R_pu=1.5e3)[0]
        d, e = red_vbus(85, par(Rs, 1e3)), red_vbus(85, par(Rs, 1e3), maxima=True)
        print(f"   {Rs/1e3:6.1f}k | {a:5.2f} / {b:5.2f} V                          |  {c:5.2f} V             |  {d:5.3f} / {e:5.3f} V")
    print("   sensibilidad al IS del diodo de U6 (R_src 1 kΩ, pull-up 1,5 k): " +
          ", ".join(f"IS {IS:.0e} → {camino_dmas(25, R_vbus=1e3, IS=IS)[0]:.2f} V" for IS in (1e-15, 1e-14, 1e-12)))

    print("\n6) N3 en J8: el STM32 con el pull-up de D+ prendido y el host que se va (sin cable o host apagado)")
    for etq, R in (("hoy, 162 kΩ", 162e3), ("con 1 kΩ en paralelo", par(162e3, 1e3))):
        vb, vdp, i = camino_dmas(25, R_vbus=R, R_dp=float("inf"))
        print(f"   {etq:22s}: VBUS {vb:.2f} V → PA5 {vb*100/162:.2f} V  ({i:.0f} µA por U9)")

    print("\n7) Arreglo de 1 kΩ con VBUS presente")
    for v in (4.4, 5.0, 5.25, 5.5):
        print(f"   VBUS {v:4.2f} V: {v/1e3*1e3:.2f} mA, {v*v/1e3*1e3:.1f} mW en el 1 kΩ 0402; "
              f"PA5 = {v*100/162:.3f} V (nominal), extremos ±1 %: {v*99/(99+62.62):.3f}–{v*101/(101+61.38):.3f} V")
