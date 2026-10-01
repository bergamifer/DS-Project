#!/usr/bin/env python3
"""pcb_status.py — estado de un .kicad_pcb sin KiCad: qué hay puesto, qué falta rutear.

USO
  tools/pcb_status.py hardware/DS/DS.kicad_pcb [hardware/DS/DS.net] [salida.pcb.net]

Imprime: huellas dentro/fuera del contorno (las nuevas caen afuera al importar),
redes con pads y CERO cobre, cobre sin red (huérfano de componentes que se movieron
o borraron), pads ubicados que tocan una pista de OTRA red (lo que el DRC va a
marcar), una estimación de conexiones sin rutear, y —si se le pasa el netlist del
esquemático (tools/sch2net.py)— qué designadores faltan importar. Además escribe un
netlist derivado de los pads del PCB (mismo formato), para comparar conectividad
esquemático ↔ PCB con tools/netlist_diff.py.

AUTOPRUEBA
Corrido sobre el PCB de la DS11 tal como se fabricó (tag ds11-release, DRC en 0):
0 huellas fuera del contorno, 0 redes sin cobre, 0 cobre huérfano, y las mismas
conexiones que su esquemático. Sus falsos positivos conocidos en ese archivo, que hay
que descontar: pads pasantes de J9 y H4 "tocando" pistas internas (KiCad quita el
anillo de las capas sin uso), y la isla de +5VA (5 conexiones que sí están).

LO QUE NO HACE: no es un DRC. Los rellenos de zona los toma como están guardados,
que es justamente lo que check_pcb.sh verifica.
"""
import os
import re
import sys
from collections import defaultdict, Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sch2net import parse, kids, kid, prop, num, natural, UF  # noqa: E402


def bbox_pts(pts):
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    return min(xs), min(ys), max(xs), max(ys)


def rot(px, py, ang):
    # Convención de KiCad (eje Y hacia abajo): un ángulo positivo gira en sentido antihorario
    # EN PANTALLA, que en coordenadas y-abajo es  x' = x·cos + y·sin ;  y' = −x·sin + y·cos.
    import math
    a = math.radians(ang)
    return px * math.cos(a) + py * math.sin(a), -px * math.sin(a) + py * math.cos(a)


def point_in_poly(x, y, poly):
    inside = False
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]; x2, y2 = poly[(i + 1) % n]
        if (y1 > y) != (y2 > y):
            xin = (x2 - x1) * (y - y1) / (y2 - y1) + x1
            if x < xin:
                inside = not inside
    return inside


def main():
    pcb = parse(open(sys.argv[1], encoding='utf-8').read())
    # KiCad 10 escribe la tabla como (net N "nombre") y en pads/pistas sólo (net "nombre").
    netnames = {}
    for n in kids(pcb, 'net'):
        if len(n) > 2:
            netnames[str(n[2])] = str(n[2])
        else:
            netnames[str(n[1])] = str(n[1])

    def netno(n):
        if n is None:
            return None
        if len(n) > 2:
            return str(n[2])
        v = str(n[1])
        return v if not v.lstrip('-').isdigit() else netnames.get(v, v)
    # ---- contorno
    edge = []
    for k in ('gr_line', 'gr_rect', 'gr_arc', 'gr_circle', 'gr_poly'):
        for g in kids(pcb, k):
            ly = kid(g, 'layer')
            if ly and str(ly[1]) == 'Edge.Cuts':
                for key in ('start', 'end', 'mid', 'center'):
                    for it in kids(g, key):
                        edge.append((num(it[1]), num(it[2])))
                pts = kid(g, 'pts')
                if pts:
                    edge += [(num(p[1]), num(p[2])) for p in kids(pts, 'xy')]
    bb = bbox_pts(edge) if edge else None
    print(f"redes declaradas en el PCB: {len(netnames)}")
    print(f"contorno (Edge.Cuts): {bb and tuple(round(v, 2) for v in bb)}  → {bb and round(bb[2]-bb[0],2)} × {bb and round(bb[3]-bb[1],2)} mm")

    # ---- footprints y pads
    fps = []
    pads = []   # dict(ref, num, net, x, y, w, h, layers)
    for fp in kids(pcb, 'footprint'):
        at = kid(fp, 'at'); fx, fy = num(at[1]), num(at[2]); frot = num(at[3]) if len(at) > 3 else 0.0
        ref = prop(fp, 'Reference') or '?'
        layer = str(kid(fp, 'layer')[1])
        attrs = [str(a) for a in (kid(fp, 'attr') or [])[1:]]
        d = dict(ref=ref, lib=str(fp[1]), x=fx, y=fy, layer=layer, attrs=attrs,
                 value=prop(fp, 'Value') or '', lcsc=prop(fp, 'LCSC') or '',
                 inside=(bb is not None and bb[0] - 0.5 <= fx <= bb[2] + 0.5 and bb[1] - 0.5 <= fy <= bb[3] + 0.5),
                 pads=0, pads_con_red=0)
        for pd in kids(fp, 'pad'):
            pat = kid(pd, 'at'); px, py = num(pat[1]), num(pat[2])
            prot = num(pat[3]) if len(pat) > 3 else frot
            sz = kid(pd, 'size'); w, h = num(sz[1]), num(sz[2])
            rx, ry = rot(px, py, frot)
            net = kid(pd, 'net')
            layers = [str(l) for l in (kid(pd, 'layers') or [])[1:]]
            d['pads'] += 1
            if net:
                d['pads_con_red'] += 1
                pads.append(dict(ref=ref, num=str(pd[1]), net=netno(net), x=fx + rx, y=fy + ry, rot=prot, w0=w, h0=h,
                                 w=h if 45 < (prot % 180) < 135 else w,
                                 h=w if 45 < (prot % 180) < 135 else h, layers=layers,
                                 th=(str(pd[2]) == 'thru_hole')))
        fps.append(d)
    print(f"footprints: {len(fps)}  · fuera del contorno: {sum(1 for f in fps if not f['inside'])}  · pads con red: {len(pads)}")
    fuera = sorted([f for f in fps if not f['inside']], key=lambda f: natural(f['ref']))
    if fuera:
        print("  fuera del contorno (sin ubicar):", ' '.join(f"{f['ref']}" for f in fuera))
    # ---- cobre
    segs = [(num(kid(s, 'start')[1]), num(kid(s, 'start')[2]), num(kid(s, 'end')[1]), num(kid(s, 'end')[2]),
             str(kid(s, 'layer')[1]), netno(kid(s, 'net')), num(kid(s, 'width')[1])) for s in kids(pcb, 'segment')]
    for a in kids(pcb, 'arc'):
        st, md, en = kid(a, 'start'), kid(a, 'mid'), kid(a, 'end')
        ly, nt, wd = str(kid(a, 'layer')[1]), netno(kid(a, 'net')), num(kid(a, 'width')[1])
        segs.append((num(st[1]), num(st[2]), num(md[1]), num(md[2]), ly, nt, wd))
        segs.append((num(md[1]), num(md[2]), num(en[1]), num(en[2]), ly, nt, wd))
    vias = [(num(kid(v, 'at')[1]), num(kid(v, 'at')[2]), netno(kid(v, 'net')), num(kid(v, 'size')[1])) for v in kids(pcb, 'via')]
    zones = []
    for z in kids(pcb, 'zone'):
        nn = (str(kid(z, 'net_name')[1]) if kid(z, 'net_name') else None) or netno(kid(z, 'net')) or 0
        layers = [str(l) for l in (kid(z, 'layers') or kid(z, 'layer') or [])[1:]]
        polys = []
        for fpz in kids(z, 'filled_polygon'):
            ly = str(kid(fpz, 'layer')[1])
            pts = [(num(p[1]), num(p[2])) for p in kids(kid(fpz, 'pts'), 'xy')]
            polys.append((ly, pts, bbox_pts(pts)))
        zones.append(dict(net=nn, layers=layers, polys=polys, name=(kid(z, 'net_name') and str(kid(z, 'net_name')[1]))))
    print(f"pistas: {len(segs)} (incl. {2 * len(kids(pcb, 'arc'))} cuerdas de arcos) · vías: {len(vias)} · zonas: {len(zones)} ({sum(len(z['polys']) for z in zones)} polígonos rellenos)")
    huerf = [x for x in segs if x[5] in (None, '')] + [v for v in vias if v[2] in (None, '')]
    print(f"cobre SIN red (pistas/vías huérfanas de componentes que se movieron o borraron): {len(huerf)}")
    for z in zones:
        print(f"   zona red={z['net']!r:14} capas={','.join(z['layers']):20} polígonos rellenos={len(z['polys'])}")

    # ---- redes: pads vs cobre
    by_net_pads = defaultdict(list)
    for p in pads:
        by_net_pads[p['net']].append(p)
    seg_by_net = Counter(s[5] for s in segs); via_by_net = Counter(v[2] for v in vias)
    zone_nets = {z['net'] for z in zones if z['polys']}
    sin_cobre = [(n, ps) for n, ps in by_net_pads.items() if len(ps) >= 2 and seg_by_net[n] == 0 and via_by_net[n] == 0 and n not in zone_nets]
    print(f"\nredes con ≥2 pads y CERO cobre (ni pista, ni vía, ni zona): {len(sin_cobre)}")
    for n, ps in sorted(sin_cobre, key=lambda t: netnames.get(t[0], '')):
        refs = sorted({p['ref'] for p in ps}, key=natural)
        print(f"   {netnames.get(n, n):28} {len(ps):3} pads  {' '.join(refs[:14])}{' …' if len(refs) > 14 else ''}")

    # ---- conectividad estimada por red (ratsnest)
    def pad_hit(p, x, y, tol=0.02):
        return abs(x - p['x']) <= p['w'] / 2 + tol and abs(y - p['y']) <= p['h'] / 2 + tol

    def dist_pt_seg(px, py, x1, y1, x2, y2):
        dx, dy = x2 - x1, y2 - y1
        L2 = dx * dx + dy * dy
        t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((px - x1) * dx + (py - y1) * dy) / L2))
        cx, cy = x1 + t * dx, y1 + t * dy
        return ((px - cx) ** 2 + (py - cy) ** 2) ** 0.5

    def seg_touches_pad(p, s):
        # distancia real entre el segmento y el rectángulo del pad (en el marco del pad)
        import math
        a = math.radians(-p.get('rot', 0.0))
        def loc(x, y):
            dx, dy = x - p['x'], y - p['y']
            return dx * math.cos(a) + dy * math.sin(a), -dx * math.sin(a) + dy * math.cos(a)
        (x1, y1), (x2, y2) = loc(s[0], s[1]), loc(s[2], s[3])
        hw, hh = p['w0'] / 2, p['h0'] / 2
        tol = s[6] / 2 + 0.01
        # ¿algún extremo dentro del rectángulo?
        if (-hw <= x1 <= hw and -hh <= y1 <= hh) or (-hw <= x2 <= hw and -hh <= y2 <= hh):
            return True
        # ¿el segmento pasa a menos de tol de alguna arista, o cruza el rectángulo?
        edges = [((-hw, -hh), (hw, -hh)), ((hw, -hh), (hw, hh)), ((hw, hh), (-hw, hh)), ((-hw, hh), (-hw, -hh))]
        def seg_seg(ax, ay, bx, by, cx, cy, dx, dy):
            def ccw(px, py, qx, qy, rx, ry):
                return (ry - py) * (qx - px) - (qy - py) * (rx - px)
            if (ccw(ax, ay, cx, cy, dx, dy) * ccw(bx, by, cx, cy, dx, dy) < 0 and
                    ccw(cx, cy, ax, ay, bx, by) * ccw(dx, dy, ax, ay, bx, by) < 0):
                return 0.0
            return min(dist_pt_seg(ax, ay, cx, cy, dx, dy), dist_pt_seg(bx, by, cx, cy, dx, dy),
                       dist_pt_seg(cx, cy, ax, ay, bx, by), dist_pt_seg(dx, dy, ax, ay, bx, by))
        return any(seg_seg(x1, y1, x2, y2, e[0][0], e[0][1], e[1][0], e[1][1]) <= tol for e in edges)

    def pad_on_layer(p, ly):
        return p['th'] or ly in p['layers'] or '*.Cu' in p['layers'] or any(l.endswith('.Cu') and l == ly for l in p['layers'])

    total_unrouted = 0
    unrouted_by_net = []
    unplaced = {f['ref'] for f in fps if not f['inside']}
    aislados = defaultdict(list)
    for n, ps_all in by_net_pads.items():
        ps = [p for p in ps_all if p['ref'] not in unplaced]
        if len(ps) < 2 or n in (None, '', '-1'):
            continue
        uf = UF()
        nodes = [('pad', i) for i in range(len(ps))]
        nsegs = [s for s in segs if s[5] == n]
        nvias = [v for v in vias if v[2] == n]
        nzones = [z for z in zones if z['net'] == n]
        # pistas: extremos → pads en la misma capa; vías → cualquier capa
        for si, s in enumerate(nsegs):
            uf.find(('seg', si))
            for i, p in enumerate(ps):
                if pad_on_layer(p, s[4]) and seg_touches_pad(p, s):
                    uf.union(('seg', si), ('pad', i))
            for vi, v in enumerate(nvias):
                if dist_pt_seg(v[0], v[1], s[0], s[1], s[2], s[3]) <= v[3] / 2 + s[6] / 2 - 0.01:
                    uf.union(('seg', si), ('via', vi))
            # pista a pista: un extremo de una sobre el cuerpo de la otra, misma capa
            for sj, t in enumerate(nsegs[:si]):
                if t[4] != s[4]:
                    continue
                tol = (s[6] + t[6]) / 2 - 0.01
                if (dist_pt_seg(s[0], s[1], t[0], t[1], t[2], t[3]) <= tol or dist_pt_seg(s[2], s[3], t[0], t[1], t[2], t[3]) <= tol
                        or dist_pt_seg(t[0], t[1], s[0], s[1], s[2], s[3]) <= tol or dist_pt_seg(t[2], t[3], s[0], s[1], s[2], s[3]) <= tol):
                    uf.union(('seg', si), ('seg', sj))
        for vi, v in enumerate(nvias):
            for i, p in enumerate(ps):
                if pad_hit(p, v[0], v[1], tol=v[3] / 2):
                    uf.union(('via', vi), ('pad', i))
        for zi, z in enumerate(nzones):
            for ly, poly, zb in z['polys']:
                for i, p in enumerate(ps):
                    if not pad_on_layer(p, ly):
                        continue
                    if not (zb[0] - 1 <= p['x'] <= zb[2] + 1 and zb[1] - 1 <= p['y'] <= zb[3] + 1):
                        continue
                    if point_in_poly(p['x'], p['y'], poly) or any(pad_hit(p, vx, vy, 0.06) for vx, vy in poly):
                        uf.union(('zone', zi), ('pad', i))
                for vi, v in enumerate(nvias):
                    if zb[0] <= v[0] <= zb[2] and zb[1] <= v[1] <= zb[3] and point_in_poly(v[0], v[1], poly):
                        uf.union(('zone', zi), ('via', vi))
                for si, s in enumerate(nsegs):
                    if s[4] == ly and ((zb[0] <= s[0] <= zb[2] and zb[1] <= s[1] <= zb[3] and point_in_poly(s[0], s[1], poly))
                                       or (zb[0] <= s[2] <= zb[2] and zb[1] <= s[3] <= zb[3] and point_in_poly(s[2], s[3], poly))):
                        uf.union(('zone', zi), ('seg', si))
        roots = Counter(uf.find(nd) for nd in nodes)
        comps = len(roots)
        if comps > 1:
            total_unrouted += comps - 1
            unrouted_by_net.append((comps - 1, str(n), len(ps)))
            main_root = roots.most_common(1)[0][0]
            for i, p in enumerate(ps):
                if uf.find(('pad', i)) != main_root:
                    aislados[str(n)].append(f"{p['ref']}.{p['num']}")
    cortos = defaultdict(list)
    for p in pads:
        if p['ref'] in unplaced or not p['net']:
            continue
        for s_ in segs:
            # en pads pasantes sólo miramos F.Cu y B.Cu: KiCad quita el anillo de las capas
            # internas sin uso, y una pista interna que pasa por debajo no es un corto
            if p['th'] and s_[4] not in ('F.Cu', 'B.Cu'):
                continue
            if s_[5] and s_[5] != p['net'] and pad_on_layer(p, s_[4]) and seg_touches_pad(p, s_):
                cortos[(p['ref'], p['num'], p['net'])].append((s_[5], s_[4]))
    print(f"\nPADS UBICADOS QUE TOCAN UNA PISTA DE OTRA RED (corto en potencia, lo que el DRC va a marcar): {len(cortos)}")
    for (r, n, nt), others in sorted(cortos.items(), key=lambda kv: (natural(kv[0][0]), natural(kv[0][1]))):
        print(f"   {r}.{n:<4} ({nt}) toca {sorted(set(others))}")
    print(f"\nconexiones sin rutear (estimación, sólo footprints dentro del contorno): {total_unrouted} en {len(unrouted_by_net)} redes")
    for nm in ('GND', '+3V3', '+5VA', '+5V_AUX', 'SAI1_MCLK'):
        if aislados.get(nm):
            print(f"   pads aislados en {nm}: {' '.join(aislados[nm][:60])}")
    for k, nm, npads in sorted(unrouted_by_net, reverse=True)[:40]:
        print(f"   {k:3} faltan   {nm:30} ({npads} pads)")

    # ---- netlist derivado del PCB, mismo formato, para tools/netlist_diff.py
    out = os.path.splitext(sys.argv[1])[0] + '.pcb.net' if len(sys.argv) < 4 else sys.argv[3]
    from sch2net import esc
    with open(out, 'w', encoding='utf-8') as f:
        f.write('(export\n\t(version "E")\n\t(design\n\t\t(source "' + esc(sys.argv[1]) + '")\n\t\t(date "pcb")\n\t\t(tool "pcb_status.py")\n\t)\n\t(components\n')
        for fp_ in sorted(fps, key=lambda f_: natural(f_['ref'])):
            f.write(f'\t\t(comp\n\t\t\t(ref "{esc(fp_["ref"])}")\n\t\t\t(value "{esc(fp_["value"])}")\n\t\t\t(footprint "{esc(fp_["lib"])}")\n')
            if fp_['lcsc']:
                f.write(f'\t\t\t(property\n\t\t\t\t(name "LCSC")\n\t\t\t\t(value "{esc(fp_["lcsc"])}")\n\t\t\t)\n')
            f.write('\t\t)\n')
        f.write('\t)\n\t(nets\n')
        for i, (n, ps) in enumerate(sorted(by_net_pads.items(), key=lambda kv: str(kv[0])), 1):
            if not n:
                continue
            f.write(f'\t\t(net\n\t\t\t(code "{i}")\n\t\t\t(name "{esc(str(n))}")\n\t\t\t(class "Default")\n')
            for p in sorted(ps, key=lambda p_: (natural(p_['ref']), natural(p_['num']))):
                f.write(f'\t\t\t(node\n\t\t\t\t(ref "{esc(p["ref"])}")\n\t\t\t\t(pin "{esc(p["num"])}")\n\t\t\t)\n')
            f.write('\t\t)\n')
        f.write('\t)\n)\n')
    print(f"\nnetlist derivado del PCB: {out}")

    # ---- comparación con el esquemático
    if len(sys.argv) > 2:
        from netlist_digest import leer
        comps, nets, pin2net, meta = leer(sys.argv[2])
        excl = set()
        src = open(sys.argv[2], encoding='utf-8').read()
        for b in src.split('\n\t\t(comp\n')[1:]:
            if '(name "exclude_from_board")' in b:
                m = re.search(r'\(ref "([^"]*)"\)', b)
                if m:
                    excl.add(m.group(1))
        sch_refs = {c['ref'] for c in comps} - excl
        print(f"(excluidos del PCB en el esquemático: {len(excl)}  {' '.join(sorted(excl, key=natural))})")
        pcb_refs = {f['ref'] for f in fps}
        print(f"\n== esquemático ({meta['fecha']}) vs PCB ==")
        solo_sch = sorted(sch_refs - pcb_refs, key=natural); solo_pcb = sorted(pcb_refs - sch_refs, key=natural)
        print(f"en el esquemático y NO en el PCB: {len(solo_sch)}  {' '.join(solo_sch)}")
        print(f"en el PCB y NO en el esquemático: {len(solo_pcb)}  {' '.join(solo_pcb)}")
        # valor / footprint / LCSC distintos
        fpd = {f['ref']: f for f in fps}
        dif = []
        for c in comps:
            f = fpd.get(c['ref'])
            if not f:
                continue
            if c['fp'] and f['lib'].split(':')[-1] != c['fp']:
                dif.append((c['ref'], 'footprint', c['fp'], f['lib'].split(':')[-1]))
            if c['val'] != f['value']:
                dif.append((c['ref'], 'valor', c['val'], f['value']))
            if c['lcsc'] and f['lcsc'] and c['lcsc'] != f['lcsc']:
                dif.append((c['ref'], 'LCSC', c['lcsc'], f['lcsc']))
        print(f"designadores comunes con valor/footprint/LCSC distinto: {len(dif)}")
        for d in sorted(dif, key=lambda d: natural(d[0]))[:60]:
            print(f"   {d[0]:7} {d[1]:9} sch={d[2]!r:34} pcb={d[3]!r}")
        # redes: pin a pin
        pcb_pin2net = {(p['ref'], p['num']): (p['net'] or '') for p in pads}
        sch_netnames = {n for n, _ in nets}
        pcb_only = sorted(set(netnames) - sch_netnames - {''}); sch_only = sorted(sch_netnames - set(netnames))
        print(f"redes que están en el PCB y no en el esquemático: {len(pcb_only)}  {' '.join(pcb_only[:40])}{' …' if len(pcb_only) > 40 else ''}")
        print(f"redes del esquemático que el PCB no conoce: {len(sch_only)}  {' '.join(sch_only[:40])}{' …' if len(sch_only) > 40 else ''}")
        cambios = defaultdict(list)
        for (ref, pin), sn in pin2net.items():
            pn = pcb_pin2net.get((ref, pin))
            if pn is None:
                continue
            if sn != pn and not (sn.startswith('unconnected-') and pn == ''):
                cambios[(sn, pn)].append(f"{ref}.{pin}")
        sinred = [(r, p) for (r, p), pn in pcb_pin2net.items() if pn == '' and (r, p) in pin2net and not pin2net[(r, p)].startswith('unconnected-')]
        print(f"pads del PCB SIN red cuyo pin en el esquemático sí tiene red: {len(sinred)}  {' '.join(r + '.' + p for r, p in sorted(sinred, key=lambda t: (natural(t[0]), natural(t[1])))[:40])}")
        print("(la comparación red por red la hace tools/netlist_diff.py sobre el .pcb.net de arriba)")


if __name__ == '__main__':
    main()
