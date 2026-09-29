#!/usr/bin/env python3
"""netlist_diff.py — qué cambió ELÉCTRICAMENTE entre dos exportaciones del netlist.

POR QUÉ EXISTE
--------------
Mientras se dibuja en KiCad, un cable corrido por accidente, una etiqueta
duplicada o un pin que quedó en otra red **no hace saltar ningún error**: el ERC
sólo ve pines sin conectar o tipos incompatibles, y una conexión equivocada
sigue siendo una conexión. El diff de git tampoco ayuda: cada vez que KiCad
guarda reescribe miles de líneas (UUIDs, orden, campos ocultos).

Esto compara lo único que importa —la CONECTIVIDAD— y lista, pin por pin, cada
lugar donde algo cambió de red. Si todo lo de la lista fue intencional, el
cambio es correcto. Si aparece algo que no recordás haber tocado, es el cable
que se movió solo.

CÓMO COMPARA
------------
Para cada pin que existe en las DOS versiones, mira con QUÉ OTROS pines (también
comunes) comparte red. Si esa vecindad cambió, el pin cambió de conexión.
Los componentes nuevos no cuentan como vecinos: agregar un capacitor de
desacople a +3V3 no altera la conexión de nada más. Los renombres puros de red
(p. ej. +5V → +5V_AUX) se listan aparte porque no cambian la electricidad.

USO
  tools/netlist_diff.py VIEJO NUEVO
  tools/netlist_diff.py hardware/DS/DS.txt hardware/DS/DS.net      # DS11 → DS12
  tools/netlist_diff.py /tmp/ayer.net hardware/DS/DS.net            # lo de hoy

⚠️ Los dos archivos tienen que ser netlists exportados por KiCad, y conviene que
el nuevo sea RECIÉN exportado: el script imprime la fecha de cada uno.
"""
import os
import re
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from netlist_digest import leer, natural   # noqa: E402  (el lector se comparte)

LIMITE = 14          # cuántos pines mostrar por grupo antes de "…y N más"
CAMPOS = (('val', 'valor'), ('fp', 'footprint'), ('lcsc', 'LCSC'))


def corto(nm):
    """Nombre de red legible: sin la ruta de la hoja."""
    return nm.rsplit('/', 1)[-1] or nm


def lista(nodos, fn, limite=LIMITE):
    nodos = sorted(nodos, key=lambda n: (natural(n[0]), n[1]))
    txt = []
    for r, p in nodos[:limite]:
        f = fn.get((r, p), '')
        f = re.sub(r'_\w+$', '', f)          # "VIN_1" → "VIN"
        txt.append(f"{r}.{p}" + (f"({f})" if f and f != p else ''))
    extra = len(nodos) - limite
    return '  '.join(txt) + (f"  …y {extra} más" if extra > 0 else '')


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    ra, rb = sys.argv[1], sys.argv[2]
    for r in (ra, rb):
        if not os.path.exists(r):
            sys.exit(f"no existe {r}")
    ca, na, pa, ma = leer(ra)
    cb, nb, pb, mb = leer(rb)
    print(f"VIEJO  {ra}   (exportado {ma['fecha']})")
    print(f"NUEVO  {rb}   (exportado {mb['fecha']})")

    fn = {}
    for _, nodos in na + nb:
        for r, p, f in nodos:
            fn[(r, p)] = f

    # ---------------- componentes ----------------
    da = {c['ref']: c for c in ca}
    db = {c['ref']: c for c in cb}
    agregados = sorted(set(db) - set(da), key=natural)
    quitados = sorted(set(da) - set(db), key=natural)
    print(f"\n== COMPONENTES: {len(agregados)} agregados · {len(quitados)} quitados ==")
    for r in agregados:
        c = db[r]
        print(f"  + {r:6} {c['val'][:22]:22} {c['fp'][:30]:30} {c['lcsc'] or '⚠ sin LCSC'}")
    for r in quitados:
        c = da[r]
        print(f"  − {r:6} {c['val'][:22]:22} {c['fp'][:30]:30} {c['lcsc']}")
    cambios = []
    for r in sorted(set(da) & set(db), key=natural):
        for k, nombre in CAMPOS:
            if da[r][k] != db[r][k]:
                cambios.append((r, nombre, da[r][k], db[r][k]))
    print(f"\n== COMPONENTES QUE CAMBIARON DE VALOR/FOOTPRINT/LCSC: {len(cambios)} ==")
    for r, nombre, x, y in cambios:
        print(f"  ~ {r:6} {nombre:9} {x or '—'}  →  {y or '—'}")

    # ---------------- conectividad ----------------
    # Un pin "se movió" si su red NUEVA no conserva la mayoría de los pines comunes
    # que tenía en la vieja, o si su red vieja aporta una minoría a la nueva. Con
    # esa regla, que un pin entre a GND no marca los otros 385 pines de GND.
    comunes = set(pa) & set(pb)
    oa = {n: pa[n] for n in comunes}           # red vieja de cada pin común
    ob = {n: pb[n] for n in comunes}           # red nueva
    tam_a, tam_b = defaultdict(int), defaultdict(int)
    cruce = defaultdict(int)
    for n in comunes:
        tam_a[oa[n]] += 1
        tam_b[ob[n]] += 1
        cruce[(oa[n], ob[n])] += 1
    movidos = defaultdict(list)
    for n in comunes:
        k = cruce[(oa[n], ob[n])]
        if not (k * 2 > tam_a[oa[n]] and k * 2 > tam_b[ob[n]]):
            movidos[(oa[n], ob[n])].append(n)
    reutil = {r for r in set(da) & set(db)
              if any(da[r][k] != db[r][k] for k in ('val', 'fp'))}
    total = sum(len(v) for v in movidos.values())
    print(f"\n== CONEXIONES QUE CAMBIARON: {total} pines en {len(movidos)} grupos ==")
    print("   (red vieja → red nueva, y los pines que se movieron)")
    print("   † = designador reutilizado: el componente cambió de valor/footprint, así que es otro")
    for (x, y), nodos in sorted(movidos.items(), key=lambda kv: (-len(kv[1]), kv[0])):
        marca = lambda n: '†' if n[0] in reutil else ''
        print(f"  {corto(x)} → {corto(y)}   [{len(nodos)}]")
        print("      " + lista(nodos, fn).replace('  ', '  ') +
              ("     † " + ' '.join(sorted({n[0] for n in nodos if n[0] in reutil}, key=natural))
               if any(n[0] in reutil for n in nodos) else ''))

    # renombres puros: la mayoría mutua es la misma red, pero con otro nombre
    auto = re.compile(r'(^|/)(Net-\(|unconnected-\()')
    ren = []
    for (x, y), k in cruce.items():
        if x != y and k * 2 > tam_a[x] and k * 2 > tam_b[y] and not auto.search(x) and not auto.search(y):
            ren.append((x, y, k))
    print(f"\n== REDES RENOMBRADAS (no cambian la electricidad): {len(ren)} ==")
    for x, y, n in sorted(ren):
        print(f"  {corto(x)} → {corto(y)}   [{n} pines]")

    # pines al aire nuevos / resueltos
    def aire(nets):
        return {nm for nm, nodos in nets if len(nodos) == 1 and nm.startswith('unconnected-(')}
    nuevos_aire = sorted(aire(nb) - aire(na))
    resueltos = sorted(aire(na) - aire(nb))
    print(f"\n== PINES AL AIRE: {len(nuevos_aire)} nuevos · {len(resueltos)} resueltos ==")
    for x in nuevos_aire:
        print(f"  ⚠ nuevo  {x}")
    for x in resueltos:
        print(f"  ✓ ahora conectado  {x}")

    # redes de UN solo nodo que no son 'unconnected' (etiqueta que no llega a nada)
    sueltas = sorted(nm for nm, nodos in nb
                     if len(nodos) == 1 and not nm.startswith('unconnected-(')
                     and nm not in {n for n, o in na if len(o) == 1})
    print(f"\n== REDES NOMBRADAS QUE NO LLEGAN A NINGÚN LADO (un solo pin): {len(sueltas)} nuevas ==")
    for x in sueltas:
        print(f"  ⚠ {x}")


if __name__ == '__main__':
    main()
