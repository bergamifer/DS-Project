#!/usr/bin/env python3
"""A5 · Matriz de cobertura de la alimentación de la DS12 (ronda 2, crítico de completitud).

Lee el digest del netlist (.ds12_audit/{redes.txt, componentes.tsv}) y el DS.net (sólo para saber en qué
hoja del esquemático está cada componente) y contesta, chip por chip:
  1. ¿cada pin de alimentación que pide SU hoja de datos está en la red correcta? ¿alguno al aire?
  2. ¿el desacople que hay en la misma hoja del esquemático cumple valor y cantidad de la hoja de datos?
  3. ¿cada capacitor de una red de alimentación tiene tensión nominal con ≥25 % de margen sobre el máximo
     de su red (estacionario y transitorio)?

Uso:  python3 a5_matriz.py [--repo RAIZ]      → escribe matriz.csv y capacitores.csv al lado del script
      python3 a5_matriz.py --autoprueba       → corre sólo la autoprueba (positiva y negativa)

Regla del repo: el script se prueba contra un resultado conocido y contra algo que TIENE que fallar.
"""
import csv, json, os, re, sys, copy
from collections import defaultdict

AQUI = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(AQUI, '..', '..', '..'))

# ---------------------------------------------------------------------------------------------- datos
def cargar(repo=REPO):
    dig = os.path.join(repo, '.ds12_audit')
    comp = {}
    for l in open(os.path.join(dig, 'componentes.tsv'), encoding='utf-8'):
        p = l.rstrip('\n').split('\t')
        if p[0] == 'ref':
            continue
        comp[p[0]] = dict(value=p[1], fp=p[2], lcsc=p[3], desc=p[4] if len(p) > 4 else '')
    redes, pinred, func = {}, {}, {}
    for l in open(os.path.join(dig, 'redes.txt'), encoding='utf-8'):
        if l.startswith('#') or not l.strip():
            continue
        p = l.rstrip('\n').split('\t')
        nodos = []
        for n in p[2].split():
            m = re.match(r'([^.]+)\.([^(]+)(?:\((.*)\))?$', n)
            ref, pin, f = m.group(1), m.group(2), m.group(3) or ''
            nodos.append((ref, pin))
            pinred[(ref, pin)] = p[0]
            func[(ref, pin)] = f
        redes[p[0]] = nodos
    # hoja del esquemático de cada componente (sheetpath del DS.net)
    hoja = {}
    t = open(os.path.join(repo, 'hardware', 'DS', 'DS.net'), encoding='utf-8').read()
    for bloque in t.split('\n\t\t(comp')[1:]:
        m = re.search(r'\(ref "([^"]+)"\)', bloque)
        s = re.search(r'\(sheetpath\s*\(names "([^"]+)"\)', bloque)
        if m:
            hoja[m.group(1)] = s.group(1) if s else None
    return dict(comp=comp, redes=redes, pinred=pinred, func=func, hoja=hoja)

# Tensión nominal de cada código LCSC de capacitor. Fuente: página de producto de LCSC, leída el
# 2026-10-07 (línea «key attributes»). NO del campo description del esquemático.
NOMINAL = {
    'C1691':     (6.3, 'X5R', '0603', 'Samsung CL10A106MQ8NNNC', 'CAP CER 10uF 6.3V X5R 0603'),
    'C15849':    (50,  'X5R', '0603', 'Samsung CL10A105KB8NNNC', 'CAP CER 1uF 50V X5R 0603'),
    'C1525':     (16,  'X7R', '0402', 'Samsung CL05B104KO5NNNC', 'CAP CER 100nF 16V X7R 0402'),
    'C19666':    (16,  'X5R', '0603', 'Samsung CL10A475KO8NNNC', 'CAP CER 4.7uF 16V X5R 0603'),
    'C23630':    (16,  'X5R', '0603', 'Samsung CL10A225KO8NNNC', 'CAP CER 2.2uF 16V X5R 0603'),
    'C45783':    (25,  'X5R', '0805', 'Samsung CL21A226MAQNNNE', 'CAP CER 22uF 25V X5R 0805'),
    'C14663':    (50,  'X7R', '0603', 'YAGEO CC0603KRX7R9BB104', 'CAP CER 100nF 50V X7R 0603'),
    'C138687':   (50,  'X7R', '1210', 'Samsung CL32B106KBJNNNE', 'CAP CER 10uF 50V X7R 1210'),
    'C327198':   (50,  'NP0', '0402', 'YAGEO CC0402FRNPO9BN101', 'CAP CER 100pF 50V NP0 0402'),
    'C39122':    (50,  'C0G', '0402', 'FH 0402CG221J500NT',      'CAP CER 220pF 50V C0G 0402'),
    'C134745':   (25,  'Al',  'D4x5.7', 'Lelon VEJ100M1ETR-0406', 'CAP ALUM 10uF ±20% 25V SMD'),
    'C72505':    (50,  'Al',  'D6.3x5.4', 'ROQANG RVT1H220M0605', 'CAP ALUM 22uF ±20% 50V SMD'),
    'C2836437':  (35,  'Al',  'D6.3x7.7', 'KNSCHA RVE100UF35V67RV0072', 'CAP ALUM 100uF ±20% 35V SMD'),
    'C25503905': (16,  'Al-polímero', 'D6.3x7.7', 'ChuangHui SS221M160E7R7R4Z00ZZ', 'CAP ALUM POLY 220uF ±20% 16V SMD'),
    'C18221491': (10,  'Al',  'D5x5.4', 'RS RVT1A101M0505', 'jlcpcb.com/partdetail: 100uF 10V (LCSC dio 404)'),
}

# Máximo de cada red: (estacionario, transitorio, de dónde sale). Valores del CONTEXTO.md / encargo.
VMAX = {
    '+VIN_LOCAL':           (21.0, 32.4, 'ladrillo 20–21 V; transitorio = clamp del SMBJ20A'),
    '/DS_Power/IN':         (20.0, 32.0, '≈20 V; transitorio = VIN_LOCAL − D20'),
    '+5VA':                 (5.23, 5.75, '5,23 V (B); transitorio = OVP +10 % del AP63356 (supuesto)'),
    '+5VA_DAC':             (5.23, 5.75, 'igual que +5VA (FB3)'),
    '+5V_AUX':              (5.23, 8.0,  '≤5,23 V; golpe de ariete J8 hoy 7–8 V (≤5,8 V con 3×C45783)'),
    '/esp32-mini/VBUS':     (5.5,  8.5,  'VBUS USB 5,5 V; enchufe en caliente simulado hasta 8,5 V'),
    '/stm32_custom/VBUS':   (5.5,  8.5,  'VBUS USB 5,5 V; enchufe en caliente simulado hasta 8,5 V'),
    '+3V3':                 (3.36, 3.6,  '3,20–3,36 V (B); transitorio = OVP +10 % del AP63301'),
    '+3.3VA':               (3.36, 3.6,  'AP2125 3,3 V'),
    '/stm32_custom/VREF+':  (3.36, 3.6,  '+3V3 por FB1'),
    'Net-(IC3-VREFH1)':     (3.36, 3.6,  '+3.3VA por 20 Ω'),
    'Net-(IC3-VREFH2)':     (3.36, 3.6,  '+3.3VA por 20 Ω'),
    'Net-(IC3-VREFH3)':     (3.36, 3.6,  '+3.3VA por 20 Ω'),
    'Net-(IC3-VREFH4)':     (3.36, 3.6,  '+3.3VA por 20 Ω'),
    'Net-(IC3-VDD18)':      (1.98, 1.98, 'LDO interno del AK5538'),
    'Net-(U1-VCOM)':        (2.75, 2.75, '0,5 × VCC1 (PCM1690)'),
    'Net-(U2-VCOM)':        (2.75, 2.75, '0,5 × VCC1 (PCM1690)'),
    'Net-(U3-VCOM)':        (2.75, 2.75, '0,5 × VCC1 (PCM1690)'),
    'Net-(U4-VCOM)':        (2.75, 2.75, '0,5 × VCC1 (PCM1690)'),
    'Net-(C37-Pad1)':       (1.35, 1.35, 'VCAP (VCORE máx 1,35 V)'),
    'Net-(C39-Pad1)':       (1.35, 1.35, 'VCAP (VCORE máx 1,35 V)'),
    '/DS_Power/FB_A':       (0.81, 0.81, 'VFB'),
    '/DS_Power/FB_B':       (0.81, 0.81, 'VFB'),
    '/esp32-mini/EN{slash}CHIP_PU': (3.36, 3.6, '+3V3 por R8'),
    '/stm32_custom/NRST':   (3.36, 3.6, '+3V3'),
    '/stm32_custom/BOOT0':  (3.36, 3.6, '+3V3 por K3'),
}
# Los BST van entre BST y SW: la tensión sobre el capacitor es la del riel interno del driver (≤ 6 V abs).
VMAX_ENTRE = {frozenset(['Net-(IC4-BST)', 'Net-(IC4-SW)']): (6.0, 6.0, 'VBST − VSW ≤ 6 V (DS41949 abs máx)'),
              frozenset(['Net-(U11-BST)', 'Net-(U11-SW)']): (6.0, 6.0, 'VBST − VSW ≤ 6 V (DS42002 abs máx)')}
UMBRAL_MARGEN = 0.25

# --------------------------------------------------------------------------- especificación por chip
# (refs, pin, nombre en la hoja, red esperada, fuente). 'VCOM:' = red propia del chip con capacitor.
F_AK = 'AK5538VN 015099878-E-02 p.4–6 «Pin Functions»'
F_PCM = 'PCM1690 SBAS448B p.4–5 «Pin Functions»'
F_ST = 'DS12110 Rev10 Fig.5 «LQFP100 pinout» p.55'
F_ESP = 'ESP32-C3-MINI-1 v1.6 «Pin Definitions» p.10 y Fig.7 p.25'
F_PAM = 'PAM8302A DS41333 Rev6-2 «Pin Description» p.2'
F_EE = 'CAT24C256 Rev14 p.1 «Pin Configuration» (TSSOP)'
F_PRTR = 'UMW PRTR5V0U2X p.1 «Pinning information»: 4 = cátodos (VCC), 1 = ánodo del zener (GND)'
F_LC6 = 'USBLC6-2 (ST) p.1 Fig.1: 2 = GND, 5 = VBUS'
PAMS = ['U%d' % i for i in range(12, 44)]
PCMS = ['U1', 'U2', 'U3', 'U4']
PRTRS = ['D10'] + ['D%d' % i for i in range(12, 19)]

SPEC = [
    (['IC3'], '1', 'AVSS1', 'GND', F_AK), (['IC3'], '2', 'AVDD1 3,0–3,6 V', '+3.3VA', F_AK),
    (['IC3'], '5', 'VREFL2', 'GND', F_AK), (['IC3'], '6', 'VREFH2', 'Net-(IC3-VREFH2)', F_AK),
    (['IC3'], '11', 'VREFH3', 'Net-(IC3-VREFH3)', F_AK), (['IC3'], '12', 'VREFL3', 'GND', F_AK),
    (['IC3'], '15', 'AVDD2 3,0–3,6 V', '+3.3VA', F_AK), (['IC3'], '16', 'AVSS2', 'GND', F_AK),
    (['IC3'], '19', 'VREFH4', 'Net-(IC3-VREFH4)', F_AK), (['IC3'], '20', 'VREFL4', 'GND', F_AK),
    (['IC3'], '25', 'TVDD 3,0–3,6 V (LDOE=H)', '+3V3', F_AK), (['IC3'], '26', 'DVSS', 'GND', F_AK),
    (['IC3'], '27', 'VDD18 (salida del LDO, sólo capacitor)', 'Net-(IC3-VDD18)', F_AK),
    (['IC3'], '57', 'LDOE = H (LDO interno encendido)', '+3V3', F_AK),
    (['IC3'], '61', 'VREFL1', 'GND', F_AK), (['IC3'], '62', 'VREFH1', 'Net-(IC3-VREFH1)', F_AK),
    (['IC3'], '65', 'pad expuesto (a VSS para Ta>70 °C, p.7)', 'GND', F_AK),
    (PCMS, '12', 'VDD +3,3 V', '+3V3', F_PCM), (PCMS, '13', 'DGND', 'GND', F_PCM),
    (PCMS, '25', 'VCC1 +5 V', '+5VA_DAC', F_PCM), (PCMS, '26', 'VCOM', 'VCOM:', F_PCM),
    (PCMS, '27', 'AGND1', 'GND', F_PCM), (PCMS, '46', 'AGND2', 'GND', F_PCM),
    (PCMS, '47', 'VCC2 +5 V', '+5VA_DAC', F_PCM), (PCMS, '49', 'PowerPAD', 'GND', F_PCM),
    (['U7'], '6', 'VBAT (sin batería: a VDD, DS12110 §3.19)', '+3V3', F_ST),
    (['U7'], '10', 'VSS', 'GND', F_ST), (['U7'], '11', 'VDD', '+3V3', F_ST),
    (['U7'], '14', 'NRST', '/stm32_custom/NRST', F_ST),
    (['U7'], '19', 'VSSA', 'GND', F_ST), (['U7'], '20', 'VREF+', '/stm32_custom/VREF+', F_ST),
    (['U7'], '21', 'VDDA', '/stm32_custom/VREF+', F_ST), (['U7'], '26', 'VSS', 'GND', F_ST),
    (['U7'], '27', 'VDD', '+3V3', F_ST), (['U7'], '48', 'VCAP', 'Net-(C37-Pad1)', F_ST),
    (['U7'], '49', 'VSS', 'GND', F_ST), (['U7'], '50', 'VDD', '+3V3', F_ST),
    (['U7'], '73', 'VCAP', 'Net-(C39-Pad1)', F_ST), (['U7'], '74', 'VSS', 'GND', F_ST),
    (['U7'], '75', 'VDD', '+3V3', F_ST), (['U7'], '99', 'VSS', 'GND', F_ST), (['U7'], '100', 'VDD', '+3V3', F_ST),
    (['U5'], '1', 'GND', 'GND', F_ESP), (['U5'], '2', 'GND', 'GND', F_ESP), (['U5'], '3', '3V3', '+3V3', F_ESP),
    (['U5'], '8', 'EN (no dejar flotante)', '/esp32-mini/EN{slash}CHIP_PU', F_ESP),
    (['U5'], '11', 'GND', 'GND', F_ESP), (['U5'], '14', 'GND', 'GND', F_ESP),
] + [(['U5'], str(p), 'GND', 'GND', F_ESP) for p in range(36, 54)] + [
    (PAMS, '6', 'VDD 2,0–5,5 V', '+5VA', F_PAM), (PAMS, '7', 'GND', 'GND', F_PAM),
    (['U8'], '8', 'VCC 1,8–5,5 V', '+3V3', F_EE), (['U8'], '4', 'VSS', 'GND', F_EE),
    (PRTRS, '4', 'VCC (cátodos del puente)', '+3.3VA', F_PRTR), (PRTRS, '1', 'GND', 'GND', F_PRTR),
    (['U6'], '5', 'VBUS', '/esp32-mini/VBUS', F_LC6), (['U9'], '5', 'VBUS', '/stm32_custom/VBUS', F_LC6),
    (['U6', 'U9'], '2', 'GND', 'GND', F_LC6),
]

# Desacople pedido por cada hoja, por grupo de pines: (refs, red, [(valor_min_F, cantidad, texto)], fuente)
u, n, p = 1e-6, 1e-9, 1e-12
DESACOPLE = [
    (['IC3'], '+3.3VA', [(10*u*0.8, 2, '10 µF'), (0.1*u*0.8, 2, '0,1 µF')], 'AK5538 Fig.75 p.67: AVDD1 y AVDD2, 0,1 µF + 10 µF cada uno'),
    (['IC3'], '+3V3', [(10*u*0.8, 1, '10 µF'), (0.1*u*0.8, 1, '0,1 µF')], 'AK5538 Fig.75 p.67: TVDD 0,1 µF + 10 µF'),
    (['IC3'], 'Net-(IC3-VDD18)', [(4.7*u*0.8, 1, '4,7 µF'), (0.1*u*0.8, 1, '0,1 µF')], 'AK5538 Fig.75 p.67: VDD18 0,1 µF + 4,7 µF'),
] + [(['IC3'], 'Net-(IC3-VREFH%d)' % k, [(100*u*0.8, 1, '100 µF electrolítico'), (0.1*u*0.8, 1, '0,1 µF cerámico')],
      'AK5538 p.68 §2: 20 Ω desde AVDD + 100 µF ∥ 0,1 µF') for k in (1, 2, 3, 4)] + [
    ([r], '+3V3', [(10*u*0.8, 1, '10 µF electrolítico'), (1*u*0.8, 1, '1 µF cerámico')], 'PCM1690 Fig.41 p.37 + §9: VDD 1 µF + 10 µF') for r in PCMS] + [
    ([r], '+5VA_DAC', [(10*u*0.8, 1, '10 µF electrolítico'), (1*u*0.8, 2, '1 µF cerámico ×2 (VCC1, VCC2)')], 'PCM1690 Fig.41 p.37 + §9') for r in PCMS] + [
    (['U7'], '+3V3', [(4.7*u*0.8, 1, '4,7 µF'), (0.1*u*0.8, 5, '100 nF × N=5 pines VDD')], 'DS12110 Fig.15 p.103 (±20 % tolerado)'),
    (['U7'], '/stm32_custom/VREF+', [(1*u*0.8, 2, '1 µF ×2 (VDDA y VREF+)'), (0.1*u*0.8, 2, '100 nF ×2')], 'DS12110 Fig.15 p.103: VDDA 100 nF+1 µF; VREF+ 100 nF+1 µF'),
    (['U7'], 'Net-(C37-Pad1)', [(2.2*u*0.8, 1, '2,2 µF ±20 %, ESR<100 mΩ')], 'DS12110 Tabla 25 p.107'),
    (['U7'], 'Net-(C39-Pad1)', [(2.2*u*0.8, 1, '2,2 µF ±20 %, ESR<100 mΩ')], 'DS12110 Tabla 25 p.107'),
    (['U5'], '+3V3', [(10*u*0.8, 1, '10 µF'), (0.1*u*0.8, 1, '0,1 µF')], 'ESP32-C3-MINI-1 Fig.7 p.25 (C1, C2)'),
    (['U5'], '/esp32-mini/EN{slash}CHIP_PU', [(1*u*0.8, 1, '1 µF (RC con 10 kΩ)')], 'ESP32-C3-MINI-1 p.25: «R = 10 kΩ and C = 1 µF»'),
]
# PAM: 1 µF cerámico por chip + ≥10 µF cerca (DS41333 p.7 «Power Supply Decoupling (CS)»). Se evalúa por hoja Amp_Bank.

def valor_F(v):
    v = v.replace('µ', 'u').replace('μ', 'u').strip()
    m = re.match(r'([0-9.]+)\s*([pnuµm]?)', v)
    if not m:
        return None
    k = {'p': 1e-12, 'n': 1e-9, 'u': 1e-6, 'm': 1e-3, '': 1}[m.group(2)]
    return float(m.group(1)) * k

def caps_en(d, red, hoja=None):
    """Capacitores con un pin en `red` y el otro en GND (o en cualquier red, si red es propia de un chip)."""
    out = []
    for ref, pin in d['redes'].get(red, []):
        if not re.match(r'C\d+$', ref):
            continue
        otros = [d['pinred'][(ref, q)] for q in ('1', '2') if (ref, q) in d['pinred'] and d['pinred'][(ref, q)] != red]
        if otros != ['GND']:
            continue
        if hoja and d['hoja'].get(ref) != hoja:
            continue
        out.append(ref)
    return sorted(out, key=lambda r: int(r[1:]))

# --------------------------------------------------------------------------------- 1. pines
VERIFICADO_ANTES = {'IC4', 'U11', 'U10', 'Q1', 'IC1', 'D9', 'D3', 'D2', 'D8', 'D11', 'D19', 'D20', 'D23',
                    'J8', 'U9', 'FL1', 'J3', 'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7'}

def matriz_pines(d):
    filas = []
    for refs, pin, nombre, esperada, fuente in SPEC:
        for r in refs:
            red = d['pinred'].get((r, pin))
            if red is None:
                est, nota = 'PROBLEMA', 'el pin no existe en el netlist'
            elif red.startswith('unconnected') or len(d['redes'].get(red, [])) == 1:
                est, nota = 'PROBLEMA', 'pin al aire'
            elif esperada == 'VCOM:':
                cs = [c for c in caps_en(d, red)]
                est = 'OK ahora' if cs else 'PROBLEMA'
                nota = 'red propia con %s' % ','.join(cs) if cs else 'VCOM sin capacitor'
            elif red == esperada:
                est, nota = 'OK ahora', ''
            else:
                est, nota = 'PROBLEMA', 'está en %s' % red
            if r == 'U9' and pin == '5':
                est = 'verificado antes'
            filas.append(dict(chip=r, pin=pin, nombre=nombre, red=red or '', esperada=esperada, estado=est,
                              nota=nota, fuente=fuente))
    return filas

def pines_de_potencia_no_cubiertos(d):
    """Control de la propia SPEC: pines con nombre de alimentación en el netlist que la SPEC no mira."""
    cubiertos = {(r, p) for refs, p, *_ in SPEC for r in refs}
    patron = re.compile(r'^(A?VDD|VCC|TVDD|VDD18|VBAT|VCAP|VREF|VSS|AVSS|DVSS|GND|AGND|DGND|3V3|VBUS|PVDD|VDDA|VSSA|LDOE|EP)')
    chips = {r for (r, _) in d['func'] if re.match(r'(U|IC|D)\d', r)} - VERIFICADO_ANTES - {'D1', 'D4', 'D5', 'D6', 'D7', 'D21'}
    falt = []
    for (r, p), f in d['func'].items():
        if r in chips and patron.match(f) and (r, p) not in cubiertos:
            falt.append((r, p, f))
    return sorted(falt)

# ----------------------------------------------------------------------------- 2. desacople
def matriz_desacople(d):
    filas = []
    for refs, red, pide, fuente in DESACOPLE:
        r = refs[0]
        hoja = d['hoja'].get(r)
        propia = not red.startswith('+')
        cs = caps_en(d, red, None if propia else hoja)
        vals = [(c, valor_F(d['comp'][c]['value'])) for c in cs]
        faltas = []
        usados = set()
        for vmin, cant, txt in pide:   # de mayor a menor: cada capacitor cuenta una vez
            ok = [c for c, v in sorted(vals, key=lambda x: x[1]) if v and v >= vmin and c not in usados][:cant]
            usados.update(ok)
            if len(ok) < cant:
                faltas.append('%s: hay %d de %d' % (txt, len(ok), cant))
        est = 'OK ahora' if not faltas else 'PROBLEMA'
        filas.append(dict(chip=r, red=red, hoja=hoja, hay=' '.join('%s=%s' % (c, d['comp'][c]['value']) for c, _ in vals),
                          pide='; '.join(t for _, _, t in pide), estado=est, nota='; '.join(faltas), fuente=fuente))
    # PAM8302A, por hoja Amp_Bank (2 chips por hoja) + bulk común de /ALL_AMPS/ y /DS_Power/
    bancos = defaultdict(list)
    for r in PAMS:
        bancos[d['hoja'][r]].append(r)
    n1 = 0
    for hoja, chips in sorted(bancos.items()):
        cs = [c for c in caps_en(d, '+5VA', hoja) if (valor_F(d['comp'][c]['value']) or 0) >= 0.8e-6]
        n1 += len(cs)
        est = 'OK ahora' if len(cs) >= len(chips) else 'PROBLEMA'
        filas.append(dict(chip='+'.join(chips), red='+5VA', hoja=hoja,
                          hay=' '.join('%s=%s' % (c, d['comp'][c]['value']) for c in cs),
                          pide='1 µF cerámico por chip, «as close as possible» + ≥10 µF cerca', estado=est,
                          nota='%d de 1 µF para %d chips' % (len(cs), len(chips)), fuente='PAM8302A DS41333 p.7 «Power Supply Decoupling (CS)»'))
    return filas

# ------------------------------------------------------------------------- 3. tensión nominal
def tabla_capacitores(d, nominal=NOMINAL, vmax=VMAX):
    filas = []
    for ref, c in d['comp'].items():
        if not re.match(r'C\d+$', ref):
            continue
        redes = [d['pinred'].get((ref, q)) for q in ('1', '2')]
        if None in redes:
            continue
        clave = frozenset(redes)
        if clave in VMAX_ENTRE:
            vest, vtr, origen = VMAX_ENTRE[clave]
        else:
            vs = [vmax[r] for r in redes if r in vmax]
            if not vs or 'GND' not in redes and not any(r.startswith('/DS_Power/FB') for r in redes):
                continue
            vest, vtr, origen = max(vs)
        nom = nominal.get(c['lcsc'])
        if nom is None:
            filas.append(dict(ref=ref, value=c['value'], lcsc=c['lcsc'], redes='/'.join(redes), vnom='?',
                              vest=vest, vtr=vtr, m_est='', m_tr='', estado='ABIERTO', diel='', tam='', nota='código sin verificar'))
            continue
        vn, diel, tam, mpn, cita = nom
        m_est, m_tr = 1 - vest / vn, 1 - vtr / vn
        if m_tr >= UMBRAL_MARGEN:
            est = 'OK'
        elif m_est >= UMBRAL_MARGEN:
            est = 'MARGEN BAJO EN TRANSITORIO'
        else:
            est = 'MARGEN BAJO'
        nota = ''
        if c['value'] and re.search(r'(\d+)\s*[vV]', c['value']):
            vtxt = float(re.search(r'([0-9.]+)\s*[vV]', c['value']).group(1))
            if abs(vtxt - vn) > 0.01:
                nota = 'el value dice %g V, LCSC dice %g V' % (vtxt, vn)
        filas.append(dict(ref=ref, value=c['value'], lcsc=c['lcsc'], redes='/'.join(redes), vnom=vn, vest=vest, vtr=vtr,
                          m_est='%.0f%%' % (100 * m_est), m_tr='%.0f%%' % (100 * m_tr), estado=est, diel=diel, tam=tam,
                          nota=(nota + ('; ' if nota else '') + origen)))
    return sorted(filas, key=lambda f: int(f['ref'][1:]))

# ------------------------------------------------------------------------------- autoprueba
def autoprueba(d):
    fallas = []
    # Positivas: resultados conocidos, verificados a mano contra redes.txt
    mp = {(f['chip'], f['pin']): f for f in matriz_pines(d)}
    if mp[('IC3', '2')]['estado'] != 'OK ahora': fallas.append('AVDD1 del AK5538 debería estar OK en +3.3VA')
    if mp[('U7', '48')]['estado'] != 'OK ahora': fallas.append('VCAP 48 del STM32 debería estar OK')
    if mp[('U1', '26')]['estado'] != 'OK ahora': fallas.append('VCOM de U1 debería tener C5')
    tc = {f['ref']: f for f in tabla_capacitores(d)}
    if tc['C73']['estado'] != 'MARGEN BAJO': fallas.append('C73 (6,3 V sobre +5VA) debería salir MARGEN BAJO')
    if tc['C69']['estado'] != 'OK': fallas.append('C69 (C45783, 25 V sobre +5VA) debería salir OK')
    if 'LCSC dice 25' not in tc['C67']['nota']: fallas.append('C67: el value dice 16 V y LCSC 25 V; debería avisar')
    dk = {(f['chip'], f['red']): f for f in matriz_desacople(d)}
    if dk[('U5', '/esp32-mini/EN{slash}CHIP_PU')]['estado'] != 'PROBLEMA': fallas.append('EN del ESP32 tiene 0,1 µF: debería salir PROBLEMA')
    if dk[('U7', 'Net-(C37-Pad1)')]['estado'] != 'OK ahora': fallas.append('VCAP con 2,2 µF debería estar OK')
    # Negativas: se rompe una copia del netlist y el script TIENE que verlo
    m = copy.deepcopy(d)
    m['pinred'][('IC3', '25')] = 'GND'                                   # TVDD a masa
    m['pinred'][('U12', '6')] = 'unconnected-(U12-VDD-Pad6)'             # VDD de un PAM al aire
    m['redes']['unconnected-(U12-VDD-Pad6)'] = [('U12', '6')]
    mp2 = {(f['chip'], f['pin']): f for f in matriz_pines(m)}
    if mp2[('IC3', '25')]['estado'] != 'PROBLEMA': fallas.append('NEGATIVA: TVDD a GND no se detectó')
    if mp2[('U12', '6')]['estado'] != 'PROBLEMA': fallas.append('NEGATIVA: VDD de U12 al aire no se detectó')
    nom2 = dict(NOMINAL); nom2['C138687'] = (25, 'X7R', '1210', 'falso', 'falso')     # 25 V sobre VIN_LOCAL
    tc2 = {f['ref']: f for f in tabla_capacitores(d, nominal=nom2)}
    if tc2['C316']['estado'] == 'OK': fallas.append('NEGATIVA: un 25 V sobre +VIN_LOCAL no se detectó')
    m2 = copy.deepcopy(d)
    m2['redes']['Net-(C37-Pad1)'] = [x for x in m2['redes']['Net-(C37-Pad1)'] if x[0] != 'C37']
    dk2 = {(f['chip'], f['red']): f for f in matriz_desacople(m2)}
    if dk2[('U7', 'Net-(C37-Pad1)')]['estado'] != 'PROBLEMA': fallas.append('NEGATIVA: VCAP sin capacitor no se detectó')
    return fallas

def main():
    d = cargar()
    fallas = autoprueba(d)
    print('AUTOPRUEBA:', 'OK (8 positivas + 4 negativas)' if not fallas else 'FALLA')
    for f in fallas:
        print('  ✗', f)
    if fallas:
        sys.exit(1)
    if '--autoprueba' in sys.argv:
        return
    mp, dk, tc = matriz_pines(d), matriz_desacople(d), tabla_capacitores(d)
    with open(os.path.join(AQUI, 'matriz_pines.csv'), 'w', newline='', encoding='utf-8') as fh:
        w = csv.DictWriter(fh, fieldnames=list(mp[0])); w.writeheader(); w.writerows(mp)
    with open(os.path.join(AQUI, 'matriz_desacople.csv'), 'w', newline='', encoding='utf-8') as fh:
        w = csv.DictWriter(fh, fieldnames=list(dk[0])); w.writeheader(); w.writerows(dk)
    with open(os.path.join(AQUI, 'capacitores.csv'), 'w', newline='', encoding='utf-8') as fh:
        w = csv.DictWriter(fh, fieldnames=list(tc[0])); w.writeheader(); w.writerows(tc)
    cuenta = defaultdict(int)
    for f in mp: cuenta[f['estado']] += 1
    print('\nPINES:', dict(cuenta))
    for f in mp:
        if f['estado'] not in ('OK ahora', 'verificado antes'):
            print('  ', f)
    print('\nPines con nombre de alimentación que la SPEC no mira:', pines_de_potencia_no_cubiertos(d) or 'ninguno')
    print('\nDESACOPLE:')
    for f in dk:
        print('  %-9s %-28s %-10s %s | %s' % (f['chip'], f['red'], f['estado'], f['hay'], f['nota']))
    print('\nCAPACITORES con margen < %d %% o sin verificar:' % (100 * UMBRAL_MARGEN))
    for f in tc:
        if f['estado'] != 'OK' or 'LCSC dice' in f['nota']:
            print('  %-5s %-22s %-9s %-28s Vnom=%s est=%s/%s tr=%s/%s %s %s | %s' % (
                f['ref'], f['value'], f['lcsc'], f['redes'], f['vnom'], f['vest'], f['m_est'], f['vtr'], f['m_tr'],
                f['diel'], f['tam'], f['estado'] + ' · ' + f['nota']))
    ce = defaultdict(int)
    for f in tc: ce[f['estado']] += 1
    print('\nCapacitores revisados: %d · %s' % (len(tc), dict(ce)))
    print('\nCódigos de capacitor en redes de alimentación:', sorted({f['lcsc'] for f in tc}))

if __name__ == '__main__':
    main()
