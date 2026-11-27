"""Reruteo de los LEDs de J9 CONSERVANDO el LED 2. Trabaja sobre una COPIA del PCB.

    PY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3
    $PY intento_j9.py entrada.kicad_pcb salida.kicad_pcb

QUE ARREGLA (DRC con rellenos recalculados sobre el PCB del commit b31521d):
  5 cortos, 2 de separacion y 11 puentes de mascara, todos en la fila de pads de LED de J9.
  Vienen de que los pads pasaron a ovalo de 1,15 x 1,8 mm y las pistas viejas, trazadas para
  la huella anterior, rozan los ovalos vecinos.

COMO. Borra las pistas y vias de las ocho redes Net-(J9-PadN) y la parte de PE13 que corre
junto a la fila, y las traza de nuevo en tres carriles sobre B.Cu:
  - PE13 por arriba, a y = FILA_Y - 2,2 mm (un solo bus recto, que entra por (77,506 ; 127,172)), con una bajada a cada R;
  - los desvios del LED 1 (R111/R113/R112 quedan a la derecha del GND vecino del pad al que
    van): pasan POR ARRIBA del pad de GND, en un carril a y = FILA_Y - 1,27 mm;
  - R114 va directo, sin pasar a F.Cu.
R164, R165 y R167 se giran 180 grados para que su pata de PE13 quede arriba (del lado del
bus) y la del LED abajo, junto a su pad. R166 ya estaba asi.

NO TOCA NADA MAS. No hace falta que la salida sea buena para que el script sea util: el
criterio es el DRC, que se corre aparte.
"""
import sys
import pcbnew

src, dst = sys.argv[1], sys.argv[2]
b = pcbnew.LoadBoard(src)
mm, M, V = pcbnew.ToMM, pcbnew.FromMM, pcbnew.VECTOR2I
J9 = b.FindFootprintByReference("J9")

def pad(fp, n):
    return [p for p in fp.Pads() if p.GetNumber() == n][0]

def xy(p):
    q = p.GetPosition()
    return (mm(q.x), mm(q.y))

FILA_Y = xy(pad(J9, "9"))[1]
Y_PE13 = FILA_Y - 2.2
Y_DESVIO = FILA_Y - 1.27
W = M(0.2)

LED1 = {"R111": "9", "R113": "21", "R112": "33", "R114": "45"}   # resistencia -> pad del LED 1
LED2 = {"R164": "11", "R166": "23", "R165": "35", "R167": "47"}  # idem, LED 2
redes_led = {f"Net-(J9-Pad{n})" for n in list(LED1.values()) + list(LED2.values())}

# 1. borrar lo viejo
borrar = []
for t in b.GetTracks():
    n = t.GetNetname()
    if n in redes_led:
        borrar.append(t)
    elif n == "PE13" and not isinstance(t, pcbnew.PCB_VIA):
        if mm(t.GetStart().y) >= FILA_Y - 3.0 and mm(t.GetEnd().y) >= FILA_Y - 3.0:
            borrar.append(t)
for t in borrar:
    b.Remove(t)
print("pistas/vias borradas:", len(borrar))

# 2. R164, R165, R167: pata de PE13 hacia arriba
for ref in LED2:
    f = b.FindFootprintByReference(ref)
    p1 = [p for p in f.Pads() if p.GetNumber() == "1"][0]
    p2 = [p for p in f.Pads() if p.GetNumber() == "2"][0]
    if xy(p1)[1] > xy(p2)[1]:                 # la de PE13 esta abajo: girar
        f.SetOrientationDegrees((f.GetOrientationDegrees() + 180.0) % 360.0)

def netcode(nombre):
    return b.FindNet(nombre).GetNetCode()

def seg(net, *puntos):
    for (a, c) in zip(puntos, puntos[1:]):
        t = pcbnew.PCB_TRACK(b)
        t.SetStart(V(M(a[0]), M(a[1]))); t.SetEnd(V(M(c[0]), M(c[1])))
        t.SetWidth(W); t.SetLayer(pcbnew.B_Cu); t.SetNetCode(netcode(net))
        b.Add(t)

def pata(ref, n):
    return xy(pad(b.FindFootprintByReference(ref), n))

# 3. LED 1: R -> pad del LED 1
for ref, npad in LED1.items():
    red = f"Net-(J9-Pad{npad})"
    r2 = pata(ref, "2")
    px, py = xy(pad(J9, npad))
    if ref == "R114":                          # R114 esta debajo y va directo
        seg(red, r2, (px, r2[1] - (px - r2[0])), (px, py))
    else:                                      # por arriba del GND vecino
        seg(red, r2, (r2[0], Y_DESVIO), (px, Y_DESVIO), (px, py))

# 4. LED 2: R -> pad del LED 2, y la bajada de PE13
x_pe13 = []
for ref, npad in LED2.items():
    red = f"Net-(J9-Pad{npad})"
    px, py = xy(pad(J9, npad))
    pata_led = pata(ref, "2")
    seg(red, pata_led, (pata_led[0] + abs(py - pata_led[1]), py), (px, py))
    pe = pata(ref, "1")
    x_pe13.append(pe[0])
    seg("PE13", pe, (pe[0], Y_PE13))

# 5. el bus de PE13: entra por donde ya entraba, en (77,506 ; 127,172), y corre recto
bus = sorted(set(x_pe13))
seg("PE13", (77.506, 127.172), (77.506 + 0.7, Y_PE13), *[(x, Y_PE13) for x in bus])

pcbnew.SaveBoard(dst, b)
print("guardado:", dst)
