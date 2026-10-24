#!/usr/bin/env python3
"""check_layout.py — las reglas de layout de la DS12 que el DRC no mira.

POR QUÉ EXISTE
--------------
El DRC de KiCad contesta "¿se tocan cosas que no deberían tocarse?". No contesta
"¿este buck va a irradiar?" ni "¿hay cobre bajo la antena?": una placa puede
tener el DRC en cero y aun así tener el lazo caliente de un buck dando la vuelta
por el otro lado de la placa. Esas reglas están escritas en
hardware/DS/DS12_ALIMENTACION.md §7, y este script las mide.

QUÉ MIDE (cada sección dice de qué regla sale)
  1. Lazo caliente de cada buck (§7.1, §7.2): los capacitores cerámicos de
     entrada: uno pegado a VIN y GND (el C1 de la Figura 47 de la hoja) y dos de
     ≥ 1 µF cerca, todos en la misma cara que el integrado.
  2. Nodo SW (§7.1): ¿el inductor está pegado al pin SW y en la misma cara?
     ¿hay vías en la red SW?
  3. Vías térmicas (§7.5): ¿hay vías bajo el pad de GND del AP63356? ¿y ninguna
     en el pad de SW?
  4. Realimentación (§7.3): distancia del cobre de FB al cobre de SW.
  5. Separación (§7.6): distancia de cada buck al AK5538 y a los PCM1690.
  6. Puntos de prueba (§7.8): un TP por riel, y que el TP tenga red (en la
     DS11, seis de siete TP no estaban conectados a nada).
  7. Planos (§7.7): +5VA y +3V3 como zonas, no como pistas.
  8. Antena del ESP32: cero cobre, en ninguna capa, dentro del keepout que trae
     la huella del módulo, y el keepout contra el borde de la placa.
  9. Desacople: para cada pin de alimentación de cada integrado, a qué
     distancia está el capacitor más cercano.

CÓMO MIDE
  Carga una COPIA del .kicad_pcb (el original no se toca), recalcula los
  rellenos de zona en memoria —porque el cobre guardado puede estar viejo, ver
  check_pcb.sh— y mide distancias BORDE A BORDE entre pads, pistas y vías. Los
  pads se aproximan por su rectángulo envolvente: a 90° es exacto, girados en
  otro ángulo da de menos (la distancia real es igual o mayor).

  Los componentes se buscan por su VALOR y por la FUNCIÓN de sus pines (VIN,
  SW, FB, GND), no por su designador: si mañana IC4 pasa a llamarse IC7, el
  script lo sigue encontrando. Los capacitores de entrada son los que tienen un
  pad en la red de VIN del integrado y el otro en GND.

LOS UMBRALES SON CRITERIO, NO FÍSICA
  "Pegado" = 2 mm borde a borde. Es lo que se ve en la Figura 47 de la hoja del
  AP63356, no un número que la hoja escriba. Están todos juntos más abajo
  (UMBRALES) para discutirlos en un solo lugar. Lo que sí está probado es que
  la herramienta detecta lo que dice detectar: ver --autoprueba.

USO
  tools/check_layout.py                          # hardware/DS/DS.kicad_pcb
  tools/check_layout.py otra/placa.kicad_pcb
  tools/check_layout.py --sin-relleno            # usa el cobre guardado (más rápido)
  tools/check_layout.py --autoprueba             # prueba la herramienta, no la placa

  Sale 1 si hay alguna ❌, 0 si no. Necesita el Python de KiCad (pcbnew); si se
  lo corre con otro Python, se relanza solo con el de KiCad.

AUTOPRUEBA
  Tres partes, en el espíritu de CLAUDE.md ("preguntarle qué haría si no
  hubiera nada que medir"):
    - la geometría y los veredictos, contra casos con respuesta conocida;
    - una placa VACÍA: no puede salir ningún ✅ (nada encontrado = ❌, no OK);
    - mutantes sobre la placa real: alejar los capacitores de entrada, alejar
      el inductor, sacar las vías térmicas, poner una vía en SW, pasar una
      pista bajo la antena, desconectar un TP, etc. Cada mutante tiene que
      cambiar la medición y el veredicto en la dirección esperada.

LO QUE NO MIDE (mirarlo a mano)
  - §7.4: que cada buck tenga su isla de GND unida al plano en un solo punto.
  - §7.3: que FB tome la muestra en el punto de carga y no en el inductor.
  - El camino de retorno por vías: mide distancias, no corrientes.
  - Que el layout de los filtros de salida esté lejos de los bucks (sólo mide
    contra el AK5538 y los PCM1690).
"""
import math
import os
import re
import shutil
import sys
import tempfile
from collections import namedtuple
from contextlib import contextmanager

KICAD_PY = ("/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/"
            "Versions/Current/bin/python3")


@contextmanager
def callado():
    """KiCad imprime un assert de wxWidgets ("create wxApp before…") que no
    significa nada fuera de la GUI. Se tapa stderr sólo mientras habla KiCad."""
    sys.stderr.flush()
    viejo = os.dup(2)
    nulo = os.open(os.devnull, os.O_WRONLY)
    os.dup2(nulo, 2)
    try:
        yield
    finally:
        sys.stderr.flush()
        os.dup2(viejo, 2)
        os.close(nulo)
        os.close(viejo)


try:
    with callado():
        import pcbnew
except ImportError:
    if os.path.exists(KICAD_PY) and os.path.realpath(sys.executable) != os.path.realpath(KICAD_PY):
        os.execv(KICAD_PY, [KICAD_PY, os.path.abspath(__file__)] + sys.argv[1:])
    sys.exit("check_layout: necesito el Python de KiCad (el módulo pcbnew).\n"
             f"  Probá: {KICAD_PY} {sys.argv[0]}")


# ---------------------------------------------------------------------------
# UMBRALES — todos en mm, borde a borde. Criterio, no física: ver el docstring.
# ---------------------------------------------------------------------------
CIN_OK = 2.0        # capacitor de entrada "pegado" a VIN y a GND
CIN_MAX = 5.0       # más que esto: el lazo caliente ya no es chico
SW_OK = 3.0         # pin SW → pad del inductor
SW_MAX = 8.0
FB_MIN = 1.0        # cobre de FB contra cobre de SW
SEP_ANALOG = 10.0   # buck → AK5538 / PCM1690
DESACOPLE_MAX = 3.0 # pin de alimentación → su capacitor más cercano

# Los bucks, por el valor del integrado. El criterio de entrada sale de la
# Figura 47 de la hoja del AP63356: UN capacitor (C1) pegado sobre los pads VIN y
# GND, y los demás sobre los mismos planos, cerca. Dos 1210 no pueden quedar los
# dos a 2 mm de pads de 0,8 mm: pedirlo era pedir algo que la hoja no dibuja.
# `min_cin`: cuántos cerámicos de ≥ 1 µF
# tienen que estar pegados (§7.2: dos como mínimo para el buck A).
BUCKS = [
    dict(valor="AP63356", nombre="buck A (+5VA, amplificadores)", min_cin=2, pad_termico=True),
    dict(valor="AP63301", nombre="buck B (+3V3, digital)", min_cin=2, pad_termico=False),
]
ANALOGICOS = ("AK5538", "PCM1690")
# Los rieles que tienen que tener punto de prueba (§7.8). Si una red no existe
# en la placa, se informa y no cuenta como falla (puede ser otra revisión).
RIELES = ["+20V", "+24V", "+VIN_LOCAL", "/DS_Power/IN", "+5VA", "+5VA_DAC", "+5V_AUX",
          "+3V3", "+3.3VA", "GND"]
PLANOS = ["+5VA", "+3V3"]
# Funciones de pin que son alimentación (para el desacople).
PIN_ALIM = re.compile(r"^(VDD|VCC|AVDD|DVDD|TVDD|PVDD|VREF|VBAT|3V3)\d*$")

OK, INFO, AVISO, FALLA = "ok", "info", "aviso", "falla"
ICONO = {OK: "✅", INFO: "  ", AVISO: "⚠️ ", FALLA: "❌"}
Resultado = namedtuple("Resultado", "seccion sujeto nivel texto valor")

mm = pcbnew.ToMM


def f1(x):
    """Un decimal, con coma."""
    return f"{x:.1f}".replace(".", ",")


# ---------------------------------------------------------------------------
# Geometría propia (en mm). Cada pieza de cobre es una "forma": segmentos con
# un radio (una pista es un segmento engordado; una vía, un punto engordado) y,
# si es un área (un pad), su polígono, para saber si algo cae adentro.
# ---------------------------------------------------------------------------
class Forma:
    def __init__(self, segs, r=0.0, poly=None, que="", capas=None):
        self.segs, self.r, self.poly, self.que = segs, r, poly, que
        self.capas = capas   # set de capas de cobre; None = no importa


def _dist_punto_seg(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    l2 = dx * dx + dy * dy
    t = 0.0 if l2 == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l2))
    return math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy)


def _se_cruzan(a, b, c, d):
    def o(p, q, r):
        v = (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
        return (v > 0) - (v < 0)
    return o(a, b, c) * o(a, b, d) < 0 and o(c, d, a) * o(c, d, b) < 0


def _dist_seg_seg(a, b, c, d):
    if _se_cruzan(a, b, c, d):
        return 0.0
    return min(_dist_punto_seg(a, c, d), _dist_punto_seg(b, c, d),
               _dist_punto_seg(c, a, b), _dist_punto_seg(d, a, b))


def _adentro(p, poly):
    dentro = False
    n = len(poly)
    for i in range(n):
        (x1, y1), (x2, y2) = poly[i], poly[(i + 1) % n]
        if (y1 > p[1]) != (y2 > p[1]):
            if p[0] < (x2 - x1) * (p[1] - y1) / (y2 - y1) + x1:
                dentro = not dentro
    return dentro


def poligono(pts):
    return Forma([(pts[i], pts[(i + 1) % len(pts)]) for i in range(len(pts))], 0.0, list(pts))


def rect(x0, y0, x1, y1):
    return poligono([(x0, y0), (x1, y0), (x1, y1), (x0, y1)])


def distancia(a, b):
    """Borde a borde. 0 si se tocan o una está dentro de la otra."""
    if a.poly and _adentro(b.segs[0][0], a.poly):
        return 0.0
    if b.poly and _adentro(a.segs[0][0], b.poly):
        return 0.0
    d = min(_dist_seg_seg(p, q, s, t) for p, q in a.segs for s, t in b.segs)
    return max(0.0, d - a.r - b.r)


def xy(v):
    return (mm(v.x), mm(v.y))


def forma_caja(caja):
    return rect(mm(caja.GetLeft()), mm(caja.GetTop()), mm(caja.GetRight()), mm(caja.GetBottom()))


def forma_pad(pad):
    f = forma_caja(pad.GetBoundingBox())
    f.que = f"pad {pad.GetParentFootprint().GetReference()}.{pad.GetNumber()}"
    f.capas = set(l for l in pad.GetLayerSet().CuStack())
    return f


def ancho_via(v):
    try:
        return mm(v.GetWidth(pcbnew.F_Cu))
    except TypeError:
        return mm(v.GetWidth())


def forma_pista(t):
    c = t.GetClass()
    if c == "PCB_VIA":
        p = xy(t.GetPosition())
        return Forma([(p, p)], ancho_via(t) / 2, que=f"vía en {f1(p[0])}·{f1(p[1])}",
                     capas=set(t.GetLayerSet().CuStack()))
    capa = t.GetLayerName()
    if c == "PCB_ARC":
        return Forma([(xy(t.GetStart()), xy(t.GetMid())), (xy(t.GetMid()), xy(t.GetEnd()))],
                     mm(t.GetWidth()) / 2, que=f"arco en {capa}", capas={t.GetLayer()})
    a, b = xy(t.GetStart()), xy(t.GetEnd())
    return Forma([(a, b)], mm(t.GetWidth()) / 2,
                 que=f"pista en {capa} ({f1(a[0])}·{f1(a[1])} → {f1(b[0])}·{f1(b[1])})",
                 capas={t.GetLayer()})


def forma_cadena(sps, i=0):
    """El contorno exterior i de un SHAPE_POLY_SET, como polígono."""
    c = sps.Outline(i)
    return poligono([xy(c.CPoint(k)) for k in range(c.PointCount())])


def dmin(formas_a, formas_b):
    return min((distancia(a, b) for a in formas_a for b in formas_b), default=None)


def comparten_capa(a, b):
    return a.capas is None or b.capas is None or bool(a.capas & b.capas)


def par_mas_cercano(formas_a, formas_b, misma_capa=False):
    """(distancia, índice en a, índice en b), o None si no hay pares que medir.
    Con misma_capa, sólo pares que comparten una capa de cobre: entre F.Cu y
    B.Cu está el plano de GND de In1, que las blinda."""
    return min(((distancia(a, b), i, j) for i, a in enumerate(formas_a) for j, b in enumerate(formas_b)
                if not misma_capa or comparten_capa(a, b)),
               default=None, key=lambda x: x[0])


# ---------------------------------------------------------------------------
# Veredictos: funciones puras, para poder probarlas con números conocidos.
# ---------------------------------------------------------------------------
def veredicto_cin(gap_vin, gap_gnd, misma_cara):
    if not misma_cara:
        return FALLA
    peor = max(gap_vin, gap_gnd)
    return OK if peor <= CIN_OK else AVISO if peor <= CIN_MAX else FALLA


def veredicto_cin_buck(hay_pegado, n_grandes_cerca, minimo):
    """hay_pegado: algún cerámico (de cualquier valor) a ≤ CIN_OK de VIN y GND,
    en la misma cara — el C1 de la Figura 47. n_grandes_cerca: cerámicos de
    ≥ 1 µF a ≤ CIN_MAX, misma cara (§7.2: la corriente RMS pide dos)."""
    if hay_pegado and n_grandes_cerca >= minimo:
        return OK
    return AVISO if hay_pegado or n_grandes_cerca > 0 else FALLA


def veredicto_sw(gap, misma_cara):
    if not misma_cara:
        return FALLA
    return OK if gap <= SW_OK else AVISO if gap <= SW_MAX else FALLA


def veredicto_minimo(d, minimo):
    return OK if d >= minimo else AVISO


# ---------------------------------------------------------------------------
# Ayudas sobre la placa
# ---------------------------------------------------------------------------
def funcion(pad):
    """'VIN_1' → 'VIN'. Los símbolos de la librería local le pegan el número."""
    f = pad.GetPinFunction().upper()
    return re.sub(r"_\d+$", "", f)


def pads_func(fp, *funcs):
    return [p for p in fp.Pads() if funcion(p) in funcs]


def por_valor(b, valor):
    return [f for f in b.GetFootprints() if f.GetValue().upper().startswith(valor.upper())]


def cara(fp):
    return "B" if fp.IsFlipped() else "F"


def es_electrolitico(fp):
    n = fp.GetFPIDAsString().split(":")[-1]
    return n.startswith("CP_") or "Elec" in n or "Tantal" in n


def faradios(valor):
    m = re.search(r"(\d+(?:[.,]\d+)?)\s*([pnuµμ])", valor)
    if not m:
        return None
    return float(m.group(1).replace(",", ".")) * {"p": 1e-12, "n": 1e-9, "u": 1e-6,
                                                   "µ": 1e-6, "μ": 1e-6}[m.group(2)]


def capacitores_entre(b, red_a, red_b):
    """Capacitores (ref C…, dos pads) con un pad en cada una de las dos redes."""
    out = []
    for f in b.GetFootprints():
        pads = list(f.Pads())
        if f.GetReference().startswith("C") and len(pads) == 2 \
                and {p.GetNetname() for p in pads} == {red_a, red_b}:
            out.append(f)
    return out


def pad_en_red(fp, red):
    return [p for p in fp.Pads() if p.GetNetname() == red]


class Placa:
    """La placa y lo que se calcula una sola vez sobre ella."""

    def __init__(self, b):
        self.b = b
        e = b.GetBoardEdgesBoundingBox()
        self.borde = None if e.GetWidth() == 0 else (mm(e.GetLeft()), mm(e.GetTop()),
                                                     mm(e.GetRight()), mm(e.GetBottom()))
        self.pistas = list(b.GetTracks())

    def ubicado(self, fp):
        """Los componentes recién importados caen fuera del contorno."""
        if self.borde is None:
            return True
        x, y = xy(fp.GetPosition())
        x0, y0, x1, y1 = self.borde
        return x0 <= x <= x1 and y0 <= y <= y1

    def cobre_de_red(self, red, excluir_fp=()):
        formas = [forma_pista(t) for t in self.pistas if t.GetNetname() == red]
        for f in self.b.GetFootprints():
            if f in excluir_fp:
                continue
            formas += [forma_pad(p) for p in pad_en_red(f, red)]
        return formas

    def vias(self):
        return [t for t in self.pistas if t.GetClass() == "PCB_VIA"]


# ---------------------------------------------------------------------------
# Las verificaciones. Cada una devuelve una lista de Resultado.
# ---------------------------------------------------------------------------
def chequear_bucks(P):
    R = []
    for cfg in BUCKS:
        ics = por_valor(P.b, cfg["valor"])
        if not ics:
            R.append(Resultado("cin", cfg["valor"], FALLA,
                               f"no encuentro ningún {cfg['valor']} ({cfg['nombre']}) en la placa", None))
            continue
        for ic in ics:
            R += chequear_un_buck(P, ic, cfg)
    return R


def chequear_un_buck(P, ic, cfg):
    R = []
    ref = ic.GetReference()
    nombre = f"{ref} {cfg['nombre']}"
    if not P.ubicado(ic):
        return [Resultado("cin", ref, AVISO, f"{nombre}: sin ubicar (fuera del contorno), no mido", None)]

    vin, sw, gnd, fb = (pads_func(ic, "VIN", "IN"), pads_func(ic, "SW"),
                        pads_func(ic, "GND"), pads_func(ic, "FB"))
    faltan = [n for n, l in (("VIN", vin), ("SW", sw), ("GND", gnd), ("FB", fb)) if not l]
    if faltan:
        return [Resultado("cin", ref, FALLA,
                          f"{nombre}: la huella no tiene pines con función {', '.join(faltan)} — "
                          "no puedo saber dónde está el lazo", None)]
    red_vin, red_sw, red_gnd, red_fb = (vin[0].GetNetname(), sw[0].GetNetname(),
                                        gnd[0].GetNetname(), fb[0].GetNetname())
    f_vin, f_sw, f_gnd = [forma_pad(p) for p in vin], [forma_pad(p) for p in sw], [forma_pad(p) for p in gnd]

    # 1. Lazo caliente: capacitores cerámicos de entrada.
    hay_pegado, n_grandes = False, 0
    cins = [c for c in capacitores_entre(P.b, red_vin, red_gnd) if not es_electrolitico(c)]
    lineas = []
    for c in sorted(cins, key=lambda c: c.GetReference()):
        cr = c.GetReference()
        if not P.ubicado(c):
            lineas.append(Resultado("cin", f"{ref}/{cr}", AVISO, f"{cr} {c.GetValue()}: sin ubicar", None))
            continue
        gv = dmin([forma_pad(p) for p in pad_en_red(c, red_vin)], f_vin)
        gg = dmin([forma_pad(p) for p in pad_en_red(c, red_gnd)], f_gnd)
        misma = c.IsFlipped() == ic.IsFlipped()
        nv = veredicto_cin(gv, gg, misma)
        grande = (faradios(c.GetValue()) or 0) >= 1e-6
        if grande:
            n_grandes += nv in (OK, AVISO)
        hay_pegado |= nv == OK
        texto = (f"{cr} {c.GetValue()}: VIN {f1(gv)} mm · GND {f1(gg)} mm · "
                 + ("misma cara" if misma else f"OTRA CARA ({cara(c)}, el integrado está en {cara(ic)}): "
                    "el lazo pasa por vías"))
        lineas.append(Resultado("cin", f"{ref}/{cr}", nv, texto, max(gv, gg)))
    nb = veredicto_cin_buck(hay_pegado, n_grandes, cfg["min_cin"])
    R.append(Resultado("cin", ref, nb,
                       f"{nombre}: {'uno' if hay_pegado else 'NINGUNO'} pegado a VIN y GND (≤ {f1(CIN_OK)} mm), "
                       f"{n_grandes} de ≥{cfg['min_cin']} de ≥1 µF cerca (≤ {f1(CIN_MAX)} mm) — misma cara",
                       n_grandes))
    R += lineas

    # 2. Nodo SW → inductor.
    inds = [f for f in P.b.GetFootprints() if f.GetReference().startswith("L")
            and len(list(f.Pads())) == 2 and pad_en_red(f, red_sw)]
    if not inds:
        R.append(Resultado("sw", ref, FALLA, f"{nombre}: no encuentro el inductor en la red {red_sw}", None))
    for L in inds:
        lr = L.GetReference()
        if not P.ubicado(L):
            R.append(Resultado("sw", f"{ref}/{lr}", AVISO, f"{lr}: sin ubicar", None))
            continue
        g = dmin([forma_pad(p) for p in pad_en_red(L, red_sw)], f_sw)
        misma = L.IsFlipped() == ic.IsFlipped()
        R.append(Resultado("sw", f"{ref}/{lr}", veredicto_sw(g, misma),
                           f"{ref}.SW → {lr}: {f1(g)} mm · "
                           + ("misma cara" if misma else f"OTRA CARA ({lr} en {cara(L)}, {ref} en {cara(ic)})"), g))
    vias_sw = [v for v in P.vias() if v.GetNetname() == red_sw]
    largo = sum(math.dist(xy(t.GetStart()), xy(t.GetEnd())) for t in P.pistas
                if t.GetNetname() == red_sw and t.GetClass() != "PCB_VIA")
    R.append(Resultado("sw", f"{ref}/vias", AVISO if vias_sw else OK,
                       f"red SW ({red_sw}): {len(vias_sw)} vías, {f1(largo)} mm de pista"
                       + (" — cada vía del nodo SW agranda la antena" if vias_sw else ""), len(vias_sw)))

    # 3. Vías térmicas (sólo el integrado con pad expuesto).
    if cfg["pad_termico"]:
        n_gnd = sum(1 for v in P.vias() if v.GetNetname() == red_gnd
                    and dmin([forma_pista(v)], f_gnd) == 0)
        n_vin = sum(1 for v in P.vias() if v.GetNetname() == red_vin
                    and dmin([forma_pista(v)], f_vin) == 0)
        R.append(Resultado("termico", f"{ref}/gnd", OK if n_gnd else FALLA,
                           f"{ref}: {n_gnd} vías en el pad de GND, {n_vin} en el de VIN"
                           + ("" if n_gnd else " — sin ellas los 25 °C/W de la hoja no existen"), n_gnd))
    n_en_sw = sum(1 for v in P.vias() if dmin([forma_pista(v)], f_sw) == 0)
    if n_en_sw:
        R.append(Resultado("termico", f"{ref}/sw", FALLA,
                           f"{ref}: {n_en_sw} vías DENTRO del pad SW — no va ninguna ahí", n_en_sw))

    # 4. FB lejos de SW. Sin los pads del propio integrado: sus pines están
    #    pegados por construcción y no se pueden alejar.
    cobre_fb = P.cobre_de_red(red_fb, excluir_fp=(ic,))
    cobre_sw = P.cobre_de_red(red_sw, excluir_fp=(ic,)) + f_sw
    par = par_mas_cercano(cobre_fb, cobre_sw, misma_capa=True)
    if par is None:
        R.append(Resultado("fb", ref, INFO, f"{ref}: FB ({red_fb}) sin cobre en una capa compartida con SW", None))
    else:
        d, i, j = par
        sin_pista = not any(t.GetNetname() == red_fb for t in P.pistas)
        R.append(Resultado("fb", ref, veredicto_minimo(d, FB_MIN),
                           f"{ref}: cobre de FB a {f1(d)} mm del cobre de SW en la misma capa"
                           + (" (FB sin rutear: mido sólo pads)" if sin_pista else ""), d))
        if d < FB_MIN:
            R.append(Resultado("fb", ref + "/par", INFO,
                               f"   FB: {cobre_fb[i].que}  ↔  SW: {cobre_sw[j].que}", None))

    # 5. Separación de lo analógico: el buck = integrado + inductor + Cin.
    grupo = [ic] + inds + cins
    f_grupo = [forma_pad(p) for f in grupo if P.ubicado(f) for p in f.Pads()]
    for v in ANALOGICOS:
        for a in por_valor(P.b, v):
            if not P.ubicado(a):
                continue
            d = dmin(f_grupo, [forma_pad(p) for p in a.Pads()])
            R.append(Resultado("separacion", f"{ref}/{a.GetReference()}", veredicto_minimo(d, SEP_ANALOG),
                               f"{ref} → {a.GetReference()} ({a.GetValue()}): {f1(d)} mm", d))
    return R


def chequear_tps(P):
    R = []
    tps = [f for f in P.b.GetFootprints() if f.GetReference().startswith("TP")]
    redes_placa = set(P.b.GetNetsByName().keys())
    redes_placa = {str(n) for n in redes_placa}
    con_tp = {}
    for t in tps:
        redes = {p.GetNetname() for p in t.Pads() if p.GetNetCode() > 0}
        if not redes:
            R.append(Resultado("tp", t.GetReference(), FALLA,
                               f"{t.GetReference()} no está conectado a ninguna red — no mide nada", None))
        elif not P.ubicado(t):
            R.append(Resultado("tp", t.GetReference(), AVISO, f"{t.GetReference()}: sin ubicar", None))
        for r in redes:
            con_tp.setdefault(r, []).append(t.GetReference())
    for riel in RIELES:
        if riel not in redes_placa:
            R.append(Resultado("tp", riel, INFO, f"{riel}: esa red no existe en esta placa", None))
        elif riel in con_tp:
            R.append(Resultado("tp", riel, OK, f"{riel}: {', '.join(sorted(con_tp[riel]))}", None))
        else:
            R.append(Resultado("tp", riel, AVISO, f"{riel}: sin punto de prueba", None))
    return R


def chequear_planos(P):
    R = []
    zonas = [z for z in P.b.Zones() if not z.GetIsRuleArea()]
    for red in PLANOS:
        capas = sorted({P.b.GetLayerName(l) for z in zonas if z.GetNetname() == red
                        for l in z.GetLayerSet().Seq()})
        R.append(Resultado("planos", red, OK if capas else AVISO,
                           f"{red}: " + (f"zona en {', '.join(capas)}" if capas else "sin zona (§7.7 pide plano)"),
                           len(capas)))
    return R


def chequear_antena(P):
    R = []
    mods = por_valor(P.b, "ESP32")
    if not mods:
        return [Resultado("antena", "ESP32", FALLA, "no encuentro el módulo ESP32", None)]
    for m in mods:
        ref = m.GetReference()
        kos = [z for z in m.Zones() if z.GetIsRuleArea()]
        if not kos:
            R.append(Resultado("antena", ref, FALLA,
                               f"{ref}: la huella no trae keepout de antena — revisar a mano", None))
            continue
        for ko in kos:
            R += chequear_un_keepout(P, m, ko)
    return R


def chequear_un_keepout(P, m, ko):
    R = []
    ref = m.GetReference()
    capas = list(ko.GetLayerSet().Seq())
    banderas = {"pistas": ko.GetDoNotAllowTracks(), "vías": ko.GetDoNotAllowVias(),
                "rellenos": ko.GetDoNotAllowZoneFills()}
    sin = [k for k, v in banderas.items() if not v]
    if sin:
        R.append(Resultado("antena", f"{ref}/reglas", AVISO,
                           f"{ref}: el keepout PERMITE {', '.join(sin)} — KiCad no lo va a impedir", None))
    contorno = ko.Outline()
    f_ko = forma_cadena(contorno)
    malos = []
    # Rellenos: intersección real de polígonos.
    for z in P.b.Zones():
        if z.GetIsRuleArea():
            continue
        for l in z.GetLayerSet().Seq():
            if l not in capas:
                continue
            x = pcbnew.SHAPE_POLY_SET(z.GetFilledPolysList(l))
            x.BooleanIntersection(contorno)
            a = x.Area() / 1e12
            if a > 0.01:
                malos.append(f"relleno {z.GetNetname()} en {P.b.GetLayerName(l)} ({f1(a)} mm²)")
    # Pistas y vías, de cualquier red (también sin red: cobre huérfano).
    for t in P.pistas:
        if not any(t.IsOnLayer(l) for l in capas):
            continue
        if distancia(forma_pista(t), f_ko) == 0:
            tipo = "vía" if t.GetClass() == "PCB_VIA" else "pista"
            capa = "pasante" if tipo == "vía" else P.b.GetLayerName(t.GetLayer())
            malos.append(f"{tipo} {t.GetNetname() or '(sin red)'} en {capa} en {f1(mm(t.GetPosition().x))}·"
                         f"{f1(mm(t.GetPosition().y))}")
    # Pads de otros componentes.
    for f in P.b.GetFootprints():
        if f is m:
            continue
        for p in f.Pads():
            if any(p.IsOnLayer(l) for l in capas) and distancia(forma_pad(p), f_ko) == 0:
                malos.append(f"pad {f.GetReference()}.{p.GetNumber()}")
    if malos:
        R.append(Resultado("antena", f"{ref}/cobre", FALLA,
                           f"{ref}: {len(malos)} cosas de cobre dentro del keepout de antena", len(malos)))
        R += [Resultado("antena", f"{ref}/cobre", INFO, "   " + x, None) for x in malos[:12]]
        if len(malos) > 12:
            R.append(Resultado("antena", f"{ref}/cobre", INFO, f"   … y {len(malos) - 12} más", None))
    else:
        R.append(Resultado("antena", f"{ref}/cobre", OK,
                           f"{ref}: cero cobre en el keepout ({', '.join(P.b.GetLayerName(l) for l in capas)})", 0))
    # ¿La antena está contra el borde? (cruza el contorno o lo toca)
    if P.borde:
        bb = ko.GetBoundingBox()
        x0, y0, x1, y1 = P.borde
        al_borde = (mm(bb.GetTop()) <= y0 or mm(bb.GetBottom()) >= y1
                    or mm(bb.GetLeft()) <= x0 or mm(bb.GetRight()) >= x1)
        R.append(Resultado("antena", f"{ref}/borde", OK if al_borde else AVISO,
                           f"{ref}: " + ("la antena llega al borde de la placa" if al_borde else
                                         "la antena NO llega al borde: queda rodeada de placa"), None))
    return R


def chequear_desacople(P):
    R = []
    excluir = tuple(c["valor"].upper() for c in BUCKS)
    caps_por_red = {}
    for f in P.b.GetFootprints():
        pads = list(f.Pads())
        if f.GetReference().startswith("C") and len(pads) == 2 and P.ubicado(f):
            redes = [p.GetNetname() for p in pads]
            if "GND" in redes:
                for p in pads:
                    if p.GetNetname() != "GND":
                        caps_por_red.setdefault(p.GetNetname(), []).append((f, p))
    for ic in sorted(P.b.GetFootprints(), key=lambda f: f.GetReference()):
        ref = ic.GetReference()
        if len(list(ic.Pads())) < 8 or ref.startswith(("J", "H", "TP", "RN")) \
                or ic.GetValue().upper().startswith(excluir) or not P.ubicado(ic):
            continue
        pines = [p for p in ic.Pads() if p.GetNetname().startswith("+") and PIN_ALIM.match(funcion(p))]
        if not pines:
            continue
        peores = []
        for p in pines:
            cands = caps_por_red.get(p.GetNetname(), [])
            if not cands:
                peores.append((None, p, None))
                continue
            d, c = min(((distancia(forma_pad(p), forma_pad(cp)), c) for c, cp in cands), key=lambda x: x[0])
            peores.append((d, p, c))
        lejos = [x for x in peores if x[0] is None or x[0] > DESACOPLE_MAX]
        peor = max((x for x in peores if x[0] is not None), key=lambda x: x[0], default=None)
        texto = f"{ref} {ic.GetValue()}: {len(pines)} pines de alimentación"
        if peor:
            texto += f", el más lejos {f1(peor[0])} mm ({funcion(peor[1])}, pin {peor[1].GetNumber()} → {peor[2].GetReference()})"
        R.append(Resultado("desacople", ref, AVISO if lejos else OK, texto, peor[0] if peor else None))
        for d, p, c in lejos:
            R.append(Resultado("desacople", ref, INFO,
                               f"   pin {p.GetNumber()} {funcion(p)} ({p.GetNetname()}): " +
                               (f"{f1(d)} mm hasta {c.GetReference()}" if c else "ningún capacitor a GND en esa red"),
                               d))
    return R


SECCIONES = [
    ("cin", "1. Lazo caliente — capacitores de entrada (§7.1, §7.2)"),
    ("sw", "2. Nodo SW — inductor y vías (§7.1)"),
    ("termico", "3. Vías térmicas (§7.5)"),
    ("fb", "4. Realimentación lejos de SW (§7.3)"),
    ("separacion", f"5. Bucks lejos de lo analógico (§7.6, umbral {f1(SEP_ANALOG)} mm)"),
    ("tp", "6. Puntos de prueba (§7.8)"),
    ("planos", "7. Rieles como planos (§7.7)"),
    ("antena", "8. Antena del ESP32"),
    ("desacople", f"9. Desacople (pin → capacitor más cercano, umbral {f1(DESACOPLE_MAX)} mm)"),
]


def chequear(P):
    return (chequear_bucks(P) + chequear_tps(P) + chequear_planos(P)
            + chequear_antena(P) + chequear_desacople(P))


def imprimir(R):
    for clave, titulo in SECCIONES:
        rs = [r for r in R if r.seccion == clave]
        print(f"\n{titulo}")
        if not rs:
            print("      (nada que medir)")
        if clave == "desacople":   # 40 integrados: los que están bien, en una línea
            buenos = [r.sujeto for r in rs if r.nivel == OK]
            if buenos:
                print(f"  ✅ {len(buenos)} integrados con todos sus pines a ≤ {f1(DESACOPLE_MAX)} mm "
                      f"(el peor de ellos: {f1(max(r.valor for r in rs if r.nivel == OK))} mm)")
            rs = [r for r in rs if r.nivel != OK]
        for r in rs:
            print(f"  {ICONO[r.nivel]} {r.texto}")
    nf = sum(r.nivel == FALLA for r in R)
    na = sum(r.nivel == AVISO for r in R)
    print(f"\n{'❌' if nf else '✅'} {nf} fallas, {na} avisos.")
    print("   No mide (a mano): islas de GND (§7.4), punto de toma de FB, retornos por vías.")
    return nf


# ---------------------------------------------------------------------------
# Carga: siempre sobre una copia, con su .kicad_pro y .kicad_dru al lado.
# ---------------------------------------------------------------------------
def cargar(ruta, tmp):
    base = os.path.splitext(os.path.basename(ruta))[0]
    for ext in (".kicad_pcb", ".kicad_pro", ".kicad_dru"):
        src = os.path.splitext(ruta)[0] + ext
        if os.path.exists(src):
            shutil.copy(src, os.path.join(tmp, base + ext))
    with callado():
        return pcbnew.LoadBoard(os.path.join(tmp, base + ".kicad_pcb"))


def rellenar(b):
    with callado():
        pcbnew.ZONE_FILLER(b).Fill(b.Zones())


# ---------------------------------------------------------------------------
# Autoprueba
# ---------------------------------------------------------------------------
def autoprueba(ruta):
    fallas = []

    def exigir(cond, que):
        print(f"  {'✅' if cond else '❌'} {que}")
        if not cond:
            fallas.append(que)

    print("Geometría y veredictos, contra respuestas conocidas:")
    exigir(abs(distancia(rect(0, 0, 1, 1), rect(3, 0, 4, 1)) - 2.0) < 1e-9, "dos cuadrados a 2 mm → 2,0")
    exigir(abs(distancia(rect(0, 0, 1, 1), rect(4, 5, 5, 6)) - 5.0) < 1e-9, "en diagonal 3-4-5 → 5,0")
    exigir(distancia(rect(0, 0, 4, 4), rect(1, 1, 2, 2)) == 0, "uno dentro del otro → 0")
    exigir(abs(distancia(Forma([((0, 3), (10, 3))], 0.5), rect(0, 0, 10, 1)) - 1.5) < 1e-9,
           "pista de 1 mm a 2 mm de un pad → 1,5")
    exigir(distancia(Forma([((-1, 0.5), (11, 0.5))], 0.1), rect(0, 0, 10, 1)) == 0, "pista que cruza un pad → 0")
    exigir(veredicto_cin(0.5, 0.5, False) == FALLA, "capacitor pegado pero en la otra cara → ❌")
    exigir(veredicto_cin(CIN_OK, 0.1, True) == OK, f"capacitor a {f1(CIN_OK)} mm → ✅")
    exigir(veredicto_cin(CIN_OK + 0.1, 0.1, True) == AVISO, "un poco más lejos → ⚠️")
    exigir(veredicto_cin(CIN_MAX + 0.1, 0.1, True) == FALLA, f"más de {f1(CIN_MAX)} mm → ❌")
    exigir(veredicto_cin_buck(True, 2, 2) == OK and veredicto_cin_buck(True, 1, 2) == AVISO
           and veredicto_cin_buck(False, 3, 2) == AVISO and veredicto_cin_buck(False, 0, 2) == FALLA,
           "el resumen por buck: uno pegado Y dos cerca → ✅; nada → ❌")
    exigir(all(math.isclose(faradios(v), f) for v, f in
               (("10uF x 50V", 10e-6), ("0.1uF x 50v", 1e-7), ("220pF", 220e-12), ("4.7nF", 4.7e-9))),
           "lee los valores de capacitor del BOM")

    print("\nPlaca VACÍA (no hay nada que medir):")
    with callado():
        vacia = pcbnew.BOARD()
    R = chequear(Placa(vacia))
    exigir(not any(r.nivel == OK for r in R), "ningún ✅")
    for sec in ("cin", "antena"):
        exigir(any(r.seccion == sec and r.nivel == FALLA for r in R), f"sección '{sec}': ❌ por no encontrar nada")

    with tempfile.TemporaryDirectory() as tmp:
        def nueva():
            return cargar(ruta, tmp)

        def medir(b, rellenando=False):
            if rellenando:
                rellenar(b)
            return chequear(Placa(b))

        def buscar(R, sec, suj):
            return [r for r in R if r.seccion == sec and r.sujeto == suj]

        print(f"\nMutantes sobre {ruta}:")
        b0 = nueva()
        R0 = medir(b0, rellenando=True)
        def misma_cara(f, ic):
            if f.IsFlipped() != ic.IsFlipped():
                f.Flip(f.GetPosition(), pcbnew.FLIP_DIRECTION_TOP_BOTTOM)

        def acercar(f, pads_destino, nets_destino):
            """Pone f encima de los pads destino, en la orientación que mejor los
            pisa. Que se superponga con el integrado no importa: acá sólo se mide."""
            cx = sum(p.GetPosition().x for p in pads_destino) // len(pads_destino)
            cy = sum(p.GetPosition().y for p in pads_destino) // len(pads_destino)
            mejor = None
            for ang in (0, 90, 180, 270):
                f.SetOrientationDegrees(ang)
                f.SetPosition(pcbnew.VECTOR2I(cx, cy))
                peor = max(dmin([forma_pad(p) for p in pad_en_red(f, n)],
                                [forma_pad(q) for q in pads_destino if q.GetNetname() == n])
                           for n in nets_destino)
                if mejor is None or peor < mejor[0]:
                    mejor = (peor, ang)
            f.SetOrientationDegrees(mejor[1])
            f.SetPosition(pcbnew.VECTOR2I(cx, cy))

        def alejar(f, de):
            """Lleva f a la esquina de la placa más lejana de `de` (adentro del
            contorno, para que siga contando como ubicado)."""
            x0, y0, x1, y1 = Placa(f.GetBoard()).borde
            dx, dy = xy(de.GetPosition())
            ex = x0 + 5 if dx > (x0 + x1) / 2 else x1 - 5
            ey = y0 + 5 if dy > (y0 + y1) / 2 else y1 - 5
            f.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(ex), pcbnew.FromMM(ey)))

        for cfg in BUCKS:
            for ic0 in por_valor(b0, cfg["valor"]):
                ref = ic0.GetReference()
                vin = pads_func(ic0, "VIN", "IN")[0].GetNetname()
                gnd = pads_func(ic0, "GND")[0].GetNetname()
                sw = pads_func(ic0, "SW")[0].GetNetname()

                # a0) en positivo: los cerámicos, en la misma cara y encima de VIN/GND
                b = nueva()
                ic = b.FindFootprintByReference(ref)
                destino = pads_func(ic, "VIN", "IN") + pads_func(ic, "GND")
                cins = [c for c in capacitores_entre(b, vin, gnd) if not es_electrolitico(c)]
                for c in cins:
                    misma_cara(c, ic)
                    acercar(c, destino, (vin, gnd))
                R = medir(b)
                exigir(bool(cins) and buscar(R, "cin", ref)[0].nivel == OK
                       and all(buscar(R, "cin", f"{ref}/{c.GetReference()}")[0].nivel == OK for c in cins),
                       f"{ref}: los capacitores de entrada bien puestos → ✅ (no da ❌ por cualquier cosa)")

                # a) los capacitores de entrada, a la otra punta de la placa
                b = nueva()
                ic = b.FindFootprintByReference(ref)
                cins = [c for c in capacitores_entre(b, vin, gnd) if not es_electrolitico(c)]
                for c in cins:
                    alejar(c, ic)
                R = medir(b)
                ok = bool(cins)
                for c in cins:
                    antes = buscar(R0, "cin", f"{ref}/{c.GetReference()}")
                    despues = buscar(R, "cin", f"{ref}/{c.GetReference()}")
                    ok &= bool(antes and despues and antes[0].valor is not None
                               and despues[0].valor is not None
                               and despues[0].valor > max(antes[0].valor, 30) and despues[0].nivel == FALLA)
                ok &= buscar(R, "cin", ref)[0].nivel == FALLA
                exigir(ok, f"{ref}: alejar los {len(cins)} capacitores de entrada → la distancia crece y da ❌")

                # b0) en positivo: el inductor en la misma cara, encima del pin SW
                b = nueva()
                ic = b.FindFootprintByReference(ref)
                L = [f for f in b.GetFootprints() if f.GetReference().startswith("L") and pad_en_red(f, sw)]
                for f in L:
                    misma_cara(f, ic)
                    acercar(f, pads_func(ic, "SW"), (sw,))
                R = medir(b)
                exigir(bool(L) and all(buscar(R, "sw", f"{ref}/{f.GetReference()}")[0].nivel == OK for f in L),
                       f"{ref}: el inductor bien puesto → ✅")

                # b) el inductor, a la otra punta de la placa
                b = nueva()
                L = [f for f in b.GetFootprints() if f.GetReference().startswith("L") and pad_en_red(f, sw)]
                ic = b.FindFootprintByReference(ref)
                for f in L:
                    alejar(f, ic)
                R = medir(b)
                ok = bool(L)
                for f in L:
                    a, d = buscar(R0, "sw", f"{ref}/{f.GetReference()}"), buscar(R, "sw", f"{ref}/{f.GetReference()}")
                    ok &= bool(a and d and d[0].valor is not None
                               and d[0].valor > max(a[0].valor, 30) and d[0].nivel == FALLA)
                exigir(ok, f"{ref}: alejar el inductor → la distancia SW crece y da ❌")

                # c) vía dentro del pad SW
                b = nueva()
                ic = b.FindFootprintByReference(ref)
                v = pcbnew.PCB_VIA(b)
                v.SetPosition(pads_func(ic, "SW")[0].GetPosition())
                v.SetDrill(pcbnew.FromMM(0.3))
                v.SetWidth(pcbnew.FromMM(0.6))
                v.SetNet(b.FindNet(sw))
                b.Add(v)
                R = medir(b)
                exigir(any(r.sujeto == f"{ref}/sw" and r.nivel == FALLA for r in R)
                       and buscar(R, "sw", f"{ref}/vias")[0].valor >= 1,
                       f"{ref}: una vía en el pad SW → ❌ y la cuenta de vías de SW sube")

                # d) vías térmicas: sacarlas todas / poner una
                if cfg["pad_termico"]:
                    b = nueva()
                    ic = b.FindFootprintByReference(ref)
                    fg = [forma_pad(p) for p in pads_func(ic, "GND")]
                    for t in [t for t in b.GetTracks() if t.GetClass() == "PCB_VIA"
                              and dmin([forma_pista(t)], fg) == 0]:
                        b.Remove(t)
                    R = medir(b)
                    exigir(buscar(R, "termico", f"{ref}/gnd")[0].nivel == FALLA,
                           f"{ref}: sin vías en el pad de GND → ❌")
                    v = pcbnew.PCB_VIA(b)
                    v.SetPosition(pads_func(ic, "GND")[0].GetPosition())
                    v.SetDrill(pcbnew.FromMM(0.3))
                    v.SetWidth(pcbnew.FromMM(0.6))
                    v.SetNet(b.FindNet(gnd))
                    b.Add(v)
                    R = medir(b)
                    r = buscar(R, "termico", f"{ref}/gnd")[0]
                    exigir(r.nivel == OK and r.valor == 1, f"{ref}: con una vía en el pad de GND → ✅ y cuenta 1")

                # e) una pista de SW pegada a la red de FB
                b = nueva()
                ic = b.FindFootprintByReference(ref)
                fbred = pads_func(ic, "FB")[0].GetNetname()
                otros = [p for f in b.GetFootprints() if f is not ic for p in pad_en_red(f, fbred)]
                if otros:
                    pad = otros[0]
                    p, alto = pad.GetPosition(), pad.GetBoundingBox().GetHeight()
                    y = p.y + alto // 2 + pcbnew.FromMM(0.4)     # borde de la pista a 0,2 mm del pad
                    t = pcbnew.PCB_TRACK(b)
                    t.SetStart(pcbnew.VECTOR2I(p.x - pcbnew.FromMM(2), y))
                    t.SetEnd(pcbnew.VECTOR2I(p.x + pcbnew.FromMM(2), y))
                    t.SetWidth(pcbnew.FromMM(0.4))
                    t.SetLayer(pcbnew.B_Cu if pad.GetParentFootprint().IsFlipped() else pcbnew.F_Cu)
                    t.SetNet(b.FindNet(sw))
                    b.Add(t)
                    R = medir(b)
                    r, r0 = buscar(R, "fb", ref), buscar(R0, "fb", ref)
                    # ≤ 0,2: puede haber otro cobre de FB todavía más cerca de la pista nueva
                    exigir(bool(r) and r[0].nivel == AVISO and r[0].valor <= 0.2 + 1e-6
                           and (not r0 or r0[0].valor is None or r[0].valor < r0[0].valor),
                           f"{ref}: una pista de SW a 0,2 mm de {pad.GetParentFootprint().GetReference()} "
                           f"(FB) en su misma capa → ⚠️, mide {f1(r[0].valor) if r else '—'} mm")
                    t.SetLayer(pcbnew.In2_Cu)
                    R = medir(b)
                    r = buscar(R, "fb", ref)
                    exigir(bool(r) and r[0].valor is not None and r[0].valor > 0.3,
                           f"{ref}: la misma pista en In2, detrás del plano → no cuenta")

                # f) el AK5538 al lado del buck
                b = nueva()
                ic = b.FindFootprintByReference(ref)
                ak = por_valor(b, "AK5538")
                if ak:
                    a = ak[0]
                    a.SetPosition(pcbnew.VECTOR2I(ic.GetPosition().x, ic.GetPosition().y + pcbnew.FromMM(8)))
                    R = medir(b)
                    r = buscar(R, "separacion", f"{ref}/{a.GetReference()}")
                    exigir(bool(r) and r[0].nivel == AVISO and r[0].valor < SEP_ANALOG,
                           f"{ref}: el AK5538 puesto al lado → ⚠️")

        # g) cobre bajo la antena: una pista, y un relleno con el keepout desactivado
        for m0 in por_valor(b0, "ESP32"):
            ref = m0.GetReference()
            b = nueva()
            m = b.FindFootprintByReference(ref)
            ko = [z for z in m.Zones() if z.GetIsRuleArea()]
            if not ko:
                continue
            bb = ko[0].GetBoundingBox()
            t = pcbnew.PCB_TRACK(b)
            t.SetStart(pcbnew.VECTOR2I(bb.GetLeft() - pcbnew.FromMM(1), bb.GetCenter().y))
            t.SetEnd(pcbnew.VECTOR2I(bb.GetRight() + pcbnew.FromMM(1), bb.GetCenter().y))
            t.SetWidth(pcbnew.FromMM(0.25))
            t.SetLayer(pcbnew.In1_Cu)
            t.SetNet(b.FindNet("GND"))
            b.Add(t)
            R = medir(b)
            exigir(any(r.sujeto == f"{ref}/cobre" and r.nivel == FALLA for r in R),
                   f"{ref}: una pista de GND en In1 cruzando la antena → ❌")
            # Un plano de GND en In1 ya relleno bajo la antena. Se le pone el
            # relleno a mano: así se prueba la intersección de polígonos de esta
            # herramienta, no el rellenador de KiCad.
            b = nueva()
            m = b.FindFootprintByReference(ref)
            ko = [z for z in m.Zones() if z.GetIsRuleArea()][0]
            ko.SetDoNotAllowZoneFills(False)
            z = pcbnew.ZONE(b)
            z.SetLayer(pcbnew.In1_Cu)
            z.SetNet(b.FindNet("GND"))
            # Ojo: SetOutline() se queda con el puntero y lo libera KiCad; pasarle
            # un objeto de Python termina en doble liberación (segfault). Se
            # copian las esquinas una por una.
            cad = ko.Outline().Outline(0)
            for k in range(cad.PointCount()):
                z.AppendCorner(cad.CPoint(k), -1)
            z.SetFilledPolysList(pcbnew.In1_Cu, ko.Outline())
            b.Add(z)
            R = medir(b)
            exigir(any(r.sujeto == f"{ref}/cobre" and r.nivel == FALLA for r in R)
                   and any(r.sujeto == f"{ref}/reglas" and r.nivel == AVISO for r in R),
                   f"{ref}: keepout que permite rellenos y un plano en In1 bajo la antena → ⚠️ por la regla y ❌ por el cobre")

        # h) un TP desconectado
        tps = [f for f in b0.GetFootprints() if f.GetReference().startswith("TP")
               and any(p.GetNetname() == "+3V3" for p in f.Pads())]
        if tps:
            b = nueva()
            t = b.FindFootprintByReference(tps[0].GetReference())
            for p in t.Pads():
                p.SetNetCode(0)
            R = medir(b)
            exigir(any(r.sujeto == t.GetReference() and r.nivel == FALLA for r in R)
                   and buscar(R, "tp", "+3V3")[0].nivel == AVISO,
                   f"{t.GetReference()} sin red → ❌, y +3V3 queda sin punto de prueba → ⚠️")

        # i) los capacitores de +3V3, todos a la otra punta de la placa
        stm = por_valor(b0, "STM32")
        if stm:
            b = nueva()
            P = Placa(b)
            sx, sy = xy(b.FindFootprintByReference(stm[0].GetReference()).GetPosition())
            x0, y0, x1, y1 = P.borde
            lejos_x = x0 + 2 if sx > (x0 + x1) / 2 else x1 - 2
            lejos_y = y0 + 2 if sy > (y0 + y1) / 2 else y1 - 2
            for f in b.GetFootprints():
                if f.GetReference().startswith("C") and any(p.GetNetname() == "+3V3" for p in f.Pads()):
                    f.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(lejos_x), pcbnew.FromMM(lejos_y)))
            R = medir(b)
            r = buscar(R, "desacople", stm[0].GetReference())
            exigir(bool(r) and r[0].nivel == AVISO, f"{stm[0].GetReference()}: sin capacitores de +3V3 cerca → ⚠️")

    print(f"\n{'❌ la herramienta NO pasa su autoprueba' if fallas else '✅ autoprueba completa'}"
          f" ({len(fallas)} fallas)")
    return 1 if fallas else 0


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    raiz = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ruta = os.path.abspath(args[0]) if args else os.path.join(raiz, "hardware/DS/DS.kicad_pcb")
    if not os.path.exists(ruta):
        sys.exit(f"check_layout: no existe {ruta}")
    if "--autoprueba" in sys.argv:
        sys.exit(autoprueba(ruta))
    with tempfile.TemporaryDirectory() as tmp:
        b = cargar(ruta, tmp)
        relleno = "--sin-relleno" not in sys.argv
        if relleno:
            rellenar(b)
        print(f"check_layout: {os.path.relpath(ruta)}  "
              f"({'rellenos recalculados en memoria' if relleno else 'rellenos tal como están guardados'}; "
              "el archivo no se toca)")
        nf = imprimir(chequear(Placa(b)))
    sys.exit(1 if nf else 0)


if __name__ == "__main__":
    main()
