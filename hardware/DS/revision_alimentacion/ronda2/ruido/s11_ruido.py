#!/usr/bin/env python3
"""S11 — ¿cuánto ruido de +5VA llega al ADC (AK5538) y a los DAC (PCM1690)?

Cadena (verificada en .ds12_audit/redes.txt):
  +5VA ─ FB69 ─ D19 ─ +5V_AUX ─ U10 AP2125 ─ +3.3VA ─┬─ AVDD1/AVDD2 (IC3.2, IC3.15)
                (C57 1µ, C59 100n,                    └─ R102..R105 20 Ω ─ VREFH1..4 (100 µF elec + 0,1 µF c/u)
                 +3×C45783 propuestos, +1 kΩ)
  +5VA ─ FB3 ─ +5VA_DAC (4×10 µF elec + 8×1 µF) ─ VCC1/VCC2 de U1..U4 (PCM1690)

Python puro, sin dependencias. Corre la autoprueba primero; si falla, se detiene.
  python3 s11_ruido.py                    # autoprueba + informe
  python3 s11_ruido.py --prueba-negativa  # corrompe un parámetro: la autoprueba TIENE que fallar

Los supuestos que no salen de una hoja de datos están marcados SUPUESTO y se barren.
"""
import cmath, math, os, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'hardware', 'DS', 'revision_alimentacion'))
from s4_hotplug import IS, RS, NVT          # SS34 de MDD ajustado a su Fig. 3 (0,34 V @0,1 A; 0,45 V @1 A)
from a1_ncaps import c45783                 # C45783 con la curva ΔC(V) de Samsung

TAU = 2 * math.pi
def par(*zs):
    zs = [z for z in zs if z is not None]
    return 1 / sum(1 / z for z in zs)
def cap(C, esr, esl):
    return lambda f: esr + 1j * TAU * f * esl + 1 / (1j * TAU * f * C)
def db(x):
    return 20 * math.log10(abs(x)) if abs(x) > 0 else float('-inf')

# ---------------------------------------------------------------- datos de hoja
# AP2125-3.3, DS38144 p.16 (VIN 4,3 V, IOUT 30 mA, COUT 1 µF). Digitalizado del render a 200 dpi.
PSRR_AP2125 = [(10, 72), (100, 71), (300, 70), (1e3, 70.6), (2e3, 69.5), (3e3, 66.7), (6e3, 69.6),
               (1e4, 64), (2e4, 58), (5e4, 50), (1e5, 41.6)]
def psrr_ldo(f, escenario='central'):
    """dB de rechazo. Más allá de 100 kHz la hoja no dice nada: tres escenarios."""
    if f <= PSRR_AP2125[-1][0]:
        for (f0, d0), (f1, d1) in zip(PSRR_AP2125, PSRR_AP2125[1:]):
            if f0 <= f <= f1:
                t = math.log10(f / f0) / math.log10(f1 / f0)
                return d0 + t * (d1 - d0)
        return PSRR_AP2125[0][1]
    if escenario == 'optimista':  return 41.6                       # sigue plano
    if escenario == 'pesimista':  return 20.0                       # piso típico de un LDO CMOS
    return max(20.0, 41.6 - 27 * math.log10(f / 1e5))               # sigue la pendiente final (−27 dB/déc)

AK_FS_PICO = 0.925        # V: 1,85 Vpp diferencial = 0 dBFS (AK5538 Note 10, p.9)
AK_VREF = 3.3
AK_PSRR_1K = 60.0         # dB, AVDD, sólo a 1 kHz (Note 11)
AK_PISO_DBFS = -111.0     # DR típico (p.9); la DS11 midió −113,4 (docs/protocolo_medicion.md)
AK_PISO_PSD = AK_PISO_DBFS - 10 * math.log10(20e3)   # dBFS/Hz, ≈ −154

# ---------------------------------------------------------------- componentes
L_FB, R_FB = 0.9e-6, 0.15           # CBW201209U601T: L de la curva p.24 (SUPUESTO ±30 %), DCR típ. (máx 0,22)
def rd(i):                          # resistencia dinámica del SS34 (modelo de s4_hotplug)
    return NVT / i + RS

def z_5va(f, lumped_esl=True):
    """Impedancia del nodo +5VA a alta frecuencia, todo en paralelo (el reparto real depende del PCB)."""
    zs = [cap(220e-6, 0.024, 2.5e-9)(f)] * 5                 # C71, C290-C293: 24 mΩ (hoja); ESL SUPUESTO
    zs += [cap(c45783(5.088), 0.005, 0.8e-9)(f)] * 2         # C69, C70
    zs += [cap(4.0e-6, 0.008, 0.6e-9)(f)] * 8                # C73-C80 (C1691 10 µF 0603: ~4 µF a 5 V, SUPUESTO)
    zs += [cap(0.85e-6, 0.02, 0.6e-9)(f)] * 16               # C89, C100… (C15849 1 µF 50 V)
    zs += [cap(0.1e-6, 0.03, 0.4e-9)(f), 1j * TAU * f * 6.8e-6]   # C72, y L1 a masa por los FET
    return par(*zs)

def h1(f, nuevos=True, i_d19=0.043, L=L_FB, R=R_FB, r_diodo=None):
    """+5VA → +5V_AUX: (FB69 + D19) en serie contra los capacitores de +5V_AUX."""
    zs = R + 1j * TAU * f * L + (rd(i_d19) if r_diodo is None else r_diodo)
    zsh = [cap(0.85e-6, 0.02, 0.6e-9)(f), cap(0.1e-6, 0.03, 0.4e-9)(f)]      # C57, C59
    if nuevos:
        zsh += [cap(c45783(4.7), 0.004, 0.8e-9)(f)] * 3 + [1000.0]          # 3×C45783 + 1 kΩ (A1/A2)
    zp = par(*zsh)
    return zp / (zs + zp), zs, zp

def z_33va(f):
    return par(cap(c45783(3.3), 0.004, 0.8e-9)(f), cap(3.0e-6, 0.01, 0.6e-9)(f),  # C67, C64 (4,7 µ ~3 µ a 3,3 V)
               cap(5.5e-6, 0.008, 0.6e-9)(f), cap(5.5e-6, 0.008, 0.6e-9)(f),     # C259, C265
               *[cap(0.09e-6, 0.03, 0.4e-9)(f)] * 3)                             # C260, C266, C63

def h3(f, esr_el=1.5, R=20.0):
    """+3.3VA → VREFH: 20 Ω contra (100 µF electrolítico ∥ 0,1 µF). ESR del electrolítico: SUPUESTO (sin dato)."""
    zp = par(cap(100e-6, esr_el, 3e-9)(f), cap(0.09e-6, 0.03, 0.4e-9)(f))
    return zp / (R + zp)

def h_dac(f, esr_el=2.0, L=L_FB, R=R_FB):
    """+5VA → +5VA_DAC: FB3 contra 4×10 µF elec (C2, C8, C14, C20; ESR SUPUESTO) ∥ 8×1 µF."""
    zp = par(*[cap(10e-6, esr_el, 3e-9)(f)] * 4, *[cap(0.85e-6, 0.02, 0.6e-9)(f)] * 8)
    return zp / (R + 1j * TAU * f * L + zp)

def pico(fun, f0=1e3, f1=3e6, n=3000):
    best = (0, -1e9)
    for k in range(n):
        f = f0 * (f1 / f0) ** (k / (n - 1))
        d = db(fun(f))
        if d > best[1]: best = (f, d)
    return best

def dIL(vin, vout, L, fsw):
    return (vin - vout) * vout / (vin * L * fsw)

# ---------------------------------------------------------------- autoprueba
def autoprueba(corromper=False):
    ok, msgs = True, []
    def chk(cond, txt):
        nonlocal ok
        ok &= bool(cond); msgs.append(('OK  ' if cond else 'FALLA ') + txt)
    # 1) RC ideal de VREFH: −3,01 dB en 1/(2π·20·100µ) = 79,58 Hz
    Rr = 200.0 if corromper else 20.0
    zc = lambda f: 1 / (1j * TAU * f * 100e-6)
    hrc = lambda f: zc(f) / (Rr + zc(f))
    chk(abs(db(hrc(79.577)) + 3.0103) < 0.05, f"RC 20 Ω/100 µF: {db(hrc(79.577)):.2f} dB en 79,6 Hz (esperado −3,01)")
    # 2) LC ideal: L 1 µH, C 1 µF, R 10 mΩ → pico = Q = 100 a 159,2 kHz
    hlc = lambda f: (1 / (1j * TAU * f * 1e-6)) / (0.01 + 1j * TAU * f * 1e-6 + 1 / (1j * TAU * f * 1e-6))
    fp, dp = pico(hlc, 50e3, 500e3, 20000)
    chk(abs(10 ** (dp / 20) - 100) < 1 and abs(fp - 159155) < 300, f"LC ideal: pico {10**(dp/20):.1f} a {fp/1e3:.1f} kHz (esperado 100 a 159,2)")
    # 3) diodo: el modelo reusado pasa por los dos puntos de la Fig. 3 de MDD
    vd = lambda i: NVT * math.log1p(i / IS) + i * RS
    chk(abs(vd(0.1) - 0.34) < 0.01 and abs(vd(1.0) - 0.45) < 0.01, f"SS34: {vd(0.1):.3f} V @0,1 A, {vd(1):.3f} V @1 A")
    # 4) ancla del rizado del buck: DS41949 Fig. 20 (12 V→5 V, 6,8 µH, 2×22 µF): IL ≈ 1 App, VOUT ≈ 15-20 mVpp
    d = dIL(12, 5, 6.8e-6, 450e3)
    zo = par(cap(c45783(5), 0.003, 0.5e-9)(450e3), cap(c45783(5), 0.003, 0.5e-9)(450e3))
    vpp = d / (8 * 450e3 * 2 * c45783(5)) + d * 0.0015
    chk(0.7 < d < 1.3 and 7e-3 < vpp < 40e-3, f"buck ancla: ΔIL {d:.2f} App (Fig.20 ≈1), Vpp {vpp*1e3:.1f} mV (Fig.20 ≈15-20, se acepta ×2)")
    # 5) nada que medir: sin bead ni diodo no hay atenuación inventada; sin rizado no hay ruido
    hh, _, _ = h1(250e3, nuevos=True, L=0.0, R=0.0, r_diodo=0.0)
    chk(abs(abs(hh) - 1) < 1e-9, f"sin FB69 ni D19, H1 = {abs(hh):.6f} (esperado 1: el filtro no inventa atenuación)")
    chk(inyeccion_vref([0.0], -60) == float('-inf'), "rizado cero → inyección −∞ dBFS/Hz")
    # 6) PSRR digitalizado: la tabla de la hoja dice 65 dB a 1 kHz con 0,5 Vpp; el gráfico ~70 (±6 dB aceptado)
    chk(abs(psrr_ldo(1e3) - 65) <= 6, f"PSRR AP2125 a 1 kHz: {psrr_ldo(1e3):.1f} dB (tabla: 65)")
    return ok, msgs

def inyeccion_vref(v_tonos, psd_q):
    """PSD en banda (dBFS/Hz) que mete el rizado de VREFH al multiplicar el ruido de cuantización
    fuera de banda del modulador: (v/Vref)²/2 · S_q(f_r), sumado sobre tonos."""
    s = sum((v / AK_VREF) ** 2 / 2 for v in v_tonos)
    return psd_q + 10 * math.log10(s) if s > 0 else float('-inf')

# ---------------------------------------------------------------- informe
def main():
    neg = '--prueba-negativa' in sys.argv
    ok, msgs = autoprueba(corromper=neg)
    print('AUTOPRUEBA' + (' (NEGATIVA: R de VREFH corrompida a 200 Ω)' if neg else ''))
    for m in msgs: print('  ' + m)
    if neg:
        print('→ prueba negativa ' + ('BIEN: la autoprueba detectó el error' if not ok else 'MAL: no lo detectó'))
        sys.exit(0 if not ok else 1)
    if not ok:
        print('→ autoprueba FALLÓ: no se dan números'); sys.exit(1)

    F = [28e3, 100e3, 200e3, 250e3, 300e3, 450e3, 3.072e6, 6.144e6]
    print('\n1) ATENUACIÓN POR ETAPA (dB, negativos = atenúa)')
    print('   f        |Z+5VA| | H1 hoy  H1 +3×22µ | LDO opt/cen/pes | H3 ESR 0,5/1,5/3 Ω | DAC FB3')
    for f in F:
        a, _, _ = h1(f, nuevos=False); b, _, _ = h1(f, nuevos=True)
        print(f'   {f/1e3:7.1f}k {abs(z_5va(f))*1e3:6.2f}mΩ | {db(a):6.1f} {db(b):8.1f} | '
              f'{-psrr_ldo(f,"optimista"):5.1f} {-psrr_ldo(f):5.1f} {-psrr_ldo(f,"pesimista"):5.1f} | '
              f'{db(h3(f,0.5)):6.1f} {db(h3(f,1.5)):6.1f} {db(h3(f,3)):6.1f} | {db(h_dac(f)):6.1f}')

    print('\n2) RESONANCIA FB69 + capacitores de +5V_AUX (pico de |H1|; Q ≈ √(L/C)/R)')
    for nuevos in (False, True):
        for i in (0.010, 0.043, 0.5, 1.0):
            fp, dp = pico(lambda f: h1(f, nuevos, i)[0], 1e3, 3e6)
            C = (0.95e-6 + (3 * c45783(4.7) if nuevos else 0))
            q = math.sqrt(L_FB / C) / (R_FB + rd(i))
            print(f'   {"+3×22µ" if nuevos else "hoy   "} I_D19={i*1e3:5.0f} mA  r_d={rd(i):.3f} Ω  f0={1/(TAU*math.sqrt(L_FB*C))/1e3:6.1f} kHz'
                  f'  Q={q:4.2f}  pico={dp:+5.2f} dB a {fp/1e3:6.1f} kHz')
        fp, dp = pico(lambda f: h1(f, nuevos, 0.5, R=0.10, L=1.2e-6)[0], 1e3, 3e6)
        print(f'   {"+3×22µ" if nuevos else "hoy   "} peor: DCR 0,10 Ω, L 1,2 µH, 0,5 A → pico {dp:+.2f} dB a {fp/1e3:.1f} kHz')
        print(f'   {"+3×22µ" if nuevos else "hoy   "} H1 en la banda de los PAM con 0,5 A por D19: ' +
              ', '.join(f'{f/1e3:.0f} kHz {db(h1(f, nuevos, 0.5)[0]):+.1f} dB' for f in (200e3, 250e3, 300e3, 450e3)))
    fp, dp = pico(lambda f: h_dac(f, 2.0), 1e3, 3e6)
    fp2, dp2 = pico(lambda f: h_dac(f, 8.0, R=0.10, L=1.2e-6), 1e3, 3e6)
    print(f'   FB3/+5VA_DAC: pico {dp:+.2f} dB a {fp/1e3:.1f} kHz (ESR elec 2 Ω); peor (ESR 8 Ω, DCR 0,1): {dp2:+.2f} dB a {fp2/1e3:.1f} kHz')

    print('\n3) FUENTES EN +5VA')
    d = dIL(19.6, 5.088, 6.8e-6, 450e3); d_peor = dIL(19.6, 5.088, 6.8e-6 * 0.8, 400e3)
    i1 = 8 / math.pi ** 2 * d / 2
    v_buck = i1 * abs(z_5va(450e3))
    print(f'   buck A: ΔIL = {d:.2f} App (peor, L−20 % y 400 kHz: {d_peor:.2f}); fundamental {i1:.2f} A → {v_buck*1e3:.2f} mV pico a 450 kHz ±6 %')
    pam = {}
    for nombre, ipam in (('central', 0.15), ('pesimista', 0.70)):
        # cada PAM: fundamental de su tren de pulsos de corriente (2/π)·I_L·sin(πD); 0,15 A ≈ 0,3 W/canal sobre 4 Ω,
        # 0,70 A = máximo con 2,5 W (I_L 1,1 A, D 0,5). Las 32 frecuencias son distintas: se suman en potencia.
        v1 = ipam * abs(z_5va(250e3))
        pam[nombre] = v1
        print(f'   PAM ({nombre}): {ipam:.2f} A a f_i → {v1*1e3:.2f} mV por tono; 32 tonos → {v1*math.sqrt(32)*1e3:.1f} mV rms total')

    print('\n4) LLEGADA AL ADC (por tono, en 250 kHz y 450 kHz)')
    psd_peor, psd_tip = -60.0, -95.0   # S_q en f_r: sin noise shaping (cota absoluta) y típico 1 bit de 5º orden
    for caso, nuevos, esc, esr, vpam in (('central  hoy     ', False, 'central', 1.5, pam['central']),
                                         ('central  +3×22µ  ', True, 'central', 1.5, pam['central']),
                                         ('pesimista hoy    ', False, 'pesimista', 3.0, 5 * pam['pesimista']),
                                         ('pesimista +3×22µ ', True, 'pesimista', 3.0, 5 * pam['pesimista'])):
        tonos = []
        for f, v in [(250e3, vpam)] * 32 + [(450e3, v_buck * (5 if 'pes' in caso else 1))]:
            g = abs(h1(f, nuevos)[0]) * 10 ** (-psrr_ldo(f, esc) / 20)
            tonos.append((f, v * g, v * g * abs(h3(f, esr))))
        avdd = math.sqrt(sum(t[1] ** 2 for t in tonos) / 2); vref = [t[2] for t in tonos]
        inj_p = inyeccion_vref(vref, psd_peor); inj_t = inyeccion_vref(vref, psd_tip)
        # plegado: cada tono de AVDD a f_i, con PSRR de AVDD a esa f = 0 dB (cota), pasa por la banda de rechazo
        # del decimador (≥85 dB, AK5538 p.10) y cae en f_i mod 48 kHz. Se compara con el piso por bin (FFT 2048).
        t_max = max(t[1] for t in tonos)
        plegado = db(t_max / AK_FS_PICO) - 85
        print(f'   {caso}: AVDD {avdd*1e6:8.2f} µV rms | VREFH máx {max(vref)*1e6:8.3f} µV | '
              f'inyección {inj_t:7.1f} dBFS/Hz (S_q típ) / {inj_p:7.1f} (sin shaping) vs piso {AK_PISO_PSD:.1f} | '
              f'tono plegado ≤ {plegado:6.1f} dBFS (PSRR_HF 0 dB) / {plegado-20:6.1f} (20 dB) vs bin {AK_PISO_PSD+10*math.log10(48e3/2048):.1f}')

    print('\n5) BATIDOS PAM-PAM DEMODULADOS POR D19 (única alinealidad de la cadena antes del LDO)')
    for nuevos in (False, True):
        for nombre, mult in (('central', 1), ('pesimista ×5', 5)):
            v = pam['central' if mult == 1 else 'pesimista'] * mult
            hh, zs, zp = h1(250e3, nuevos)
            v_d = v * rd(0.043) / abs(zs + zp)
            beat = v_d ** 2 / (2 * NVT)
            n_pares = 496 * 0.36            # pares con |f_i − f_j| < 20 kHz si f_i es uniforme en 200-300 kHz
            tot = beat * math.sqrt(n_pares)
            avdd = tot * 10 ** (-psrr_ldo(5e3) / 20)
            print(f'   {"+3×22µ" if nuevos else "hoy   "} {nombre:12s}: v_diodo {v_d*1e6:7.1f} µV → batido {beat*1e9:8.2f} nV c/u, '
                  f'{tot*1e9:8.1f} nV rms en +5V_AUX → AVDD {avdd*1e12:7.2f} pV → '
                  f'{db(avdd*10**(-AK_PSRR_1K/20)/AK_FS_PICO*math.sqrt(2)):7.1f} dBFS')

    print('\n6) RIZADO EN BANDA, DEPENDIENTE DEL PROGRAMA (corriente de audio de los PAM sobre el Zout del buck)')
    for v_audio, f in ((0.1, 2e3), (0.5, 600.0)):
        v33 = v_audio * 10 ** (-psrr_ldo(f) / 20)
        e_avdd = db(v33 * 10 ** (-AK_PSRR_1K / 20) / AK_FS_PICO)
        hv = abs(1 / (1 + 1j * f / 79.577))
        e_ref = db(v33 * hv / (2 * AK_VREF))
        print(f'   {v_audio*1e3:4.0f} mV a {f:5.0f} Hz → +3.3VA {v33*1e6:6.1f} µV → AVDD {e_avdd:6.1f} dBFS; '
              f'VREFH {v33*hv*1e6:5.2f} µV → bandas laterales {e_ref:6.1f} dBc (−1 dBFS de señal → {e_ref-1:6.1f} dBFS)')
        # DAC: si la referencia es VCOM (7,5 kΩ × 10 µF → 2,1 Hz) o VCC1 directo
        print(f'        DAC: modulación de ganancia si ref = VCC1 directo: {db(v_audio/5.0):6.1f} dB; '
              f'si ref = VCOM filtrado: {db(v_audio/5.0*2.12/f):6.1f} dB')

    print('\n7) ALIASING CERCA DE k·f_mod (f_mod no está en la hoja: se barren 64/128/256 fs)')
    for fm in (3.072e6, 6.144e6, 12.288e6):
        n = round(fm / 250e3); i_n = 2 * 1.1 / (n * math.pi)        # cota: pulso de 1,1 A, armónicos ~1/n
        v = i_n * abs(z_5va(fm))
        for nuevos in (False, True):
            g = abs(h1(fm, nuevos)[0]) * 10 ** (-20 / 20)
            print(f'   f_mod {fm/1e6:6.3f} MHz (armónico {n} de un PAM): {v*1e3:.2f} mV en +5VA → AVDD {v*g*1e9:7.1f} nV '
                  f'({"+3×22µ" if nuevos else "hoy"}) → con PSRR de AVDD = 0 dB: {db(v*g/AK_FS_PICO):6.1f} dBFS')
    print('\nNOTA: z_5va es concentrado; la inductancia de las pistas entre PAM, bulk y FB69 la decide el PCB.')

if __name__ == '__main__':
    main()
