#!/usr/bin/env python3
"""S13 — la entrada de la DS12 con una fuente de 24 V en vez de 20 V.

Simula el enchufe en caliente (conectar la ficha con la fuente ya encendida) y mide
lo que importa para no romper nada:
  - el pico en VIN_LOCAL, que es lo que ven los pines VIN de IC4 (AP63356) y, vía D20,
    de U11 (AP63301): 32 V de operación, 35 V continuos, 40 V por 400 ms (DS41949, DS42002);
  - la corriente y la energía que absorbe el TVS D9;
  - el I²t que atraviesa F1 (JFC1206-1200FS: se funde con 0,41 A²s).

Circuito (nombres del netlist DS12, exportado del esquemático el 2026-10-23):
  fuente ─ Rc ─ Lc ─ F1 ─ Q1 ─┬─ VIN_LOCAL: C316+C324+C326 (3× 10 µF X7R 1210) + C315
                              ├─ C273 ∥ C314 (2× 100 µF electrolítico, ESR Re c/u)
                              ├─ D9 (TVS, a GND)
                              └─ D20 ── VIN_B: C319+C325 (2× 10 µF) + C60 (22 µF electrolítico)
  Los dos bucks están apagados durante el pico (soft-start de 4 ms): peor caso, sin carga.
  Rc incluye cable, contactos, F1 (~50 mΩ) y Q1; por eso arranca en 0,10 Ω. Fuente rígida (peor caso).

TVS: lineal a tramos, como lo especifica la hoja (MDD, C364296 y familia):
  i = 0 bajo VBR; i = (v − VBR)/Rd arriba, con Rd = (VC − VBR)/IPP.
  Con VBR en el máximo de la hoja el recorte es el más alto: es el que se usa para el pico.

USO:  s13_entrada_24v.py              (autoprueba + barrido)
      s13_entrada_24v.py --autoprueba  (sólo la autoprueba)
"""
import math
import sys

VT = 0.02585

# SS34 (MDD), mismo ajuste que s4_hotplug.py
def _diodo(n=1.1, p1=(0.1, 0.34), p2=(1.0, 0.45)):
    (i1, v1), (i2, v2) = p1, p2
    nvt = n * VT
    rs = ((v2 - v1) - nvt * math.log(i2 / i1)) / (i2 - i1)
    is_ = i2 / math.exp((v2 - i2 * rs) / nvt)
    return is_, rs, nvt

IS, RS, NVT = _diodo()

def i_ss34(v):
    """Corriente del SS34 con v entre sus extremos (resistencia serie incluida), por bisección."""
    if v <= 0.05:
        return 0.0
    lo, hi = 0.0, v
    for _ in range(40):
        vj = 0.5 * (lo + hi)
        i = IS * (math.exp(vj / NVT) - 1)
        if vj + i * RS > v:
            hi = vj
        else:
            lo = vj
    return IS * (math.exp(lo / NVT) - 1)

# tabla de 0 a 3 V cada 1 mV: la bisección en cada paso hace el barrido 40 veces más lento
_PASO = 1e-3
_TABLA = [i_ss34(k * _PASO) for k in range(3001)]

def i_ss34_rapido(v):
    if v <= 0.05:
        return 0.0
    x = v / _PASO
    k = int(x)
    if k >= 3000:
        return _TABLA[3000] + (v - 3.0) / RS
    return _TABLA[k] + (x - k) * (_TABLA[k + 1] - _TABLA[k])

# Tabla de la hoja MDD (SMBJ20A_MDD_C364296.txt, líneas 146-157): VRWM, VBR mín, VBR máx, VC, IPP
TVS = {
    "SMBJ20A": (20.0, 22.2, 24.5, 32.4, 18.6),
    "SMBJ22A": (22.0, 24.4, 26.9, 35.5, 16.9),
    "SMBJ24A": (24.0, 26.7, 29.5, 38.9, 15.5),
    "SMBJ26A": (26.0, 28.9, 31.9, 42.1, 14.3),
}

def tvs_param(nombre, esquina="max"):
    vrwm, vbmin, vbmax, vc, ipp = TVS[nombre]
    vbr = vbmax if esquina == "max" else vbmin
    return vbr, (vc - vbmax) / ipp   # Rd de la hoja: (VC − VBR máx)/IPP

def simular(V=24.0, Lc=1.0e-6, Rc=0.15, Cc=15e-6, Ce=200e-6, Re=0.3,
            CbB=10e-6, C60=22e-6, Re60=1.5, tvs="SMBJ24A", esquina="max",
            t_fin=1.5e-3, dt=1e-8, sin_d20=False):
    """Euler semi-implícito. Devuelve picos, energía en el TVS e I²t de F1."""
    vbr, rd = tvs_param(tvs, esquina) if tvs else (1e9, 1.0)
    iL = v1 = vE = vB = v60 = 0.0
    pk = dict(v1=0.0, vB=0.0, iL=0.0, itvs=0.0, Etvs=0.0, I2t=0.0)
    t = 0.0
    while t < t_fin:
        iL += (V - iL * Rc - v1) / Lc * dt
        iE = (v1 - vE) / Re
        it = (v1 - vbr) / rd if v1 > vbr else 0.0
        i20 = 0.0 if sin_d20 else i_ss34_rapido(v1 - vB)
        i60 = (vB - v60) / Re60
        v1 += (iL - iE - it - i20) * dt / Cc
        vE += iE * dt / Ce
        vB += (i20 - i60) * dt / CbB
        v60 += i60 * dt / C60
        pk["I2t"] += iL * iL * dt
        pk["Etvs"] += it * v1 * dt
        if v1 > pk["v1"]: pk["v1"] = v1
        if vB > pk["vB"]: pk["vB"] = vB
        if iL > pk["iL"]: pk["iL"] = iL
        if it > pk["itvs"]: pk["itvs"] = it
        t += dt
    pk["v1_final"] = v1
    return pk

def autoprueba():
    """Casos con respuesta conocida. Si el modelo no los da, sus números no valen."""
    ok = True
    # 1) nada que medir: sin tensión, todo en cero
    r = simular(V=0.0, t_fin=200e-6)
    ok &= r["v1"] == 0 and r["I2t"] == 0 and r["Etvs"] == 0
    # 2) LC sin pérdidas, sin TVS ni electrolíticos ni D20: el pico es 2 V (escalón en LC serie)
    r = simular(V=10.0, Lc=1e-6, Rc=1e-6, Cc=10e-6, Ce=1e-12, Re=1e9, tvs=None,
                sin_d20=True, t_fin=40e-6, dt=1e-9)
    ok &= abs(r["v1"] - 20.0) < 0.1
    # 3) RC puro (L→0): I²t = V²·C/(2R), analítico
    C, R, V = 100e-6, 0.5, 24.0
    r = simular(V=V, Lc=1e-9, Rc=R, Cc=C, Ce=1e-12, Re=1e9, tvs=None, sin_d20=True,
                t_fin=12 * R * C, dt=1e-9)
    teor = V * V * C / (2 * R)
    ok &= abs(r["I2t"] - teor) / teor < 0.02
    # 4) sobreamortiguado (Rc ≫ √(L/C)): sin sobrepico apreciable
    r = simular(V=24.0, Lc=1e-6, Rc=3.0, tvs=None, t_fin=6e-3, dt=1e-8)
    ok &= r["v1"] < 24.0 * 1.01
    # 5) prueba negativa: el TVS TIENE que bajar el pico de un LC sin pérdidas
    sin = simular(V=24.0, Lc=1e-6, Rc=1e-6, Cc=10e-6, Ce=1e-12, Re=1e9, tvs=None,
                  sin_d20=True, t_fin=40e-6, dt=1e-9)
    con = simular(V=24.0, Lc=1e-6, Rc=1e-6, Cc=10e-6, Ce=1e-12, Re=1e9, tvs="SMBJ24A",
                  sin_d20=True, t_fin=40e-6, dt=1e-9)
    #    (sin ninguna pérdida la corriente pasa la IPP de la hoja: sólo se pide que recorte)
    ok &= sin["v1"] > 47 and con["v1"] < sin["v1"] - 3 and con["Etvs"] > 0
    print(f"autoprueba: {'OK' if ok else 'FALLA'}  (LC sin pérdidas: 2×; RC: I²t analítico "
          f"{teor:.3f} A²s; TVS recorta 48 V → {con['v1']:.1f} V)")
    return ok

def barrido():
    print("\n— Enchufe en caliente, fuente rígida, bucks apagados. TVS en VBR máximo (peor recorte) —")
    print("   ESR por electrolítico 0,3–0,6 Ω (C2836437: 600 mΩ @100 kHz máx); ceramicos efectivos 9–18 µF")
    print(" V[V]  TVS      Lc[µH] Rc[Ω] | pico VIN_LOCAL | pico VIN_B | I TVS  E TVS  | I²t F1 [A²s]")
    peor = {}
    for V, tvs in ((20.0, "SMBJ20A"), (24.0, "SMBJ24A"), (26.4, "SMBJ24A")):
        for Lc in (0.3e-6, 1.0e-6, 3.0e-6, 7.0e-6):
            for Rc in (0.10, 0.20, 0.35):
                pv, pb, pit, pe, pi2 = 0, 0, 0, 0, 0
                for Cc in (9e-6, 18e-6):
                    for Re in (0.3, 0.6):
                        r = simular(V=V, Lc=Lc, Rc=Rc, Cc=Cc, Re=Re / 2, tvs=tvs)
                        pv, pb = max(pv, r["v1"]), max(pb, r["vB"])
                        pit, pe, pi2 = max(pit, r["itvs"]), max(pe, r["Etvs"]), max(pi2, r["I2t"])
                print(f" {V:4.1f}  {tvs}  {Lc*1e6:4.1f}  {Rc:4.2f} | {pv:8.2f} V     | {pb:6.2f} V  |"
                      f" {pit:4.1f} A {pe*1e3:5.2f} mJ | {pi2:.3f}")
                k = (V, tvs)
                pp = peor.get(k, (0, 0, 0))
                peor[k] = (max(pp[0], pv), max(pp[1], pe), max(pp[2], pi2))
    print("\n— Resumen por caso (peor de todo el barrido) —")
    for (V, tvs), (pv, pe, pi2) in peor.items():
        print(f"  {V:4.1f} V con {tvs}: pico {pv:.1f} V · TVS {pe*1e3:.2f} mJ · I²t {pi2:.3f} A²s"
              f" ({100*pi2/0.41:.0f} % del I²t de fusión de F1)")
    print("\n— Régimen permanente: ¿el TVS conduce con la fuente quieta? —")
    for tvs in ("SMBJ20A", "SMBJ24A"):
        vrwm, vbmin, vbmax, vc, ipp = TVS[tvs]
        for V in (24.0, 26.4, 28.8):
            if V <= vrwm:
                est = f"≤ VRWM ({vrwm:.0f} V): fuga ≤ 1 µA"
            elif V < vbmin:
                est = f"entre VRWM y VBR mín ({vbmin} V): fuga < 1 mA, inofensiva"
            elif V < vbmax:
                est = f"DENTRO de la dispersión de VBR ({vbmin}–{vbmax} V): según la unidad, nada o amperes"
            else:
                est = f"POR ENCIMA de VBR máx ({vbmax} V): conduce en todas las unidades"
            print(f"  {tvs} con {V:4.1f} V: {est}")

if __name__ == "__main__":
    if not autoprueba():
        sys.exit(1)
    if "--autoprueba" not in sys.argv:
        barrido()
