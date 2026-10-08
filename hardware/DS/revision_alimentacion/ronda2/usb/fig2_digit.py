#!/usr/bin/env python3
"""Digitaliza la Fig. 2 del SS34 (MDD, Rev 2024A5, p.2: «Typical Reverse Characteristics»)
desde el render a 300 dpi `.ds12_audit/hojas/ss34_p2-2.png`.

Qué hace:
  1. Calibra los ejes buscando las líneas de la grilla en la imagen (no con números a ojo):
     - eje X: las 6 verticales de 0, 20, 40, 60, 80 y 100 % de la tensión inversa nominal (40 V);
     - eje Y: las 5 horizontales gruesas de 10^4 … 10^0 µA (escala logarítmica).
  2. Busca los píxeles del color de las curvas SÓLIDAS (SS32~SS36, gris 106) y los separa en tres
     bandas: 100 °C, 75 °C y 25 °C. Excluye el recuadro de la leyenda.
  3. Promedia el centro del trazo por columna y guarda (% VR, µA) cada 1 % en fig2_ss34.json.

Autoprueba: la grilla tiene que ser regular (±2 px), y los puntos tienen que coincidir con la lectura
a mano que hizo A2 (a2_drenaje.py) dentro de ±25 %. Prueba negativa: con el color de las curvas
PUNTEADAS (SS38~SS3200, gris 62) la fuga a 25 °C tiene que dar MENOR que con las sólidas, como dice la
leyenda; si diera igual o mayor, el filtro de color no está separando las curvas.

Uso: python3 -I fig2_digit.py   (escribe fig2_ss34.json al lado)
"""
import json, math, os, sys
from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
PNG = os.path.join(AQUI, "..", "..", "hojas", "ss34_p2-2.png")
SOLIDA, PUNTEADA = (106, 106, 106), (62, 62, 62)
ZONA = (1500, 650, 2185, 1135)          # recorte que contiene la Fig. 2 (px de la imagen a 300 dpi)
LEYENDA = (1860, 925, 2175, 990)        # recuadro de la leyenda: se excluye

def _oscuro(c, th): return max(c) < th

def calibrar(px):
    x0, y0, x1, y1 = ZONA
    cols = [x for x in range(x0, x1) if sum(1 for y in range(660, 1125) if _oscuro(px[x, y], 120)) > 250]
    grupos = []
    for x in cols:
        if grupos and x - grupos[-1][-1] <= 1: grupos[-1].append(x)
        else: grupos.append([x])
    xs = [sum(g) / len(g) for g in grupos]
    filas = [y for y in range(y0, y1) if sum(1 for x in range(x0, x1) if _oscuro(px[x, y], 60)) > 500]
    gy = []
    for y in filas:
        if gy and y - gy[-1][-1] <= 1: gy[-1].append(y)
        else: gy.append([y])
    ys = [sum(g) / len(g) for g in gy]
    return xs, ys

def curvas(px, color, tol=6):
    xs, ys = calibrar(px)
    xp0, xp100 = xs[0], xs[-1]
    ytop, ybot = ys[0], ys[-1]
    dec = (ybot - ytop) / 4.0
    out = {100: {}, 75: {}, 25: {}}
    ultimo = {100: 800.0, 75: 900.0, 25: 1105.0}   # semilla: dónde está cada curva a la izquierda
    for x in range(int(xp0) + 3, int(xp100) - 2):
        pct = 100.0 * (x - xp0) / (xp100 - xp0)
        yy = [y for y in range(int(ytop) + 3, int(ybot) - 2)
              if all(abs(px[x, y][k] - color[k]) <= tol for k in range(3))
              and not (LEYENDA[0] <= x <= LEYENDA[2] and LEYENDA[1] <= y <= LEYENDA[3])]
        # tramos contiguos (un trazo mide ~6 px)
        tramos = []
        for y in yy:
            if tramos and y - tramos[-1][-1] <= 1: tramos[-1].append(y)
            else: tramos.append([y])
        centros = [sum(t) / len(t) for t in tramos if len(t) >= 3]
        # cada curva toma el tramo más cercano a su posición anterior (seguimiento), si está a < 25 px
        for T in (100, 75, 25):
            if not centros: break
            c = min(centros, key=lambda v: abs(v - ultimo[T]))
            if abs(c - ultimo[T]) < 25:
                ultimo[T] = c
                centros.remove(c)
                out[T].setdefault(round(pct), []).append(4.0 - (c - ytop) / dec)
    # promedio por % entero → µA
    return {T: sorted((p, 10 ** (sum(v) / len(v))) for p, v in d.items()) for T, d in out.items()}, (xs, ys)

def autoprueba(px, sol, cal):
    ok = True
    xs, ys = cal
    paso_x = [b - a for a, b in zip(xs, xs[1:])]
    paso_y = [b - a for a, b in zip(ys, ys[1:])]
    ok &= len(xs) == 6 and max(paso_x) - min(paso_x) < 2.0
    ok &= len(ys) == 5 and max(paso_y) - min(paso_y) < 2.0
    # contra la lectura a mano de A2 (a2_drenaje.py, FIG2): ±25 %
    a2 = {25: [(20, 1.45), (40, 1.6), (60, 1.9), (80, 2.4)],
          75: [(20, 85), (40, 100), (60, 120), (80, 165)],
          100: [(20, 550), (40, 650), (60, 850), (80, 1200)]}
    for T, pts in a2.items():
        d = dict(sol[T])
        for p, ua in pts:
            cerca = min(d, key=lambda q: abs(q - p))
            ok &= abs(cerca - p) <= 2 and abs(d[cerca] / ua - 1) < 0.25
    # prueba negativa: las punteadas (SS38~) tienen que dar menos fuga a 25 °C que las sólidas
    pun, _ = curvas(px, PUNTEADA)
    dp, ds = dict(pun[25]), dict(sol[25])
    comunes = [p for p in ds if p in dp and 10 <= p <= 90]
    ok &= len(comunes) > 20 and all(dp[p] < ds[p] for p in comunes)
    print("autoprueba fig2:", "OK" if ok else "FALLA")
    return ok

if __name__ == "__main__":
    px = Image.open(PNG).convert("RGB").load()
    sol, cal = curvas(px, SOLIDA)
    assert autoprueba(px, sol, cal)
    print("grilla X (px):", [round(x, 1) for x in cal[0]], " Y (px):", [round(y, 1) for y in cal[1]])
    json.dump({str(T): v for T, v in sol.items()}, open(os.path.join(AQUI, "fig2_ss34.json"), "w"), indent=0)
    print("\n % VR |  25 °C  |  75 °C  | 100 °C   (µA, curva sólida SS32~SS36)")
    for p in (3, 5, 10, 12, 15, 20, 30, 40, 60, 80, 95, 99):
        fila = []
        for T in (25, 75, 100):
            d = dict(sol[T]); q = min(d, key=lambda k: abs(k - p))
            fila.append(f"{d[q]:7.2f}" if abs(q - p) <= 2 else "   —   ")
        print(f" {p:4d} | {fila[0]} | {fila[1]} | {fila[2]}")
