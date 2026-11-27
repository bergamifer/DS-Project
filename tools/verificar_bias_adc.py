#!/usr/bin/env python3
"""verificar_bias_adc.py — ¿cada una de las 16 entradas del AK5538 tiene su resistencia de bias?

QUÉ COMPRUEBA (sobre un netlist, el .net que escribe tools/sch2net.py):
  - Para cada red /ADC_AK5538/AINnP y AINnN (n = 1..8): hay UNA resistencia cuya otra
    pata está en el nodo VCM (la red donde están C58 y C62, el filtro del 1,65 V).
  - Esa resistencia vale 10 kΩ (C25744, decisión del 2026-10-27, DS12_CHECKLIST §B).
  - Ninguna otra resistencia cuelga del nodo VCM, salvo las dos del divisor (una a
    +3,3VA y otra a GND): lo demás sería un cableado de más.

USO
  tools/sch2net.py hardware/DS/DS.kicad_sch /tmp/DS.net
  tools/verificar_bias_adc.py /tmp/DS.net          # exit 0 = las 16, exit 1 = faltan
  tools/verificar_bias_adc.py --autoprueba          # sin nada que medir: ¿el chequeo puede fallar?

AUTOPRUEBA. Arma un netlist sintético con las 16 y comprueba que PASA; le saca una
resistencia y comprueba que FALLA nombrando la entrada; le cambia un valor y comprueba
que FALLA; le agrega una resistencia de más al VCM y comprueba que FALLA. Un chequeo
que da OK sobre un esquemático sin bias no sirve.
"""
import re
import sys

NODO_REF = (("C58", "1"), ("C62", "1"))   # el nodo VCM es la red con estas dos patas (la otra va a GND)
ESPERADO = "10k"
ENTRADAS = [f"AIN{n}{p}" for n in range(1, 9) for p in "PN"]


def leer(texto):
    """→ (valores {ref: valor}, redes {nombre: [(ref, pin)]})"""
    valores = dict(re.findall(r'\(ref "([^"]+)"\)\s+\(value "([^"]*)"', texto))
    redes = {}
    for nombre, cuerpo in re.findall(r'\(net\s+\(code "\d+"\)\s+\(name "([^"]*)"\)(.*?)\n\t\t\)\n',
                                     texto, re.S):
        redes[nombre] = re.findall(r'\(ref "([^"]+)"\)\s+\(pin "([^"]+)"\)', cuerpo)
    return valores, redes


def verificar(texto):
    valores, redes = leer(texto)
    vcm = [n for n, nodos in redes.items()
           if all(par in nodos for par in NODO_REF)]
    if len(vcm) != 1:
        return [f"no encuentro el nodo VCM (la red con C58.1 y C62.1): {vcm}"], 0
    vcm = vcm[0]
    r_en_vcm = {r for r, _ in redes[vcm] if r.startswith("R")}
    problemas, bien = [], 0
    usadas = set()
    for e in ENTRADAS:
        red = next((n for n in redes if n.endswith("/" + e)), None)
        if red is None:
            problemas.append(f"{e}: la red no existe"); continue
        # una R en esta red cuya otra pata está en VCM (el mismo ref en los dos nodos)
        cands = [r for r, _ in redes[red] if r in r_en_vcm]
        if len(cands) != 1:
            problemas.append(f"{e}: {len(cands)} resistencias al VCM (tiene que haber 1)")
            continue
        usadas.add(cands[0])
        if valores.get(cands[0]) != ESPERADO:
            problemas.append(f"{e}: {cands[0]} vale {valores.get(cands[0])}, tiene que ser {ESPERADO}")
            continue
        bien += 1
    # el divisor que genera el VCM (una R a +3,3VA y otra a GND) es legítimo
    divisor = {r for n, nodos in redes.items()
               if n == "GND" or "3.3VA" in n or "3V3A" in n for r, _ in nodos} & r_en_vcm
    sobran = sorted(r_en_vcm - usadas - divisor)
    if sobran:
        problemas.append(f"resistencias al VCM que no van a ninguna entrada del ADC: {', '.join(sobran)}")
    return problemas, bien


def _sintetico(omitir=None, extra=False):
    comp, redes = [], []
    nodos_vcm = ['(ref "C58")\n(pin "1")', '(ref "C62")\n(pin "1")']
    for i, e in enumerate(ENTRADAS):
        ref = f"R{200 + i}"
        comp.append(f'(ref "{ref}")\n(value "{ESPERADO}")')
        if e != omitir:
            nodos_vcm.append(f'(ref "{ref}")\n(pin "1")')
            redes.append((f"/ADC_AK5538/{e}", [(ref, "2")]))
        else:
            redes.append((f"/ADC_AK5538/{e}", []))
    if extra:
        comp.append('(ref "R999")\n(value "10k")')
        nodos_vcm.append('(ref "R999")\n(pin "1")')
    salida = ["(comp\n" + c + "\n)" for c in comp]
    n = 0
    def red(nombre, nodos):
        nonlocal n
        n += 1
        return f'\t\t(net\n\t\t\t(code "{n}")\n\t\t\t(name "{nombre}")\n' + "".join(
            f'\t\t\t(node\n\t\t\t\t{x}\n\t\t\t)\n' for x in nodos) + "\t\t)\n"
    txt = "\n".join(salida) + "\n"
    txt += red("Net-(C58-Pad1)", nodos_vcm)
    for nombre, nodos in redes:
        txt += red(nombre, [f'(ref "{r}")\n(pin "{p}")' for r, p in nodos])
    return txt


def autoprueba():
    fallas = 0
    def esperar(nombre, texto, debe_pasar, contiene=None):
        nonlocal fallas
        prob, bien = verificar(texto)
        ok = (not prob) == debe_pasar and (contiene is None or any(contiene in p for p in prob))
        print(("  ok   " if ok else "  FALLA ") + nombre + ("" if ok else f"  → {prob}"))
        fallas += 0 if ok else 1
    esperar("las 16 puestas: PASA", _sintetico(), True)
    esperar("falta AIN5N: FALLA y la nombra", _sintetico(omitir="AIN5N"), False, "AIN5N")
    t = _sintetico().replace('(ref "R203")\n(value "10k")', '(ref "R203")\n(value "22k")')
    esperar("una R de 22k en vez de 10k: FALLA", t, False, "22k")
    esperar("una R de más al VCM: FALLA", _sintetico(extra=True), False, "R999")
    print("autoprueba:", "ok" if not fallas else f"{fallas} FALLAS")
    return fallas


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--autoprueba":
        sys.exit(1 if autoprueba() else 0)
    if len(sys.argv) != 2:
        print(__doc__); sys.exit(2)
    prob, bien = verificar(open(sys.argv[1], encoding="utf-8").read())
    print(f"{bien} de 16 entradas con su resistencia de bias de {ESPERADO}")
    for p in prob:
        print("  ✗", p)
    sys.exit(1 if prob else 0)
