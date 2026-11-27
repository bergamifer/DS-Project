"""Mitigación de S4.f: +5V_AUX con C57+C59 (≈0,9 µF efectivos) MÁS un electrolítico con ESR.
Mismo modelo que s4f_verif.py, con una rama RC extra en +5V_AUX."""
import math
from s4_hotplug import IS, RS, NVT, i_diodo
def caso(Lc, Rc, Cel, esr, Vh=5.25, C55=4.0e-6, Caux=0.9e-6, v5va=5.088, Rfb=0.22,
         I_ldo=0.040, rd=0.5, vbr=6.1, t_fin=80e-6, dt=1e-9, Rhost=0.05, Rf3=0.06):
    vj = NVT*math.log1p(I_ldo/IS); v2 = v5va - I_ldo*(Rfb+RS) - vj; vE = v2
    iL = 0.0; v1 = 0.0; a = b = 0.0; pk = v2; t = 0.0
    while t < t_fin:
        iL += (Vh - iL*(Rhost+Rc) - v1)/Lc*dt
        icl = max(0.0, (v1-vbr)/rd) if rd > 0 else 0.0
        i8, a = i_diodo(v1-v2, RS+Rf3, a); i19, b = i_diodo(v5va-v2, RS+Rfb, b)
        iE = (v2 - vE)/esr if Cel > 0 else 0.0
        v1 += (iL - i8 - icl)*dt/C55
        v2 += (i8 + i19 - I_ldo - iE)*dt/Caux
        if Cel > 0: vE += iE*dt/Cel
        pk = max(pk, v2); t += dt
    return pk
# autoprueba: sin electrolítico tiene que reproducir s4f_verif (0,7 µH / 0,15 Ω / clamp 0,5 Ω → 6,592 V)
ref = caso(0.7e-6, 0.15, 0.0, 1.0)
print(f"autoprueba: {'OK' if abs(ref-6.592) < 0.01 else 'FALLA'} (sin electrolítico: {ref:.3f} V, s4f_verif dio 6,592)")
print("pico de +5V_AUX con un electrolítico 22 µF agregado (peor fila: cable de 0,08 Ω)")
print(" Lc[µH] Rc[Ω] | ESR 0,5 Ω | ESR 1,5 Ω | ESR 3 Ω | ESR 8 Ω")
for Lc in (0.3e-6, 0.7e-6, 1.5e-6):
    for Rc in (0.08, 0.15):
        fila = [caso(Lc, Rc, 22e-6, e) for e in (0.5, 1.5, 3.0, 8.0)]
        print(f"  {Lc*1e6:4.1f}  {Rc:4.2f}  | " + " | ".join(f"{v:7.3f} V" for v in fila))

# Las dos perillas que sí actúan sobre la energía del cable: más cerámico en +5V_AUX, o menos C55
# (C55 no puede bajar de 1 µF: la ECN "Device Capacitance" de USB 2.0 exige CRPB ≥ 1 µF).
from s4f_verif import caso as caso_v
print("\npico de +5V_AUX — peor caso sobre Lc 0,3/0,7/1,5 µH × Rc 0,08/0,15/0,30 Ω, y el cable típico")
print(" C55 efectivo | C(+5V_AUX) efectivo |  peor   | cable 0,7 µH / 0,15 Ω")
for c55 in (4.0e-6, 0.9e-6):
    for cx in (0.9e-6, 10e-6, 22e-6):
        peor = max(caso_v(Lc=L, Rc=R, C55=c55, Caux=cx)["v2"]
                   for L in (0.3e-6, 0.7e-6, 1.5e-6) for R in (0.08, 0.15, 0.30))
        tip = caso_v(Lc=0.7e-6, Rc=0.15, C55=c55, Caux=cx)["v2"]
        print(f"   {c55*1e6:4.1f} µF     |      {cx*1e6:5.1f} µF       | {peor:.3f} V | {tip:.3f} V")
