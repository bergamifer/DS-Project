#!/usr/bin/env python3
"""netlist_digest.py — convierte el netlist de KiCad en tres archivos grepables.

POR QUÉ EXISTE
--------------
`hardware/DS/DS.txt` son 70.454 líneas de S-expressions. Cualquier pregunta de
conectividad ("¿qué toca esta red?", "¿qué pin quedó al aire?") obliga a
parsearlo entero, y si hay varias personas —o varios agentes— preguntando a la
vez, cada una repite el mismo trabajo caro. Esto lo hace UNA vez.

En la auditoría del 2026-09-11 esa diferencia fue decisiva: doce revisiones
paralelas parseando el netlist cada una por su cuenta agotaron el presupuesto
de la sesión sin producir nada.

QUÉ GENERA (en .ds12_audit/, que está en .gitignore por ser derivado)
  componentes.tsv   ref · value · footprint · LCSC · description
  redes.txt         una línea por red: nombre · nº de nodos · ref.pin(función)…
  pines_por_ic.txt  por cada U/IC/J/Q/L/FB: el chip y todos sus pines con su red

EL TRUCO ÚTIL: `redes.txt` trae la CANTIDAD DE NODOS por red, así que encontrar
pines flotantes es un grep en vez de una investigación:
    awk -F'\\t' '$2==1' redes.txt     # pines que no van a ningún lado
    awk -F'\\t' '$2==2' redes.txt     # redes punto a punto

⚠️ El netlist NO es la fuente de verdad para el stock ni para los part numbers
comprados: para eso es `hardware/DS/jlcpcb/production_files/BOM-DS.csv`. Los dos
NO coinciden (p. ej. el 4,7 nF: DS.txt dice C883564, el BOM dice C2961271).

USO:  tools/netlist_digest.py [netlist] [directorio_de_salida]
      Sin argumentos usa hardware/DS/DS.net (el diseño en curso) y, si no existe,
      hardware/DS/DS.txt (la DS11 fabricada, congelada como referencia).
      Siempre imprime de cuándo es el netlist: un netlist viejo es una fuente
      clásica de falsos hallazgos — reexportalo antes de sacar conclusiones.
"""
import os
import re
import sys

def _por_defecto():
    # DS.net = el que exporta KiCad del diseño en curso (DS12).
    # DS.txt = el de la DS11 FABRICADA: se conserva congelado como referencia.
    return 'hardware/DS/DS.net' if os.path.exists('hardware/DS/DS.net') else 'hardware/DS/DS.txt'


NETLIST = sys.argv[1] if len(sys.argv) > 1 else _por_defecto()
OUT = sys.argv[2] if len(sys.argv) > 2 else '.ds12_audit'


def natural(ref):
    """R10 después de R9, no antes."""
    m = re.search(r'(\d+)$', ref)
    return (re.sub(r'\d+$', '', ref), int(m.group(1)) if m else 0)


def leer(ruta):
    """Lee un netlist de KiCad (.txt o .net) y devuelve (comps, nets, pin2net, meta).

    comps    lista de dicts: ref, val, fp, lcsc, desc
    nets     lista de (nombre, [(ref, pin, funcion), ...])
    pin2net  {(ref, pin): nombre_de_red}
    meta     {'fecha': ..., 'origen': ...}  — cuándo y desde qué esquemático se exportó
    """
    src = open(ruta, encoding='utf-8', errors='replace').read()
    fecha = re.search(r'\(date "([^"]*)"\)', src)
    origen = re.search(r'\(source "([^"]*)"\)', src)
    meta = {'fecha': fecha.group(1) if fecha else '?',
            'origen': os.path.basename(origen.group(1)) if origen else '?'}

    comps = []
    for b in src.split('\n\t\t(comp\n')[1:]:
        g = lambda p: (re.search(p, b).group(1) if re.search(p, b) else '')
        lcsc = re.search(r'\(name "LCSC"\) "([^"]*)"\)', b) \
            or re.search(r'\(name "LCSC"\)\s*\n\s*\(value "([^"]*)"\)', b)
        comps.append(dict(
            ref=g(r'\(ref "([^"]*)"\)'), val=g(r'\(value "([^"]*)"\)'),
            fp=g(r'\(footprint "([^"]*)"\)').split(':')[-1],
            lcsc=lcsc.group(1) if lcsc else '',
            desc=g(r'\(description "([^"]*)"\)')[:90]))

    ns = src.find('\t(nets')
    nets, pin2net = [], {}
    for nb in re.split(r'\n\t\t\(net\n', src[ns:])[1:]:
        nm = re.search(r'\(name "([^"]*)"\)', nb)
        nm = nm.group(1) if nm else '?'
        nodes = re.findall(
            r'\(ref "([^"]+)"\)\s*\n\s*\(pin "([^"]+)"\)'
            r'(?:\s*\n\s*\(pinfunction "([^"]*)"\))?', nb)
        if not nodes:
            nodes = [(m.group(1), m.group(2), '') for m in
                     re.finditer(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', nb)]
        nets.append((nm, nodes))
        for r, p, fn in nodes:
            pin2net[(r, p)] = nm
    return comps, nets, pin2net, meta


def main():
    if not os.path.exists(NETLIST):
        sys.exit(f"no existe {NETLIST}")
    os.makedirs(OUT, exist_ok=True)
    comps, nets, pin2net, meta = leer(NETLIST)
    print(f"netlist: {NETLIST}  (exportado {meta['fecha']} desde {meta['origen']})")

    with open(f'{OUT}/componentes.tsv', 'w') as f:
        f.write("ref\tvalue\tfootprint\tLCSC\tdescription\n")
        for c in sorted(comps, key=lambda c: natural(c['ref'])):
            f.write(f"{c['ref']}\t{c['val']}\t{c['fp']}\t{c['lcsc']}\t{c['desc']}\n")

    # ---- redes ----
    with open(f'{OUT}/redes.txt', 'w') as f:
        f.write("# net\tnodos\tref.pin(pinfunction) ...\n")
        f.write("# pines al aire:  awk -F'\\t' '$2==1' redes.txt\n")
        for nm, nodes in sorted(nets):
            f.write(f"{nm}\t{len(nodes)}\t" + " ".join(
                f"{r}.{p}({fn})" if fn else f"{r}.{p}" for r, p, fn in nodes) + "\n")

    # ---- pines por IC ----
    keyrefs = sorted({c['ref'] for c in comps if re.match(r'^(U|IC|J|Q|L|FB)', c['ref'])},
                     key=natural)
    with open(f'{OUT}/pines_por_ic.txt', 'w') as f:
        for ref in keyrefs:
            pins = sorted(((p, n) for (r, p), n in pin2net.items() if r == ref),
                          key=lambda x: (len(x[0]), x[0]))
            if not pins:
                continue
            c = next((c for c in comps if c['ref'] == ref), {})
            f.write(f"\n=== {ref}  {c.get('val','')}  [{c.get('fp','')}]  {c.get('lcsc','')}\n")
            for p, n in pins:
                f.write(f"  pin {p:>4} -> {n}\n")

    solos = sum(1 for _, nd in nets if len(nd) == 1)
    print(f"✅ {OUT}/ — {len(comps)} componentes · {len(nets)} redes · {len(pin2net)} nodos")
    for fn in ('componentes.tsv', 'redes.txt', 'pines_por_ic.txt'):
        print(f"   {fn:20} {os.path.getsize(os.path.join(OUT, fn)) // 1024:4} KB")
    print(f"   ⚠️ {solos} redes con UN SOLO nodo (pines que no van a ningún lado):")
    print(f"      awk -F'\\t' '$2==1' {OUT}/redes.txt")


if __name__ == '__main__':
    main()
