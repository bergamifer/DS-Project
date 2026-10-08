"""Digitaliza la curva SS32~SS35 (Fig.3, hoja MDD SS34 p.2) desde el recorte ss34_fig3.png.
Calibración por píxeles: grilla horizontal 1,0 A en y=311, década = 191 px (líneas 2..9 A verificadas);
eje x: 0 V en x=80, 1,8 V en x=735.
Autoprueba: la línea de 2 A tiene que caer en y≈254 (se mide la grilla) y la curva tiene que ser monótona.
Prueba negativa: buscar la curva a 50 A (fuera del gráfico) tiene que fallar (devolver None)."""
import math, sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('L'); px = im.load(); W, H = im.size
x0, x18 = 80, 735
def V(x): return (x - x0) / (x18 - x0) * 1.8
def y_of(I): return 311 - 191 * math.log10(I)
def is_grid_row(y):  # una fila de grilla horizontal es oscura casi de punta a punta
    return sum(1 for x in range(160, 196) if px[x, y] < 170) > 30
def vf(I):
    y = round(y_of(I))
    if y < 25 or y > 500: return None
    for yy in (y, y - 2, y + 2, y - 3, y + 3):  # esquivar filas de grilla
        if not is_grid_row(yy): break
    # el trazo de la curva tiene ~6 px; las líneas de grilla, 2 px: tomar la primera corrida de >= 4 px
    run = []
    for x in range(150, 340):
        if px[x, yy] < 140:
            run.append(x)
        else:
            if len(run) >= 4: break
            run = []
    if len(run) < 4: return None
    # si la corrida incluye una línea de grilla pegada, el centro se corre <=1 px (0,003 V)
    return V((run[0] + run[-1]) / 2)
# autoprueba
assert abs(y_of(2) - 254) < 2, y_of(2)
assert vf(50) is None, 'prueba negativa: 50 A está fuera del gráfico'
pts = [(I, vf(I)) for I in (0.12, 0.15, 0.2, 0.3, 0.4, 0.5, 0.7, 0.9, 1.0, 1.3, 2.0, 3.0)]
vals = [v for _, v in pts]
assert all(b >= a - 0.005 for a, b in zip(vals, vals[1:])), 'no monótona'
for I, v in pts: print(f'{I:5.2f} A  ->  {v:.3f} V')
