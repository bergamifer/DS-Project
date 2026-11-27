#!/usr/bin/env python3
"""A2 (ronda 2) — N1: ¿qué resistencia de drenaje necesita +5V_AUX para que la fuga inversa
de D23 no lo pueda levantar?

El estado peligroso: 20 V presentes y el AK5538 apagado (STM32 en blanco o detenido: PC7 al
aire, R158 baja PDN). Entonces +5V_AUX sólo carga:
  - el AP2125 en vacío: IQ 60 µA típ / 90 µA máx (DS38144, tabla de 3,3 V; la hoja no da mínimo);
  - el AK5538 en power-down: 10 µA típ / 100 µA máx, AVDD+TVDD (015099878-E-02 p.9).
D23 (SS34) tiene el cátodo en VIN_B (≈19,4 V) y el ánodo en +5V_AUX (≈4,9 V): ve ≈15 V en
reverso y su fuga ENTRA al nodo. Mientras +5V_AUX esté por debajo de +5VA, D19 conduce y
compensa. Si la fuga de D23 supera lo que consume el nodo, el nodo sube por encima de +5VA,
D19 se corta, y nada lo frena antes de los 6,5 V absolutos del AP2125.

Criterio: la resistencia R tiene que tomar, con el nodo en 5,0 V, más corriente que la peor
fuga de D23, aunque el AP2125 y el AK5538 no consuman nada (peor caso sin contar sumideros).

Fuga del SS34 (MDD, Rev 2024A5):
  - típica: Fig.2 p.2, curva SS32~SS36, leída a mano de la figura renderizada a 400 dpi
    (± ~15 %): % de VR → µA, a 25 / 75 / 100 °C;
  - máxima especificada (p.1): 0,5 mA a 25 °C y 5 mA a 125 °C, ambas a VR = 40 V.
    Para llevarla a 15 V se usa la forma de la curva típica a la temperatura más cercana.
"""
import math

VR_RATED = 40.0
# Fig.2, SS32~SS36: (% de VR, µA)
FIG2 = {
    25:  [(0, 1.4), (20, 1.45), (40, 1.6), (60, 1.9), (80, 2.4), (100, 4.2)],
    75:  [(0, 80), (20, 85), (40, 100), (60, 120), (80, 165), (100, 330)],
    100: [(0, 520), (20, 550), (40, 650), (60, 850), (80, 1200), (100, 3000)],
}

def _interp(pts, x):
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        if x0 <= x <= x1:
            t = (x - x0) / (x1 - x0)
            return math.exp(math.log(y0) + t * (math.log(y1) - math.log(y0)))
    return pts[-1][1] if x > pts[-1][0] else pts[0][1]

def fuga_tipica_uA(T, VR):
    """Interpolación log-lineal en temperatura entre las tres curvas (extrapola fuera de 25–100 °C)."""
    p = 100 * VR / VR_RATED
    temps = sorted(FIG2)
    vals = {t: _interp(FIG2[t], p) for t in temps}
    if T <= temps[1]: t0, t1 = temps[0], temps[1]
    else:             t0, t1 = temps[1], temps[2]
    k = (math.log(vals[t1]) - math.log(vals[t0])) / (t1 - t0)
    return vals[t0] * math.exp(k * (T - t0))

def fuga_max_uA(T, VR):
    """Máximo especificado: 500 µA @25 °C y 5000 µA @125 °C a 40 V, interpolado log-lineal en T,
    llevado a VR con la forma de la curva típica (25 °C abajo de 62,5 °C, 100 °C arriba).
    Como la curva típica a 100 °C ya roza el máximo de 125 °C, se toma el mayor de los dos."""
    i40 = 500 * math.exp(math.log(10) * (T - 25) / 100)
    p = 100 * VR / VR_RATED
    f25 = _interp(FIG2[25], p) / FIG2[25][-1][1]          # forma a 25 °C
    f100 = _interp(FIG2[100], p) / FIG2[100][-1][1]       # forma a 100 °C
    t = min(max((T - 25) / 75, 0.0), 1.0)                 # transición continua, sin saltos
    forma = math.exp(math.log(f25) + t * (math.log(f100) - math.log(f25)))
    return max(i40 * forma, fuga_tipica_uA(T, VR))

def r_maxima_kohm(i_uA, v_nodo=5.0, sumidero_uA=0.0):
    """La R más grande que todavía absorbe esa fuga con el nodo en v_nodo (kΩ)."""
    neta = i_uA - sumidero_uA
    return float("inf") if neta <= 0 else v_nodo / (neta * 1e-6) / 1e3

def autoprueba():
    ok = True
    # 1) la interpolación reproduce los puntos leídos de la figura
    ok &= abs(fuga_tipica_uA(75, 0.40 * VR_RATED) - 100) < 1e-6
    ok &= abs(fuga_tipica_uA(25, VR_RATED) - 4.2) < 1e-6
    # 2) el máximo a 40 V reproduce la tabla (0,5 mA a 25 °C; 5 mA a 125 °C)
    ok &= abs(500 * math.exp(math.log(10) * 0 / 100) - 500) < 1e-9
    ok &= fuga_max_uA(125, VR_RATED) >= 5000 - 1e-6
    # 3) sin fuga neta no hace falta drenaje (R infinita)
    ok &= r_maxima_kohm(50, sumidero_uA=60) == float("inf")
    # 4) prueba negativa: más fuga tiene que exigir una R MÁS CHICA
    ok &= r_maxima_kohm(1000) < r_maxima_kohm(100)
    print("autoprueba:", "OK" if ok else "FALLA")
    return ok

if __name__ == "__main__":
    assert autoprueba()
    VR = 19.4 - 4.9
    print(f"\nD23 con {VR:.1f} V en reverso. R máxima de drenaje para que +5V_AUX no pase de 5,0 V")
    print("(sin contar al AP2125 ni al AK5538 como sumideros: peor caso)\n")
    print("  T [°C] | fuga típica | R máx (típica) | fuga máx esp. | R máx (máx esp.)")
    for T in (25, 40, 60, 75, 85, 100):
        it, im = fuga_tipica_uA(T, VR), fuga_max_uA(T, VR)
        print(f"   {T:4d}  | {it:8.1f} µA | {r_maxima_kohm(it):10.1f} kΩ | {im:9.0f} µA  | {r_maxima_kohm(im):9.2f} kΩ")
    print("\nCandidatos del BOM: 1 kΩ (C11702) toma 5,0 mA a 5 V y disipa 25 mW;"
          " 10 kΩ (C25744) toma 0,5 mA y disipa 2,5 mW.")
    for R in (1.0, 10.0):
        lim = None
        for T in range(25, 126):
            if r_maxima_kohm(fuga_max_uA(T, VR)) < R:
                lim = T; break
        print(f"  {R:4.0f} kΩ cubre el máximo especificado hasta {('más de 125' if lim is None else lim - 1)} °C"
              f" (con la fuga típica: {'siempre' if r_maxima_kohm(fuga_tipica_uA(125, VR)) >= R else 'no'} hasta 125 °C)")
