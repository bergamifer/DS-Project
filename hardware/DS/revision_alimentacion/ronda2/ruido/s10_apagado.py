#!/usr/bin/env python3
"""S10 — ¿qué pasa en la DS12 cuando se retiran los 20 V?

Red (verificada en .ds12_audit/redes.txt):
  +20V ─ F1 ─ Q1 (P-FET, G a GND por R149 100k, D21 10 V) ─ VIN_LOCAL ─┬─ IC4 AP63356 (EN: R151 470k / R156 40,2k) ─ L1 ─ +5VA
                                                                       └─ D20 ─ VIN_B ─ U11 AP63301 ─ +3V3
  +5VA ─ FB69 ─ D19 ─ +5V_AUX ─┬─ U10 AP2125 → AK5538 (38 mA)
                               └─ D23 ─ VIN_B          ← segundo camino de +5VA hacia U11, sin pasar por IC4
  +5VA ─ L1 ─ diodo de cuerpo del FET alto de IC4 ─ VIN_LOCAL   ← sólo con IC4 apagado

Integración: Euler implícito + Newton sobre 5 nodos (los diodos entre capacitores chicos hacen el sistema rígido).
  python3 s10_apagado.py                    # autoprueba + escenarios
  python3 s10_apagado.py --prueba-negativa  # corrompe la autoprueba: TIENE que fallar
"""
import math, os, sys
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'hardware', 'DS', 'revision_alimentacion'))
from s4_hotplug import IS, RS, NVT       # SS34 MDD (Fig. 3: 0,34 V @0,1 A, 0,45 V @1 A)

# ------------------------------------------------------------ diodo explícito (Shockley + R serie, con W de Lambert)
def _w_exp(x):
    """w tal que w·e^w = e^x (o sea w + ln w = x), estable para x grande."""
    w = x - math.log(x) if x > 1 else math.exp(x) / (1 + math.exp(x))
    for _ in range(30):
        f = w + math.log(w) - x
        w_n = w - f / (1 + 1 / w)
        if w_n <= 0: w_n = w / 2
        if abs(w_n - w) < 1e-13 * max(1, w): return w_n
        w = w_n
    return w
def i_diodo(v, Rs, Is=IS, nvt=NVT):
    if v <= 0: return 0.0
    x = math.log(Is * Rs / nvt) + (v + Is * Rs) / nvt
    return nvt / Rs * _w_exp(x) - Is
# diodo de cuerpo (silicio): SUPUESTO 0,6 V @10 mA y 0,8 V @1 A (DS41949 no lo especifica)
NVT_B = (0.8 - 0.6 - 0.99 * 0.05) / math.log(100); IS_B = 0.01 / math.exp((0.6 - 0.01 * 0.05) / NVT_B)

# ------------------------------------------------------------ umbrales de hoja
def en_umbral(R3=470e3, R4=40.2e3, ven=1.18, i_en=1.5e-6):
    return ven * (1 + R3 / R4) - i_en * R3                        # DS41949 Eq.2/3 (forma usada en DS12_ALIMENTACION §3.2)
VON_IC4, VOFF_IC4 = en_umbral(), en_umbral(ven=1.08, i_en=5.5e-6)  # 14,3 / 11,1 V
U11_ON, U11_OFF = 3.5, 3.5 - 0.44                                  # DS42002 p.5: 3,5 V típ., histéresis 440 mV
PAM_UVLO = 2.1                                                     # DS41333 p.8
MAX809 = 2.93                                                      # IC1 = MAX809S (valor del netlist)

# ------------------------------------------------------------ álgebra
def resolver(A, b):
    n = len(b); M = [row[:] + [b[i]] for i, row in enumerate(A)]
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(M[r][c])); M[c], M[p] = M[p], M[c]
        for r in range(c + 1, n):
            f = M[r][c] / M[c][c]
            for k in range(c, n + 1): M[r][k] -= f * M[c][k]
    x = [0.0] * n
    for r in range(n - 1, -1, -1):
        x[r] = (M[r][n] - sum(M[r][k] * x[k] for k in range(r + 1, n))) / M[r][r]
    return x

def paso_be(V, C, corrientes, dt):
    """Euler implícito: C·(Vn − V)/dt + I_sale(Vn) = 0 por nodo. Newton con jacobiano numérico."""
    Vn = V[:]
    for _ in range(40):
        F = [C[i] * (Vn[i] - V[i]) / dt + I for i, I in enumerate(corrientes(Vn))]
        if max(abs(f) for f in F) < 1e-7: break
        J = []
        for i in range(len(V)):
            h = 1e-6
            Vp = Vn[:]; Vp[i] += h
            Fp = [C[k] * (Vp[k] - V[k]) / dt + I for k, I in enumerate(corrientes(Vp))]
            J.append([(Fp[k] - F[k]) / h for k in range(len(V))])
        J = [list(r) for r in zip(*J)]
        d = resolver(J, [-f for f in F])
        lim = max(1.0, max(abs(x) for x in d) / 0.2)               # amortiguado: ≤ 0,2 V por iteración
        Vn = [Vn[i] + d[i] / lim for i in range(len(V))]
    return Vn

# ------------------------------------------------------------ simulación
def simular(amps='off', nuevos=True, P_amps=11.3, I_dac=0.296, I_3v3=0.55, C5=1210e-6, CL=200e-6,
            C_bus=0.0, t_fin=40e-3, dt=5e-6, voff=VOFF_IC4):
    """amps: 'off' (SD bajo), 'reposo' (SD alto, sin señal), 'senal' (P_amps W de entrada a los 32 PAM).
    Nodos: 0 +5VA, 1 +5V_AUX, 2 VIN_B, 3 VIN_LOCAL, 4 +3V3."""
    st = {'ic4': True, 'u11': True, 't': 0.0, 'u11_en_t': -1.0}
    CA = 0.95e-6 + (3 * 10.9e-6 if nuevos else 0)
    C = [C5, CA, 40e-6, CL + 3 * 8e-6 + C_bus, 90e-6]
    s = lambda v, u: 0.5 * (1 + math.tanh((v - u) / 0.03))        # escalón suave (Newton lo agradece)

    def corrientes(V):
        v5, va, vb, vl, v3 = V
        i19 = i_diodo(v5 - va, 0.15 + RS)                            # FB69 (DCR 0,15) + D19
        i23 = i_diodo(va - vb, RS)                                   # D23
        i20 = i_diodo(vl - vb, RS)                                   # D20
        ib = 0.0 if st['ic4'] else i_diodo(v5 - vl, 0.075, IS_B, NVT_B)   # L1 + cuerpo del FET alto
        if amps == 'off':      ipam = 32e-6
        elif amps == 'reposo': ipam = 0.128 * s(v5, PAM_UVLO)
        else:                  ipam = (P_amps / max(v5, 0.5) + 0.128) * s(v5, PAM_UVLO)
        iload5 = ipam + I_dac * min(1.0, max(v5, 0) / 2.0) + v5 / 47.7e3 + v5 / 200e3
        if st['ic4']:
            i_ic4 = min(max((5.088 - v5) * 50, 0.0), 4.0); i_ic4_in = v5 * i_ic4 / (0.90 * max(vl, 1.0))
        else:
            i_ic4 = i_ic4_in = 0.0
        iaux = 0.038 * min(1.0, max(va, 0) / 2.5) + (va / 1000 if nuevos else 0)
        if st['u11']:
            vt = 3.28 * min(1.0, (st['t'] - st['u11_en_t']) / 4e-3) if st['u11_en_t'] >= 0 else 3.28
            i_reg = min(max((vt - v3) * 50, 0.0), 3.0)
            i_pass = max((vb - v3) / 0.13, 0.0)
            i_u = min(i_reg, i_pass)
            buck = vb > 3.28 + 0.13 * i_u + 0.05
            i_u_in = v3 * i_u / (0.85 * max(vb, 1.0)) if buck else i_u
        else:
            i_u = i_u_in = 0.0
        iload3 = I_3v3 * min(1.0, max(v3, 0) / 2.5)
        izen = max(vl - 10.0, 0) / 100e3
        return [i19 + ib + iload5 - i_ic4,
                -i19 + i23 + iaux,
                -i20 - i23 + i_u_in,
                i20 - ib + i_ic4_in + vl / 510.2e3 + izen,
                -i_u + iload3]

    V = [5.088, 4.70, 19.3, 19.7, 3.28]
    ev = {}; reenc = {'ic4': 0, 'u11': 0}
    max_ib = q_ib = max_i23 = 0.0; max_dif = -99.0; sd_ventana = 0.0; vb_post = 0.0; i_in_off = None
    t = 0.0
    while t < t_fin:
        st['t'] = t
        V = paso_be(V, C, corrientes, dt)
        t += dt
        v5, va, vb, vl, v3 = V
        # máquinas de estado (se evalúan con el estado nuevo)
        if st['ic4'] and vl < voff: st['ic4'] = False; ev.setdefault('IC4 se apaga (EN < VOFF)', t)
        elif not st['ic4'] and vl > VON_IC4: st['ic4'] = True; reenc['ic4'] += 1
        if st['u11'] and vb < U11_OFF:
            i_in_off = i_diodo(vl - vb, RS) + i_diodo(va - vb, RS)
            st['u11'] = False; ev.setdefault('U11 se apaga (UVLO 3,06 V)', t)
        elif not st['u11'] and vb > U11_ON: st['u11'] = True; st['u11_en_t'] = t; reenc['u11'] += 1
        for nom, cond in (('+5VA < 4,58 V (PG/UVP, 90 %)', v5 < 0.9 * 5.088), ('+5VA < 4,5 V (mín. PCM1690)', v5 < 4.5),
                          ('+5VA < 2,1 V (UVLO PAM)', v5 < 2.1), ('+3V3 < 3,2 V', v3 < 3.2),
                          ('+3V3 < 2,93 V (MAX809S resetea)', v3 < MAX809)):
            if cond: ev.setdefault(nom, t)
        ib = 0.0 if st['ic4'] else i_diodo(v5 - vl, 0.075, IS_B, NVT_B)
        max_ib = max(max_ib, ib); q_ib += ib * dt
        max_i23 = max(max_i23, i_diodo(va - vb, RS)); max_dif = max(max_dif, v5 - vl)
        if not st['u11'] and 'U11 se apaga (UVLO 3,06 V)' in ev: vb_post = max(vb_post, vb)
        if v5 < v3 - 0.3 and v3 > MAX809: sd_ventana += dt           # SD (3,3 V) por encima del VDD de los PAM
        if v3 < 1.0 and v5 < 1.0: break
    return ev, reenc, max_ib, q_ib, max_i23, max_dif, sd_ventana, vb_post, i_in_off

# ------------------------------------------------------------ autoprueba
def autoprueba(corromper=False):
    ok, msgs = True, []
    def chk(c, txt):
        nonlocal ok; ok &= bool(c); msgs.append(('OK  ' if c else 'FALLA ') + txt)
    # 1) descarga a corriente constante: V = V0 − I·t/C
    Cx = 100e-6; V = [5.0]
    for _ in range(100): V = paso_be(V, [Cx], lambda v: [0.5], 5e-6)
    esperado = 5.0 - 0.5 * 0.5e-3 / (Cx * (2 if corromper else 1))
    chk(abs(V[0] - esperado) < 1e-4, f"descarga a I constante: {V[0]:.4f} V (esperado {esperado:.4f})")
    # 2) descarga a potencia constante: V² = V0² − 2Pt/C
    V = [20.0]
    for _ in range(400): V = paso_be(V, [220e-6], lambda v: [2.0 / v[0]], 5e-6)
    esp = math.sqrt(400 - 2 * 2.0 * 2e-3 / 220e-6)
    chk(abs(V[0] - esp) / esp < 0.002, f"descarga a P constante: {V[0]:.3f} V (esperado {esp:.3f})")
    # 3) umbrales de EN: los de DS12_ALIMENTACION.md §3.2
    chk(abs(VON_IC4 - 14.28) < 0.05 and abs(VOFF_IC4 - 11.12) < 0.05, f"EN de IC4: VON {VON_IC4:.2f} V, VOFF {VOFF_IC4:.2f} V (doc: 14,3 / 11,1)")
    # 4) diodo explícito = modelo de s4_hotplug (0,34 V @0,1 A; 0,45 V @1 A)
    chk(abs(i_diodo(0.34, RS) - 0.1) < 0.01 and abs(i_diodo(0.45, RS) - 1.0) < 0.05, f"SS34 explícito: {i_diodo(0.34,RS):.3f} A @0,34 V, {i_diodo(0.45,RS):.3f} A @0,45 V")
    chk(abs(i_diodo(0.6, 0.05, IS_B, NVT_B) - 0.01) < 0.002 and abs(i_diodo(0.8, 0.05, IS_B, NVT_B) - 1.0) < 0.05, "diodo de cuerpo pasa por sus dos puntos")
    # 5) nada que medir: sin cargas ni convertidores, nada cae (2 nodos unidos por un diodo en equilibrio)
    V = [5.0, 4.0]
    for _ in range(2000): V = paso_be(V, [1e-3, 200e-6], lambda v: [i_diodo(v[0] - v[1] - 1.0, RS), -i_diodo(v[0] - v[1] - 1.0, RS)], 5e-6)
    chk(abs(V[0] - 5.0) < 1e-3 and abs(V[1] - 4.0) < 1e-3, f"sin cargas, 10 ms: {V[0]:.4f} / {V[1]:.4f} V (esperado 5 / 4: no inventa descargas)")
    return ok, msgs

def main():
    neg = '--prueba-negativa' in sys.argv
    ok, msgs = autoprueba(neg)
    print('AUTOPRUEBA' + (' (NEGATIVA: C del analítico a la mitad)' if neg else ''))
    for m in msgs: print('  ' + m)
    if neg:
        print('→ prueba negativa ' + ('BIEN: detectó el error' if not ok else 'MAL: no lo detectó')); sys.exit(0 if not ok else 1)
    if not ok: print('→ autoprueba FALLÓ'); sys.exit(1)

    casos = [('amps OFF, DAC típ.', dict(amps='off')),
             ('amps en reposo', dict(amps='reposo')),
             ('amps con señal 11,3 W', dict(amps='senal')),
             ('amps OFF, DAC sin corriente (cota)', dict(amps='off', I_dac=0.0)),
             ('amps OFF, DAC sin corriente, +5VA +20 %, VIN_LOCAL −20 %', dict(amps='off', I_dac=0.0, C5=1430e-6, CL=160e-6)),
             ('amps OFF, bus con 150 µF más por placa', dict(amps='off', C_bus=150e-6)),
             ('amps con señal, VOFF peor (10,4 V)', dict(amps='senal', voff=10.4))]
    for nombre, kw in casos:
        for nuevos in (True, False):
            ev, reenc, mib, qib, mi23, mdif, sdw, vbp, iin = simular(nuevos=nuevos, **kw)
            print(f'\n== {nombre} · {"con 3×22 µF + 1 kΩ" if nuevos else "hoy"}')
            for k, t in sorted(ev.items(), key=lambda x: x[1]): print(f'   {t*1e3:7.2f} ms  {k}')
            t5 = ev.get('+5VA < 4,5 V (mín. PCM1690)'); t3 = ev.get('+3V3 < 2,93 V (MAX809S resetea)')
            if t5 and t3: print(f'   → +3V3 sobrevive a +5VA (<4,5 V) por {(t3-t5)*1e3:.2f} ms')
            print(f'   diodo de cuerpo: máx {mib*1e3:.1f} mA, carga {qib*1e6:.1f} µC · D23 máx {mi23*1e3:.0f} mA · '
                  f'máx(+5VA − VIN_LOCAL) {mdif:+.2f} V · reencendidos IC4 {reenc["ic4"]}, U11 {reenc["u11"]} · '
                  f'SD sobre VDD de los PAM con MCU vivo: {sdw*1e3:.2f} ms')
            if iin is not None:
                # el modelo no tiene ESR: cota del rebote extra = I de entrada al apagar × ESR de C273∥C314 (0,6 Ω c/u @100 kHz)
                print(f'   rebote de VIN_B tras apagarse U11: pico {vbp:.2f} V (sin ESR) + ≤{iin*0.30:.2f} V por ESR '
                      f'= {vbp+iin*0.30:.2f} V contra el umbral de subida 3,5 V típ. ({"REARRANCA" if vbp+iin*0.30 > U11_ON else "no rearranca"})')

if __name__ == '__main__':
    main()
