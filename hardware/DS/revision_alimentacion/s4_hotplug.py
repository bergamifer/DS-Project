#!/usr/bin/env python3
"""S4 — enchufar el USB con la placa SIN 20 V (estado E3): corriente de arranque,
sobretensión en +5V_AUX (AP2125: 6,5 V abs / 6 V operación) y caída del VBUS del hub.

Circuito (nombres del netlist DS12, 2026-10-01):
  host ─ Rhost ─ cable (Lc, Rc) ─┬─ VBUS (C31 4,7 µF 0603) ── F2 (PTC) ── D2 ─┬─ +5V_AUX (C57 1 µF + C59 0,1 µF) ── D23 ─┬─ VIN_B
                                 └─ U6 USBLC6 (clamp VBUS–GND)               └─ U10 AP2125 → +3.3VA (≈35 µF efectivos)    │
                                                                                     C319+C325 (2×10 µF X7R 1210) + C320  ┤
                                                                                     C60 22 µF electrolítico (ESR Re)     ┘
  U11 (AP63301) apagado: peor caso para la sobretensión (sin carga que amortigüe).

Dos modelos de host:
  "rígido":  fuente ideal + Rhost  → peor caso de sobretensión
  "hub":     120 µF (CHPB mínimo de USB 2.0 §7.2.4.1) sin que su regulador llegue a
             reaccionar → mide la CAÍDA del VBUS del hub, que es lo que la norma limita (330 mV)
"""
import math

VT = 0.02585

def diodo(n=1.1, p1=(0.1, 0.34), p2=(1.0, 0.45)):   # SS34 MDD, Fig.3 (igual que s1_reparto.py)
    (i1, v1), (i2, v2) = p1, p2
    nvt = n * VT
    rs = ((v2 - v1) - nvt * math.log(i2 / i1)) / (i2 - i1)
    is_ = i2 / math.exp((v2 - i2 * rs) / nvt)
    return is_, rs, nvt

IS, RS, NVT = diodo()

def i_diodo(v, rser, vj0):
    """Corriente de un diodo con resistencia serie total rser, para tensión v entre sus
    extremos. Newton sobre la tensión de juntura vj. Devuelve (i, vj)."""
    if v <= 0:
        return 0.0, 0.0
    vj = min(v, vj0 + 0.05) if vj0 > 0 else min(v, 0.3)
    for _ in range(30):
        e = math.exp(vj / NVT)
        g = vj + rser * IS * (e - 1) - v
        dg = 1 + rser * IS * e / NVT
        nv = vj - g / dg
        if nv > v: nv = 0.5 * (vj + v)
        if nv < 0: nv = 0.5 * vj
        if abs(nv - vj) < 1e-9:
            vj = nv; break
        vj = nv
    return IS * (math.exp(vj / NVT) - 1), vj

def simular(Vh=5.25, Lc=0.7e-6, Rc=0.15, Rhost=0.05, C31=3.0e-6, Rf2=0.06,
            Caux=0.8e-6, Cb=18.5e-6, Ce=22e-6, Re=1.0, Cldo=35e-6,
            host="rigido", Chub=120e-6, rampa=0.0, t_fin=400e-6, dt=2e-9,
            dispositivo_ref=None, carga_u11=0.0):
    """Devuelve un dict con picos y la carga entregada. dispositivo_ref = (C, R) reemplaza
    toda la placa por el dispositivo de referencia de la norma (10 µF ∥ 44 Ω)."""
    iL = 0.0; v1 = v2 = v3 = vE = v4 = 0.0
    vh = Vh                      # tensión del capacitor del hub (modo "hub")
    vj2 = vj23 = 0.0
    pk = dict(iL=0.0, v1=0.0, v2=0.0, v3=0.0, droop=0.0, Q=0.0, Q100=0.0, ilim_ldo=0.0)
    t = 0.0
    while t < t_fin:
        # fuente
        if host == "rigido":
            vs = Vh * (min(1.0, t / rampa) if rampa > 0 else 1.0)
        else:
            vs = vh
        # cable
        diL = (vs - iL * (Rhost + Rc) - v1) / Lc
        iL += diL * dt
        if host == "hub":
            vh -= iL * dt / Chub
            pk["droop"] = max(pk["droop"], Vh - vh)
        if dispositivo_ref:
            Cref, Rref = dispositivo_ref
            v1 += (iL - v1 / Rref) * dt / Cref
        else:
            # placa
            iclamp = max(0.0, (v1 - 6.1) / 0.5)          # U6 USBLC6: VBR ≥ 6 V (modelo grueso)
            i2, vj2 = i_diodo(v1 - v2, RS + Rf2, vj2)     # D2 + F2
            i23, vj23 = i_diodo(v2 - v3, RS, vj23)       # D23
            # AP2125: límite con repliegue, 50 mA a VOUT = 0 (ISHORT) → 360 mA (IOUT máx típ)
            ilim = 0.05 + (0.36 - 0.05) * min(v4, 3.3) / 3.3
            ildo = ilim if (v4 < 3.3 and v2 - v4 > 0.07) else 0.0
            iE = (v3 - vE) / Re
            iu11 = carga_u11 if v3 > 3.8 else 0.0
            v1 += (iL - i2 - iclamp) * dt / C31
            v2 += (i2 - i23 - ildo) * dt / Caux
            v3 += (i23 - iE - iu11) * dt / Cb
            vE += iE * dt / Ce
            v4 += ildo * dt / Cldo
            pk["v2"] = max(pk["v2"], v2); pk["v3"] = max(pk["v3"], v3)
        pk["iL"] = max(pk["iL"], iL); pk["v1"] = max(pk["v1"], v1)
        pk["Q"] += iL * dt
        if t <= 100e-6: pk["Q100"] += iL * dt
        t += dt
    pk["v2_final"] = v2; pk["v3_final"] = v3; pk["v4_final"] = v4
    return pk

def autoprueba():
    """Lo que el modelo TIENE que dar en casos con respuesta conocida."""
    ok = True
    # 1) sin capacitor ni carga en la placa (1 nF ∥ 1 MΩ): no hay caída del hub
    r = simular(host="hub", dispositivo_ref=(1e-9, 1e6), dt=1e-10, t_fin=20e-6)
    ok &= r["droop"] < 0.01
    # 2) límites analíticos de repartir carga: 120 µF a 5 V contra 10 µF descargado
    #    (sin resistencia de carga, para que la cuenta sea exacta):
    #    muy amortiguado → la caída tiende a 5·10/130 = 0,385 V
    #    sin pérdidas     → el LC se pasa de largo y la caída llega a 2 × 0,385 = 0,769 V
    amort = simular(Vh=5.0, host="hub", dispositivo_ref=(10e-6, 1e9), Rc=3.0, Lc=0.1e-6, t_fin=600e-6)
    sinp = simular(Vh=5.0, host="hub", dispositivo_ref=(10e-6, 1e9), Rc=1e-4, Rhost=0.0, t_fin=60e-6)
    ok &= abs(amort["droop"] - 0.385) < 0.012 and abs(sinp["droop"] - 0.769) < 0.025
    # 3) prueba negativa: con 100 µF la caída TIENE que ser mucho mayor que con 10 µF
    r10 = simular(Vh=5.0, host="hub", dispositivo_ref=(10e-6, 44.0), Rc=0.15, t_fin=50e-6)
    r100 = simular(Vh=5.0, host="hub", dispositivo_ref=(100e-6, 44.0), Rc=0.15, t_fin=50e-6)
    ok &= r100["droop"] > 3 * r10["droop"]
    print(f"autoprueba: {'OK' if ok else 'FALLA'}  (reparto de carga: amortiguado "
          f"{amort['droop']*1000:.0f} mV [teoría 385], sin pérdidas {sinp['droop']*1000:.0f} mV [teoría 769];"
          f" 10 µF de norma {r10['droop']*1000:.0f} mV, 100 µF {r100['droop']*1000:.0f} mV)")
    return ok

if __name__ == "__main__":
    assert autoprueba()
    print("\n— Hub de USB 2.0 (120 µF, primeros 50 µs, antes de que su regulador reaccione) —")
    for Rc in (0.08, 0.15, 0.30):
        ref = simular(Vh=5.0, host="hub", dispositivo_ref=(10e-6, 44.0), Rc=Rc, t_fin=50e-6)
        ds = simular(Vh=5.0, host="hub", Rc=Rc, t_fin=50e-6)
        print(f"  cable Rc = {Rc:.2f} Ω: dispositivo de norma {ref['droop']*1000:4.0f} mV · "
              f"DS12 {ds['droop']*1000:4.0f} mV  (pico {ds['iL']:.1f} A, Q100µs = {ds['Q100']*1e6:.0f} µC)")
    print("\n— Host rígido (peor caso de sobretensión), VBUS 5,25 V, U11 apagado —")
    print("   Lc[µH] Rc[Ω] ReC60[Ω] | pico I [A] | VBUS máx | +5V_AUX máx | VIN_B máx")
    peor = 0
    for Lc in (0.3e-6, 0.7e-6, 1.5e-6):
        for Rc in (0.08, 0.15, 0.30):
            for Re in (0.5, 2.0):
                r = simular(Vh=5.25, Lc=Lc, Rc=Rc, Re=Re)
                peor = max(peor, r["v2"])
                print(f"   {Lc*1e6:5.1f}  {Rc:5.2f}  {Re:5.1f}   | {r['iL']:8.2f}  | {r['v1']:7.3f}  |"
                      f" {r['v2']:9.3f}   | {r['v3']:7.3f}")
    print(f"  → peor +5V_AUX = {peor:.3f} V  (AP2125: 6,0 V operación / 6,5 V absoluto, DS38144)")
    print("\n— USB-C a USB-C: la fuente enciende VBUS DESPUÉS de ver Rd, con rampa propia —")
    for tr in (0.5e-3, 1e-3, 3e-3):
        r = simular(Vh=5.25, rampa=tr, t_fin=tr + 0.5e-3, dt=5e-9)
        print(f"  rampa de {tr*1000:.1f} ms: pico {r['iL']*1000:.0f} mA · +5V_AUX máx {r['v2']:.3f} V")
