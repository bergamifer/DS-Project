#!/usr/bin/env python3
"""geometria_array.py — dónde va cada parlante, sacado del firmware y no de un papel.

POR QUÉ EXISTE
--------------
El beamforming supone que cada cono está en un lugar exacto. Si la madera y el
firmware no coinciden, **no hay ningún error**: el haz se forma igual, pero
apunta a otro lado, y el síntoma es "suena raro" — que es el peor de todos.

Este script no tiene las medidas escritas adentro: **las lee de
`firmware/Core/Src/main.c`**, de los mismos `#define` que usa el STM32 para
calcular los retardos. Si mañana cambiás el paso en el firmware, el DXF sale
distinto solo. Si lo cambiás en la madera, no — por eso conviene cortar desde
acá.

QUÉ IMPRIME
  - la tabla de las 32 posiciones en mm
  - un mapa en ASCII para ver la traba (el entrelazado) de un vistazo
  - el vano, la diagonal y el retardo que hace falta, contra el que hay
    compilado: si no alcanza, sale con código 1

QUÉ EXPORTA
  --dxf archivo.dxf   círculos en cada centro + marco del panel + el número de
                      cada posición, en capas separadas, en milímetros.

USO
  tools/geometria_array.py                          # lo que dice main.c hoy
  tools/geometria_array.py --layout 2x16            # forzar la geometría nueva
  tools/geometria_array.py --layout 2x16 --dxf /tmp/panel.dxf --diametro 66
  tools/geometria_array.py --layout 2x16 --dxf p.dxf --marco 30 --invertir-y

⚠️ SOBRE EL DIÁMETRO: el default (66 mm) es un valor de ejemplo, NO una medida
de tu parlante. Medí el tuyo y pasalo con --diametro; el script lo imprime
siempre para que no se cuele sin querer.
"""
import argparse
import math
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAIN_C = os.path.join(RAIZ, 'firmware', 'Core', 'Src', 'main.c')


# --------------------------------------------------------------------------
# 1. leer las constantes del firmware
# --------------------------------------------------------------------------
def defines(ruta):
    """Los #define enteros que definen la geometría, tal como los ve el compilador."""
    if not os.path.exists(ruta):
        sys.exit(f"no encuentro {ruta}")
    src = open(ruta, encoding='utf-8', errors='replace').read()
    d = {}
    for nombre in ('SPK_LAYOUT_2X16', 'SPK_X_PITCH_MM_I', 'SPK_Y_PITCH_MM_I',
                   'SPK_ROW_DX_MM_I', 'TX_MAX_DELAY_SAMPLES', 'SPK_PAIR_SWAP'):
        m = re.search(rf'^#define\s+{nombre}\s+(-?\d+)', src, re.M)
        if not m:
            sys.exit(f"no encuentro #define {nombre} en {ruta}")
        d[nombre] = int(m.group(1))
    return d


def posiciones(filas, cols, px, py, dx):
    """Idéntico a Beam_InitTxGeometry(): r = pos/COLS, c = pos%COLS, filas impares
    corridas dx en X. Devuelve [(pos, fila, col, x, y)] en orden de posición."""
    out = []
    for pos in range(filas * cols):
        r, c = divmod(pos, cols)
        out.append((pos, r, c, c * px + (dx if r % 2 else 0), r * py))
    return out


# --------------------------------------------------------------------------
# 2. DXF R12 escrito a mano (sin dependencias: esto tiene que correr en
#    cualquier máquina, también en una que no tenga pip)
# --------------------------------------------------------------------------
def _p(g, v):
    return f"{g}\n{v}\n"


CAPAS = [("PANEL", 7), ("PARLANTES", 1), ("CENTROS", 3), ("TEXTO", 5)]


def dxf(pts, diam, marco, texto=True, invertir_y=False, alto_texto=8.0):
    r = diam / 2.0
    ys = [y for *_, y in pts]
    if invertir_y:
        ymax = max(ys)
        pts = [(p, f, c, x, ymax - y) for p, f, c, x, y in pts]
    xs = [x for *_, x, _ in pts]
    ys = [y for *_, y in pts]
    x0, x1 = min(xs) - marco, max(xs) + marco
    y0, y1 = min(ys) - marco, max(ys) + marco

    s = _p(0, "SECTION") + _p(2, "HEADER")
    s += _p(9, "$ACADVER") + _p(1, "AC1009")
    s += _p(9, "$INSUNITS") + _p(70, 4)          # 4 = milímetros
    s += _p(9, "$EXTMIN") + _p(10, f"{x0:.4f}") + _p(20, f"{y0:.4f}") + _p(30, "0.0")
    s += _p(9, "$EXTMAX") + _p(10, f"{x1:.4f}") + _p(20, f"{y1:.4f}") + _p(30, "0.0")
    s += _p(0, "ENDSEC")

    s += _p(0, "SECTION") + _p(2, "TABLES")
    s += _p(0, "TABLE") + _p(2, "LTYPE") + _p(70, 1)
    s += (_p(0, "LTYPE") + _p(2, "CONTINUOUS") + _p(70, 0)
          + _p(3, "Solid line") + _p(72, 65) + _p(73, 0) + _p(40, "0.0"))
    s += _p(0, "ENDTAB")
    s += _p(0, "TABLE") + _p(2, "LAYER") + _p(70, len(CAPAS))
    for nombre, color in CAPAS:
        s += (_p(0, "LAYER") + _p(2, nombre) + _p(70, 0)
              + _p(62, color) + _p(6, "CONTINUOUS"))
    s += _p(0, "ENDTAB") + _p(0, "ENDSEC")

    s += _p(0, "SECTION") + _p(2, "ENTITIES")

    def linea(capa, xa, ya, xb, yb):
        return (_p(0, "LINE") + _p(8, capa)
                + _p(10, f"{xa:.4f}") + _p(20, f"{ya:.4f}") + _p(30, "0.0")
                + _p(11, f"{xb:.4f}") + _p(21, f"{yb:.4f}") + _p(31, "0.0"))

    # marco del panel: cuatro líneas, que es lo que entiende cualquier CAM viejo
    s += linea("PANEL", x0, y0, x1, y0) + linea("PANEL", x1, y0, x1, y1)
    s += linea("PANEL", x1, y1, x0, y1) + linea("PANEL", x0, y1, x0, y0)

    for pos, fila, col, x, y in pts:
        s += (_p(0, "CIRCLE") + _p(8, "PARLANTES")
              + _p(10, f"{x:.4f}") + _p(20, f"{y:.4f}") + _p(30, "0.0")
              + _p(40, f"{r:.4f}"))
        m = min(6.0, r / 2)                       # cruz de centrado
        s += linea("CENTROS", x - m, y, x + m, y) + linea("CENTROS", x, y - m, x, y + m)
        if texto:
            s += (_p(0, "TEXT") + _p(8, "TEXTO")
                  + _p(10, f"{x + r * 0.15:.4f}") + _p(20, f"{y + r * 0.35:.4f}")
                  + _p(30, "0.0") + _p(40, f"{alto_texto:.4f}") + _p(1, str(pos)))

    s += _p(0, "ENDSEC") + _p(0, "EOF")
    return s


# --------------------------------------------------------------------------
# 3. salida por pantalla
# --------------------------------------------------------------------------
ANCHO_CELDA = 4   # "%3d" + el espacio que lo separa del siguiente


def mapa_ascii(pts, cols, px, dx):
    """Una fila por fila del array. La traba se dibuja a escala: si dx es medio
    paso, la sangría es media celda — así se ve si el entrelazado es el que
    se quiere y no sólo que 'está corrido'."""
    filas = {}
    for pos, r, c, x, y in pts:
        filas.setdefault(r, []).append((c, pos, x))
    out = []
    for r in sorted(filas):
        corrida = (dx if r % 2 else 0)
        sangria = ' ' * round(corrida / px * ANCHO_CELDA)
        out.append(f"  fila {r}  {sangria}" + ' '.join(f"{pos:>3}" for _, pos, _ in
                                                       sorted(filas[r])))
    return '\n'.join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--layout', choices=('auto', '8x4', '2x16'), default='auto',
                    help="auto = lo que dice SPK_LAYOUT_2X16 en main.c")
    ap.add_argument('--dxf', metavar='ARCHIVO', help="exportar el panel en DXF (mm)")
    ap.add_argument('--diametro', type=float, default=66.0,
                    help="diámetro del agujero del parlante en mm (default 66, "
                         "que es un EJEMPLO: medí el tuyo)")
    ap.add_argument('--marco', type=float, default=40.0,
                    help="margen del panel alrededor de los centros, en mm")
    ap.add_argument('--sin-texto', action='store_true',
                    help="no numerar las posiciones en el DXF")
    ap.add_argument('--invertir-y', action='store_true',
                    help="fila 0 abajo (Y hacia arriba, como muchos CAM)")
    ap.add_argument('--main', default=MAIN_C, help="ruta a main.c")
    a = ap.parse_args()

    d = defines(a.main)
    if a.layout == 'auto':
        dos_x_16 = bool(d['SPK_LAYOUT_2X16'])
    else:
        dos_x_16 = a.layout == '2x16'
    filas, cols = (2, 16) if dos_x_16 else (4, 8)
    px, py, dx = d['SPK_X_PITCH_MM_I'], d['SPK_Y_PITCH_MM_I'], d['SPK_ROW_DX_MM_I']

    pts = posiciones(filas, cols, px, py, dx)
    span_x = (cols - 1) * px + dx
    span_y = (filas - 1) * py
    diag = math.hypot(span_x, span_y)
    muestras = diag * 48000.0 / 343000.0

    print(f"GEOMETRÍA {filas}×{cols}   (leída de {os.path.relpath(a.main, RAIZ)}"
          f"{'' if a.layout == 'auto' else ', forzada por --layout'})")
    print(f"  paso X {px} mm · paso Y {py} mm · traba de las filas impares {dx} mm")
    print(f"  vano {span_x} × {span_y} mm · diagonal {diag:.0f} mm")
    print(f"  retardo necesario {muestras:.0f} muestras a 48 kHz · "
          f"compilado TX_MAX_DELAY_SAMPLES = {d['TX_MAX_DELAY_SAMPLES']}")
    if a.layout != 'auto' and bool(d['SPK_LAYOUT_2X16']) != dos_x_16:
        print(f"  ⚠️ main.c tiene SPK_LAYOUT_2X16 = {d['SPK_LAYOUT_2X16']}: "
              f"esta geometría NO es la que está compilada hoy.")
    print()

    print(mapa_ascii(pts, cols, px, dx))
    print()

    print("  pos  fila  col      X(mm)    Y(mm)")
    for pos, r, c, x, y in pts:
        print(f"  {pos:>3}  {r:>4}  {c:>3}   {x:>8.1f} {y:>8.1f}")
    print()

    if d['SPK_PAIR_SWAP']:
        print("⚠️ SPK_PAIR_SWAP = 1: el arnés está cruzado de a pares, así que el cono")
        print("   de la posición p lo mueve el CANAL p^1 (0↔1, 2↔3, …). Los números de")
        print("   arriba y los del DXF son POSICIONES FÍSICAS, no canales.")
        print("   Si en la DS12 el arnés queda derecho, poné SPK_PAIR_SWAP en 0 y")
        print("   entonces posición = canal.")
        print()

    # ¿entran los agujeros? La distancia mínima entre centros no es el paso en X:
    # con las filas trabadas, el vecino más cercano suele ser el de la otra fila.
    dmin, par = None, None
    for i, (pi, _, _, xi, yi) in enumerate(pts):
        for pj, _, _, xj, yj in pts[i + 1:]:
            dd = math.hypot(xi - xj, yi - yj)
            if dmin is None or dd < dmin:
                dmin, par = dd, (pi, pj)
    material = dmin - a.diametro
    print(f"Centros más cercanos: {dmin:.1f} mm (posiciones {par[0]} y {par[1]}).")
    if a.diametro >= dmin:
        print(f"❌ Con ⌀{a.diametro:.1f} mm los agujeros SE SUPERPONEN.")
    elif material < 10:
        print(f"⚠️ Con ⌀{a.diametro:.1f} mm quedan {material:.1f} mm de madera entre "
              f"agujero y agujero: poco para una fresa y para el tornillo.")
    else:
        print(f"   Con ⌀{a.diametro:.1f} mm quedan {material:.1f} mm de madera entre "
              f"agujeros.")
    print()

    if a.dxf:
        s = dxf(pts, a.diametro, a.marco, texto=not a.sin_texto,
                invertir_y=a.invertir_y)
        open(a.dxf, 'w', encoding='utf-8').write(s)
        ancho, alto = span_x + 2 * a.marco, span_y + 2 * a.marco
        print(f"✅ {a.dxf}")
        print(f"   panel {ancho:.0f} × {alto:.0f} mm · {len(pts)} agujeros de "
              f"⌀{a.diametro:.1f} mm  ← verificá ESTE diámetro contra tu parlante")
        print(f"   capas: PANEL (contorno) · PARLANTES (a cortar) · CENTROS (cruces) "
              f"· TEXTO (números)")
        print(f"   unidades: milímetros ($INSUNITS = 4). Fila 0 "
              f"{'abajo' if a.invertir_y else 'arriba (Y hacia abajo, como el firmware)'}")

    if muestras > d['TX_MAX_DELAY_SAMPLES']:
        print()
        print(f"❌ El retardo compilado NO alcanza: hacen falta {muestras:.0f} muestras "
              f"y hay {d['TX_MAX_DELAY_SAMPLES']}.")
        print("   Beam_SetTxFocus recorta en silencio y el foco sale mal en los bordes.")
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
