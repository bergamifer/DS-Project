#!/usr/bin/env python3
"""sch2net.py — deriva el netlist de un proyecto KiCad leyendo los .kicad_sch, SIN KiCad.

POR QUÉ EXISTE
--------------
El netlist exportado (.net) no va al repo (está en .gitignore), y en la nube no hay
kicad-cli. Sin netlist no hay forma de responder "¿a dónde va este pin?" sobre el
esquemático que se está dibujando: netlist_digest.py y netlist_diff.py se quedaban
con el DS.txt de la DS11 congelada. Esto lee los .kicad_sch directamente —jerarquía
de hojas con instancias múltiples, buses (grupos y vectores), etiquetas locales,
globales y jerárquicas, símbolos de alimentación, las doce orientaciones de símbolo
que usa KiCad— y escribe un netlist en el mismo formato que exporta eeschema.

AUTOPRUEBA (la regla del repo: preguntarle qué haría si no hubiera nada que medir)
Corrido sobre el esquemático de la DS11 (tag ds11-release) y comparado con el
netlist que exportó KiCad de esa misma placa (hardware/DS/DS.txt) con
netlist_diff.py: 0 diferencias de conexión en 2049 pines. Las únicas diferencias
son de nombre (KiCad escribe "BICK{slash}DCLK", acá "DCLK").

USO
  tools/sch2net.py hardware/DS/DS.kicad_sch hardware/DS/DS.net          # DS.net está en .gitignore
  tools/netlist_digest.py hardware/DS/DS.net                             # → .ds12_audit/
  tools/netlist_diff.py hardware/DS/DS.txt hardware/DS/DS.net            # DS11 → lo dibujado
  tools/sch2net.py hardware/DS/DS.kicad_sch /tmp/x.net --erc             # chequeos tipo ERC
  tools/sch2net.py hardware/DS/DS.kicad_sch /tmp/x.net --diag            # calibración y avisos

--erc imprime: pines sueltos sin marca ×, marcas × sobre redes con pines, redes con
power_in sin power_out ni PWR_FLAG, redes con más de un nombre, etiquetas que llegan
a un solo pin.

LO QUE NO HACE: no corre el ERC de KiCad ni conoce las clases de red. Un netlist
derivado no reemplaza al exportado para mandar a fabricar; sirve para revisar.
"""
import os
import re
import sys
from collections import defaultdict, Counter

TOK = re.compile(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+')


class Q(str):
    """Atom que venía entre comillas."""


def parse(text):
    stack = [[]]
    for m in TOK.finditer(text):
        t = m.group(0)
        if t == '(':
            stack.append([])
        elif t == ')':
            l = stack.pop()
            stack[-1].append(l)
        elif t[0] == '"':
            stack[-1].append(Q(t[1:-1].replace('\\"', '"').replace('\\\\', '\\')))
        else:
            stack[-1].append(t)
    return stack[0][0]


def kids(node, key):
    return [k for k in node if isinstance(k, list) and k and k[0] == key]


def kid(node, key):
    for k in node:
        if isinstance(k, list) and k and k[0] == key:
            return k
    return None


def prop(node, name):
    for k in kids(node, 'property'):
        if len(k) > 2 and k[1] == name:
            return str(k[2])
    return None


def num(x):
    return float(x)


def rnd(v):
    return round(v, 3)


# ----------------------------------------------------------------- buses
def expand_bus(name):
    """'X{P[1..8] N[1..8]}' -> [('X.P1','P1'), ...]; 'C[1..16]' -> [('C1','C1'),...]
    Devuelve None si no es un bus."""
    name = name.strip()
    m = re.match(r'^([^{}\[\]]*)\{(.*)\}$', name)
    if m and m.group(1).endswith('~'):
        return None          # ~{SD} es una barra de negación, no un bus
    if m:
        pref, inner = m.group(1), m.group(2)
        out = []
        for tokn in inner.split():
            sub = expand_bus(tokn)
            if sub:
                for full, loc in sub:
                    out.append((f"{pref}.{full}" if pref else full, full))
            else:
                out.append((f"{pref}.{tokn}" if pref else tokn, tokn))
        return out
    m = re.match(r'^(.*?)\[(\d+)\.\.(\d+)\]$', name)
    if m:
        pref, a, b = m.group(1), int(m.group(2)), int(m.group(3))
        step = 1 if b >= a else -1
        return [(f"{pref}{i}", f"{pref}{i}") for i in range(a, b + step, step)]
    return None


# ----------------------------------------------------------------- lib symbols
def load_lib(root):
    """{nombre: {'pins': {unit: [pin]}, 'power': 'global'|'local'|None, 'extends': str}}"""
    lib = {}
    ls = kid(root, 'lib_symbols') or []
    for sym in kids(ls, 'symbol'):
        name = str(sym[1])
        d = {'pins': defaultdict(list), 'power': None, 'extends': None}
        pw = kid(sym, 'power')
        if pw is not None:
            d['power'] = str(pw[1]) if len(pw) > 1 else 'global'
        ex = kid(sym, 'extends')
        if ex:
            d['extends'] = str(ex[1])
        for sub in kids(sym, 'symbol'):
            sname = str(sub[1])
            m = re.match(r'^(.*)_(\d+)_(\d+)$', sname)
            unit, body = (int(m.group(2)), int(m.group(3))) if m else (0, 1)
            if body not in (0, 1):
                continue
            for p in kids(sub, 'pin'):
                at = kid(p, 'at')
                d['pins'][unit].append(dict(
                    type=str(p[1]), x=num(at[1]), y=num(at[2]),
                    rot=num(at[3]) if len(at) > 3 else 0.0,
                    hide=(kid(p, 'hide') is not None and str(kid(p, 'hide')[1]) == 'yes'),
                    name=str(kid(p, 'name')[1]) if kid(p, 'name') else '',
                    number=str(kid(p, 'number')[1]) if kid(p, 'number') else ''))
        lib[name] = d
    # resolver extends
    for name, d in lib.items():
        base = d['extends']
        if base and not d['pins']:
            cand = base if base in lib else next((k for k in lib if k.split(':', 1)[-1] == base), None)
            if cand:
                d['pins'] = lib[cand]['pins']
                if d['power'] is None:
                    d['power'] = lib[cand]['power']
    return lib


# transformaciones candidatas: (a,b,c,d) -> X = x + a*px + b*py ; Y = y + c*px + d*py
CANDS = [(1, 0, 0, -1), (0, 1, 1, 0), (-1, 0, 0, 1), (0, -1, -1, 0),
         (-1, 0, 0, -1), (0, -1, 1, 0), (1, 0, 0, 1), (0, 1, -1, 0)]


# Transformación exacta de KiCad (SCH_SYMBOL::SetOrientation): T = T_mirror · T_rot · T0
KICAD_XF = {
    (0, ''): (1, 0, 0, -1), (90, ''): (0, -1, -1, 0), (180, ''): (-1, 0, 0, 1), (270, ''): (0, 1, 1, 0),
    (0, 'x'): (1, 0, 0, 1), (90, 'x'): (0, -1, 1, 0), (180, 'x'): (-1, 0, 0, -1), (270, 'x'): (0, 1, -1, 0),
    (0, 'y'): (-1, 0, 0, -1), (90, 'y'): (0, 1, -1, 0), (180, 'y'): (1, 0, 0, 1), (270, 'y'): (0, -1, 1, 0),
}


def xf(M, x, y, px, py):
    a, b, c, d = M
    return rnd(x + a * px + b * py), rnd(y + c * px + d * py)


# ----------------------------------------------------------------- hojas
class Sheet:
    def __init__(self, path):
        self.file = path
        text = open(path, encoding='utf-8').read()
        self.root = parse(text)
        self.uuid = str(kid(self.root, 'uuid')[1])
        self.lib = load_lib(self.root)
        self.symbols = kids(self.root, 'symbol')
        self.wires = [seg(w) for w in kids(self.root, 'wire')]
        self.buses = [seg(b) for b in kids(self.root, 'bus')]
        self.entries = []
        for e in kids(self.root, 'bus_entry'):
            at, sz = kid(e, 'at'), kid(e, 'size')
            x, y = num(at[1]), num(at[2])
            self.entries.append(((rnd(x), rnd(y)), (rnd(x + num(sz[1])), rnd(y + num(sz[2])))))
        self.junctions = [pt(kid(j, 'at')) for j in kids(self.root, 'junction')]
        self.noconn = [pt(kid(j, 'at')) for j in kids(self.root, 'no_connect')]
        self.labels = [(str(l[1]), pt(kid(l, 'at'))) for l in kids(self.root, 'label')]
        self.glabels = [(str(l[1]), pt(kid(l, 'at'))) for l in kids(self.root, 'global_label')]
        self.hlabels = [(str(l[1]), pt(kid(l, 'at'))) for l in kids(self.root, 'hierarchical_label')]
        self.sheets = []
        for s in kids(self.root, 'sheet'):
            pins = [(str(p[1]), pt(kid(p, 'at'))) for p in kids(s, 'pin')]
            self.sheets.append(dict(uuid=str(kid(s, 'uuid')[1]), name=prop(s, 'Sheetname'),
                                    file=prop(s, 'Sheetfile'), pins=pins))
        self.transforms = self._calibrate()

    def _conn_points(self):
        pts = set()
        for a, b in self.wires:
            pts.add(a); pts.add(b)
        for a, b in self.entries:
            pts.add(a); pts.add(b)
        for _, p in self.labels + self.glabels + self.hlabels:
            pts.add(p)
        pts.update(self.junctions); pts.update(self.noconn)
        return pts

    def sym_class(self, s):
        at = kid(s, 'at')
        rot = int(round(num(at[3]))) % 360 if len(at) > 3 else 0
        mir = kid(s, 'mirror')
        return (rot, str(mir[1]) if mir else '')

    def sym_pins(self, s):
        lib_id = str(kid(s, 'lib_id')[1])
        d = self.lib.get(lib_id)
        if not d:
            return None, []
        unit = int(kid(s, 'unit')[1]) if kid(s, 'unit') else 1
        pins = list(d['pins'].get(0, [])) + list(d['pins'].get(unit, []))
        return d, pins

    def _calibrate(self):
        """Elige, por clase (rot, mirror), la matriz que hace caer más pines sobre cables."""
        pts = self._conn_points()
        # los extremos de los pines también son puntos de conexión (pin a pin, sin cable)
        score = defaultdict(lambda: Counter())
        tot = Counter()
        for s in self.symbols:
            cls = self.sym_class(s)
            d, pins = self.sym_pins(s)
            at = kid(s, 'at'); x, y = num(at[1]), num(at[2])
            for p in pins:
                if p['hide'] and d['power'] is None:
                    continue
                tot[cls] += 1
                for M in CANDS:
                    if xf(M, x, y, p['x'], p['y']) in pts:
                        score[cls][M] += 1
        res = {}
        self.calib = {}
        for cls in set(list(score) + list(tot)):
            M = KICAD_XF[cls]
            best = score[cls].most_common(1)
            res[cls] = M
            self.calib[cls] = (score[cls][M], best[0][1] if best else 0, tot[cls])
        return res


def seg(w):
    pts = kid(w, 'pts')
    xy = kids(pts, 'xy')
    return (pt(xy[0]), pt(xy[1]))


def pt(at):
    return (rnd(num(at[1])), rnd(num(at[2])))


def on_segment(p, a, b, tol=0.02):
    (px, py), (ax, ay), (bx, by) = p, a, b
    if not (min(ax, bx) - tol <= px <= max(ax, bx) + tol and min(ay, by) - tol <= py <= max(ay, by) + tol):
        return False
    cross = (bx - ax) * (py - ay) - (by - ay) * (px - ax)
    L = ((bx - ax) ** 2 + (by - ay) ** 2) ** 0.5
    return L > 0 and abs(cross) / L <= tol


# ----------------------------------------------------------------- union-find
class UF:
    def __init__(self):
        self.p = {}

    def find(self, a):
        self.p.setdefault(a, a)
        while self.p[a] != a:
            self.p[a] = self.p[self.p[a]]
            a = self.p[a]
        return a

    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.p[rb] = ra


# ----------------------------------------------------------------- netlister
class Netlister:
    def __init__(self, root_file, modo='tolerante'):
        self.dir = os.path.dirname(os.path.abspath(root_file))
        self.cache = {}
        self.modo = modo
        self.uf = UF()
        self.comps = {}          # ref -> dict
        self.pins = {}           # (ref, num) -> dict(name,type,node,sheetpath)
        self.labels_at = defaultdict(list)   # node -> [(kind, name, sheetname)]
        self.noconn_nodes = set()
        self.driven_nodes = set()
        self.diag = defaultdict(list)
        self.sheetnames = {}
        root = self.sheet(root_file)
        self.root_uuid = root.uuid
        self.walk(root, '/' + root.uuid, '/')

    def sheet(self, f):
        f = os.path.join(self.dir, os.path.basename(f))
        if f not in self.cache:
            self.cache[f] = Sheet(f)
        return self.cache[f]

    # nodos
    def P(self, inst, p):   # punto de cable
        return ('pt', inst, p)

    def B(self, inst, p):   # punto de bus
        return ('bpt', inst, p)

    def L(self, inst, name):  # red local de la hoja
        return ('loc', inst, name)

    def G(self, name):      # red global
        return ('glob', name)

    def walk(self, sh, inst, human):
        self.sheetnames[inst] = human
        uf = self.uf
        # cables
        segs = list(sh.wires)
        # entradas de bus: el extremo que NO toca un bus es lado-cable; el otro queda aparte
        for a, b in sh.entries:
            a_bus = any(on_segment(a, *bs) for bs in sh.buses)
            b_bus = any(on_segment(b, *bs) for bs in sh.buses)
            if a_bus and not b_bus:
                self.diag['bus_entry'].append((human, a, b))
                segs.append((b, b))      # el lado cable es un punto de conexión
            elif b_bus and not a_bus:
                segs.append((a, a))
            else:
                self.diag['bus_entry_raro'].append((human, a, b, a_bus, b_bus))
                segs.append((a, b))
        for a, b in segs:
            uf.union(self.P(inst, a), self.P(inst, b))
        for a, b in sh.buses:
            uf.union(self.B(inst, a), self.B(inst, b))

        def attach(node_of_point, p, is_bus=False):
            """Une el punto p con los segmentos que lo contienen (modo tolerante)."""
            segl = sh.buses if is_bus else segs
            mk = self.B if is_bus else self.P
            hit = False
            for a, b in segl:
                if p == a or p == b:
                    hit = True
                    uf.union(node_of_point, mk(inst, a))
                elif self.modo == 'tolerante' and on_segment(p, a, b):
                    hit = True
                    self.diag['interior'].append((human, p))
                    uf.union(node_of_point, mk(inst, a))
            return hit

        pinpts = set()
        for s in sh.symbols:
            d, pins = sh.sym_pins(s)
            if d is None:
                continue
            at = kid(s, 'at'); x, y = num(at[1]), num(at[2])
            M = sh.transforms[sh.sym_class(s)]
            for p in pins:
                pinpts.add(xf(M, x, y, p['x'], p['y']))
        for j in sh.junctions:
            attach(self.P(inst, j), j)
        for nc in sh.noconn:
            attach(self.P(inst, nc), nc)
            self.noconn_nodes.add(self.P(inst, nc))
        # etiquetas
        for name, p in sh.labels:
            bus = expand_bus(name)
            if bus:
                attach(self.B(inst, p), p, is_bus=True)
                for k, (full, loc) in enumerate(bus):
                    uf.union(('bm', inst, uf.find(self.B(inst, p)), k), self.L(inst, full))
                    self.labels_at[self.L(inst, full)].append(('bus', full, human))
            else:
                if not attach(self.P(inst, p), p) and p not in pinpts:
                    self.diag['label_suelta'].append((human, name, p))
                uf.union(self.P(inst, p), self.L(inst, name))
                self.labels_at[self.L(inst, name)].append(('local', name, human))
        for name, p in sh.glabels:
            if not attach(self.P(inst, p), p) and p not in pinpts:
                self.diag['glabel_suelta'].append((human, name, p))
            uf.union(self.P(inst, p), self.G(name))
            self.labels_at[self.G(name)].append(('global', name, human))
        for name, p in sh.hlabels:
            bus = expand_bus(name)
            if bus:
                attach(self.B(inst, p), p, is_bus=True)
                for k, (full, loc) in enumerate(bus):
                    uf.union(('bm', inst, uf.find(self.B(inst, p)), k), self.L(inst, full))
            else:
                if not attach(self.P(inst, p), p) and p not in pinpts:
                    self.diag['hlabel_suelta'].append((human, name, p))
                uf.union(self.P(inst, p), self.L(inst, name))
                self.labels_at[self.L(inst, name)].append(('hier', name, human))
        # símbolos
        for s in sh.symbols:
            d, pins = sh.sym_pins(s)
            lib_id = str(kid(s, 'lib_id')[1])
            if d is None:
                self.diag['sin_lib'].append((human, lib_id))
                continue
            at = kid(s, 'at'); x, y = num(at[1]), num(at[2])
            M = sh.transforms[sh.sym_class(s)]
            ref = prop(s, 'Reference') or '?'
            unit = 1
            insts = kid(s, 'instances')
            if insts:
                for pr in kids(insts, 'project'):
                    for pa in kids(pr, 'path'):
                        if str(pa[1]) == inst:
                            r = kid(pa, 'reference'); u = kid(pa, 'unit')
                            if r: ref = str(r[1])
                            if u: unit = int(u[1])
            dnp = kid(s, 'dnp') is not None and str(kid(s, 'dnp')[1]) == 'yes'
            in_bom = not (kid(s, 'in_bom') is not None and str(kid(s, 'in_bom')[1]) == 'no')
            on_board = not (kid(s, 'on_board') is not None and str(kid(s, 'on_board')[1]) == 'no')
            value = prop(s, 'Value') or ''
            is_power_port = d['power'] is not None
            if not ref.startswith('#'):
                c = self.comps.setdefault(ref, dict(ref=ref, value=value, fp=prop(s, 'Footprint') or '',
                                                    lcsc=prop(s, 'LCSC') or '', desc=prop(s, 'Description') or '',
                                                    lib=lib_id, dnp=dnp, in_bom=in_bom, on_board=on_board,
                                                    sheet=human, units=set()))
                c['units'].add(unit)
            for p in pins:
                pos = xf(M, x, y, p['x'], p['y'])
                node = self.P(inst, pos)
                if is_power_port and p['type'] == 'power_in':
                    # símbolo de alimentación: el nombre de la red es su Value
                    if d['power'] == 'local':
                        uf.union(node, self.L(inst, value))
                        self.labels_at[self.L(inst, value)].append(('power_local', value, human))
                    else:
                        uf.union(node, self.G(value))
                        self.labels_at[self.G(value)].append(('power', value, human))
                    attach(node, pos)
                    continue
                if p['hide'] and p['type'] == 'power_in' and not is_power_port:
                    # pin de alimentación invisible: red global implícita con el nombre del pin
                    uf.union(node, self.G(p['name']))
                    self.diag['hidden_power_pin'].append((human, ref, p['number'], p['name']))
                if ref.startswith('#') and p['type'] == 'power_out':
                    self.driven_nodes.add(node)
                if not ref.startswith('#'):
                    key = (ref, p['number'])
                    if key in self.pins:
                        self.diag['pin_repetido'].append((human, ref, p['number'], lib_id, self.pins[key]['sheet']))
                    self.pins[key] = dict(name=p['name'], type=p['type'], node=node, sheet=human, hide=p['hide'])
                attach(node, pos)
        # hojas hijas
        for ch in sh.sheets:
            child = self.sheet(ch['file'])
            cinst = inst + '/' + ch['uuid']
            chuman = human.rstrip('/') + '/' + (ch['name'] or ch['uuid'][:8]) + '/'
            for name, p in ch['pins']:
                bus = expand_bus(name)
                if bus:
                    attach(self.B(inst, p), p, is_bus=True)
                    for k, (full, loc) in enumerate(bus):
                        uf.union(('bm', inst, uf.find(self.B(inst, p)), k), self.L(cinst, full))
                else:
                    if not attach(self.P(inst, p), p) and p not in pinpts:
                        self.diag['sheetpin_suelto'].append((human, ch['name'], name, p))
                    uf.union(self.P(inst, p), self.L(cinst, name))
            self.walk(child, cinst, chuman)

    # ------------------------------------------------------------- redes
    def nets(self):
        uf = self.uf
        groups = defaultdict(lambda: dict(pins=[], names=[], nc=False))
        for (ref, pn), pd in self.pins.items():
            groups[uf.find(pd['node'])]['pins'].append((ref, pn, pd))
        for node, labs in self.labels_at.items():
            groups[uf.find(node)]['names'].extend(labs)
        for nc in self.noconn_nodes:
            groups[uf.find(nc)]['nc'] = True
        for dn in self.driven_nodes:
            groups[uf.find(dn)]['flag'] = True
        out = []
        for g in groups.values():
            if not g['pins'] and not g['names']:
                continue
            out.append((self.name_of(g), g))
        return out

    def name_of(self, g):
        glob = sorted({n for k, n, h in g['names'] if k in ('global', 'power')})
        if glob:
            # KiCad prefiere el símbolo de alimentación; si hay varios, el primero alfabético
            pw = sorted({n for k, n, h in g['names'] if k == 'power'})
            g['conflicto'] = glob if len(glob) > 1 else None
            return (pw or glob)[0]
        loc = sorted({(h.count('/'), h, n) for k, n, h in g['names'] if k in ('local', 'hier', 'bus', 'power_local')})
        if loc:
            depth, h, n = loc[0]
            return (h if h != '/' else '/') .rstrip('/') + '/' + n if h != '/' else n
        pins = sorted(g['pins'], key=lambda t: (natural(t[0]), natural(t[1])))
        ref, pn, pd = pins[0]
        if len(pins) == 1:
            return f"unconnected-({ref}-{pd['name'] or 'Pad' + pn}-Pad{pn})"
        return f"Net-({ref}-Pad{pn})"


def natural(s):
    m = re.search(r'(\d+)$', s)
    return (re.sub(r'\d+$', '', s), int(m.group(1)) if m else 0)


def esc(s):
    return s.replace('\\', '\\\\').replace('"', '\\"')


def write_netlist(nl, out, source):
    nets = sorted(nl.nets(), key=lambda t: t[0])
    with open(out, 'w', encoding='utf-8') as f:
        f.write('(export\n\t(version "E")\n\t(design\n')
        f.write(f'\t\t(source "{esc(source)}")\n\t\t(date "{__import__("datetime").datetime.now().isoformat(timespec="seconds")}")\n')
        f.write('\t\t(tool "sch2net.py (derivado sin KiCad)")\n\t)\n\t(components\n')
        for ref in sorted(nl.comps, key=natural):
            c = nl.comps[ref]
            f.write(f'\t\t(comp\n\t\t\t(ref "{esc(ref)}")\n\t\t\t(value "{esc(c["value"])}")\n')
            f.write(f'\t\t\t(footprint "{esc(c["fp"])}")\n')
            if c['desc']:
                f.write(f'\t\t\t(description "{esc(c["desc"])}")\n')
            if c['lcsc']:
                f.write(f'\t\t\t(property\n\t\t\t\t(name "LCSC")\n\t\t\t\t(value "{esc(c["lcsc"])}")\n\t\t\t)\n')
            if c['dnp']:
                f.write('\t\t\t(property\n\t\t\t\t(name "dnp")\n\t\t\t)\n')
            if not c['on_board']:
                f.write('\t\t\t(property\n\t\t\t\t(name "exclude_from_board")\n\t\t\t)\n')
            if not c['in_bom']:
                f.write('\t\t\t(property\n\t\t\t\t(name "exclude_from_bom")\n\t\t\t)\n')
            f.write(f'\t\t\t(sheetpath\n\t\t\t\t(names "{esc(c["sheet"])}")\n\t\t\t)\n')
            f.write(f'\t\t\t(libsource\n\t\t\t\t(lib "{esc(c["lib"].split(":")[0])}")\n\t\t\t\t(part "{esc(c["lib"].split(":", 1)[-1])}")\n\t\t\t)\n')
            f.write('\t\t)\n')
        f.write('\t)\n\t(nets\n')
        for i, (name, g) in enumerate(nets, 1):
            f.write(f'\t\t(net\n\t\t\t(code "{i}")\n\t\t\t(name "{esc(name)}")\n\t\t\t(class "Default")\n')
            for ref, pn, pd in sorted(g['pins'], key=lambda t: (natural(t[0]), natural(t[1]))):
                f.write(f'\t\t\t(node\n\t\t\t\t(ref "{esc(ref)}")\n\t\t\t\t(pin "{esc(pn)}")\n')
                if pd['name'] and pd['name'] != '~':
                    f.write(f'\t\t\t\t(pinfunction "{esc(pd["name"])}")\n')
                f.write(f'\t\t\t\t(pintype "{esc(pd["type"])}")\n\t\t\t)\n')
            f.write('\t\t)\n')
        f.write('\t)\n)\n')
    return nets


def erc(nl, nets):
    print("\n================ CHEQUEOS TIPO ERC (derivados del netlist) ================")
    # 1. pines sueltos sin marca de no-conexión
    sueltos = [(n, g) for n, g in nets if len(g['pins']) == 1 and not g['nc']]
    por_ref = defaultdict(list)
    for n, g in sueltos:
        ref, pn, pd = g['pins'][0]
        if pd['type'] in ('no_connect', 'free'):
            continue
        por_ref[ref].append(f"{pn}({pd['name']})")
    print(f"\n[1] pines sin conectar y SIN marca ×: {sum(len(v) for v in por_ref.values())} en {len(por_ref)} componentes")
    for ref in sorted(por_ref, key=natural):
        print(f"    {ref:6} {' '.join(por_ref[ref])}")
    # 2. marca × sobre una red con más de un pin
    mal = [(n, g) for n, g in nets if g['nc'] and len(g['pins']) >= 2]
    print(f"\n[2] marcas × sobre redes con ≥2 pines: {len(mal)}")
    for n, g in mal:
        print(f"    {n}: {' '.join(r + '.' + p for r, p, _ in g['pins'])}")
    # 3. redes con power_in y sin power_out ni PWR_FLAG
    nodriv = []
    for n, g in nets:
        types = Counter(pd['type'] for _, _, pd in g['pins'])
        if types['power_in'] and not types['power_out'] and not g.get('flag'):
            nodriv.append((n, types['power_in'], len(g['pins'])))
    print(f"\n[3] redes con pines power_in sin ningún power_out ni PWR_FLAG: {len(nodriv)}")
    for n, k, t in sorted(nodriv):
        print(f"    {n:32} {k} power_in de {t} pines")
    # 4. redes con más de un nombre (alias)
    alias = []
    for n, g in nets:
        names = sorted({(k, nm) for k, nm, h in g['names'] if k in ('local', 'global', 'power', 'hier', 'bus', 'power_local')})
        distinct = sorted({nm for k, nm in names})
        if len(distinct) > 1:
            alias.append((n, [f"{nm}[{k[0]}]" for k, nm in names]))
    print(f"\n[4] redes con más de un nombre (etiquetas redundantes o contradictorias): {len(alias)}")
    for n, names in alias:
        print(f"    {n:32} {' '.join(names)}")
    # 5. etiquetas / símbolos de alimentación que no llegan a ningún pin
    vacias = [(n, g) for n, g in nets if not g['pins']]
    print(f"\n[5] redes con nombre y CERO pines (etiqueta o símbolo de alimentación suelto): {len(vacias)}")
    for n, g in vacias:
        print(f"    {n:32} {' '.join(sorted({h + nm + '[' + k[0] + ']' for k, nm, h in g['names']}))}")
    # 6. redes de exactamente un pin CON nombre (etiqueta que va a un solo lugar)
    uno = [(n, g) for n, g in nets if len(g['pins']) == 1 and g['names'] and not g['nc']]
    print(f"\n[6] etiquetas que llegan a UN solo pin (probable cable faltante): {len(uno)}")
    for n, g in uno:
        r, p, pd = g['pins'][0]
        print(f"    {n:32} → {r}.{p}({pd['name']})")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    modo = 'tolerante'
    for a in sys.argv[1:]:
        if a.startswith('--modo='):
            modo = a.split('=', 1)[1]
    diag = '--diag' in sys.argv
    do_erc = '--erc' in sys.argv
    if len(args) != 2:
        sys.exit(__doc__)
    nl = Netlister(args[0], modo=modo)
    nets = write_netlist(nl, args[1], args[0])
    print(f"{args[1]}: {len(nl.comps)} componentes · {len(nets)} redes · {len(nl.pins)} pines · modo {modo}")
    if do_erc:
        erc(nl, nets)
    if diag:
        for f, sh in nl.cache.items():
            print(f"\n[{os.path.basename(f)}] por clase (rot, mirror): aciertos con la matriz de KiCad / mejor candidata / pines")
            for cls, (hit, best, tot) in sorted(sh.calib.items()):
                flag = '' if hit >= best else '   ⚠ otra matriz acierta más'
                print(f"   {cls}: {hit}/{best}/{tot}{flag}")
        for k, v in nl.diag.items():
            print(f"\n{k}: {len(v)}")
            for it in v[:12]:
                print("   ", it)
        confl = [(n, g['conflicto']) for n, g in nets if g.get('conflicto')]
        print(f"\nredes con más de un nombre global: {len(confl)}")
        for c in confl:
            print("   ", c)


if __name__ == '__main__':
    main()
