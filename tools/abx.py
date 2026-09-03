#!/usr/bin/env python3
"""
Prueba A/B a ciegas, con puntaje. Para no engañarse solo escuchando.

    # ¿se oye la correccion del arnes cruzado?
    python3 tools/abx.py --host 192.168.100.77 --que arnes

    # ¿se oye mover el foco 250 mm, sin mover la cabeza?
    python3 tools/abx.py --host ... --que foco --a 14 -46 954 --b -233 -52 948

    python3 tools/abx.py --host ... --que foco --a ... --b ... --trampa
        agrega tiradas donde A y B son LO MISMO

POR QUE ESTO Y NO "escucha esto, y ahora esto"
-------------------------------------------------------------------------------
Cuando uno sabe cual espera que sea mejor, lo escucha mejor. No es debilidad de
caracter: es como funciona la percepcion. Una escucha informada sirve para
DESCUBRIR que buscar; no sirve para decidir si esta ahi.

Aca la maquina elige al azar, no dice cual puso, y cuenta. Al final sale un
p-valor: la probabilidad de acertar esa cantidad de veces TIRANDO UNA MONEDA. Si
da 0,3, lo que se escucho fue la moneda.

LA TRAMPA, que es la parte importante
-------------------------------------------------------------------------------
Con --trampa, algunas tiradas ponen la MISMA configuracion en las dos mitades.
Si en esas tiradas igual se percibe una diferencia clara, lo que se esta oyendo
es el CAMBIO —el silencio entre medio, el clic del comando— y no la
configuracion. Sin eso, un A/B puede dar 6 de 6 midiendo el ruido del switch.

COMO SE ESCUCHA
-------------------------------------------------------------------------------
Cada tirada tiene dos mitades de `--seg` segundos, con un aviso entre medio. Se
puede repetir cuantas veces haga falta antes de contestar. Quedarse QUIETO: si
uno se mueve entre las dos mitades cambia tambien la posicion dentro de la onda
estacionaria de la sala, y eso tapa lo que se quiere oir.
"""

import argparse
import json
import random
import sys
import time
import urllib.error
import urllib.request

MODOS_CON_MIC = {0, 2, 3, 4, 5, 6, 15}      # llevan el microfono a los parlantes


def _post(host, path, obj, t=8):
    req = urllib.request.Request(
        f"http://{host}{path}", data=json.dumps(obj).encode(),
        headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=t) as r:
        return json.loads(r.read().decode())


def _get(host, path, t=8):
    with urllib.request.urlopen(f"http://{host}{path}", timeout=t) as r:
        return json.loads(r.read().decode())


def binomial_p(aciertos, n):
    """P(acertar >= `aciertos` de `n` tirando una moneda). Sin scipy."""
    from math import comb
    return sum(comb(n, k) for k in range(aciertos, n + 1)) / 2.0 ** n


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.100.77")
    ap.add_argument("--que", choices=["arnes", "foco"], required=True)
    ap.add_argument("--a", type=float, nargs=3, default=[0, 0, 1000],
                    metavar=("X", "Y", "Z"), help="foco A (solo con --que foco)")
    ap.add_argument("--b", type=float, nargs=3, default=[-250, 0, 1000],
                    metavar=("X", "Y", "Z"), help="foco B")
    ap.add_argument("--modo", type=int, default=12)
    ap.add_argument("--hz", type=int, default=2000)
    ap.add_argument("--tiradas", type=int, default=8)
    ap.add_argument("--seg", type=float, default=4.0)
    ap.add_argument("--trampa", action="store_true",
                    help="mezclar tiradas donde A y B son lo mismo")
    a = ap.parse_args()

    try:
        st = _get(a.host, "/status")
    except urllib.error.URLError as e:
        sys.exit(f"no se pudo hablar con {a.host}: {e}")
    if a.modo in MODOS_CON_MIC:
        sys.exit(f"el modo {a.modo} lleva el microfono a los parlantes: acople.")
    if not st.get("amps"):
        sys.exit(f"amplificadores apagados:\n"
                 f"  curl -X POST http://{a.host}/amps -d '{{\"on\":true}}'")

    _post(a.host, "/mode", {"mode": a.modo})
    _post(a.host, "/tone", {"hz": a.hz, "dither": 1})

    def poner(cual, tramposa):
        """cual: 0 = A, 1 = B. En una tirada tramposa las dos son A."""
        if tramposa:
            cual = 0
        if a.que == "arnes":
            # A = el mapeo MEDIDO (cruzado); B = el directo, que desenfoca.
            _post(a.host, "/spkmap", {"cruzado": cual == 0})
        else:
            f = a.a if cual == 0 else a.b
            _post(a.host, "/beam", {"type": "tx", "fx": int(f[0]),
                                    "fy": int(f[1]), "fz": int(f[2])})

    print()
    if a.que == "arnes":
        print("  QUE SE COMPARA: el mapeo del arnes.")
        print("  Uno de los dos es el MEDIDO y el otro desenfoca a proposito.")
        print("  Parate en el punto focal (1 m, al frente del centro).")
    else:
        print("  QUE SE COMPARA: dos focos separados "
              f"{((a.a[0]-a.b[0])**2 + (a.a[1]-a.b[1])**2 + (a.a[2]-a.b[2])**2)**0.5:.0f} mm.")
        print(f"    A = {a.a}     B = {a.b}")
        print("  Parate donde esta el foco A y NO TE MUEVAS.")
    print()
    print("  Cada tirada son dos mitades de %.0f s. Contesta EN CUAL SE OYE MEJOR"
          % a.seg)
    print("  (mas fuerte / mas definido): 1, 2, o 'r' para repetir, 'i' si no se")
    print("  distingue. 'q' para cortar.")
    if a.trampa:
        print("  Hay tiradas donde las dos mitades son IGUALES. En esas, 'i' es la")
        print("  respuesta correcta.")
    print()

    orden = []
    for i in range(a.tiradas):
        tramposa = a.trampa and (i % 3 == 2)
        orden.append((random.randint(0, 1), tramposa))

    aciertos = n_real = 0
    trampas_ok = trampas = 0
    for i, (primero, tramposa) in enumerate(orden):
        while True:
            for mitad in (0, 1):
                cual = primero if mitad == 0 else 1 - primero
                poner(cual, tramposa)
                print(f"\r  tirada {i+1}/{a.tiradas} — sonando la {mitad+1}     ",
                      end="", flush=True)
                time.sleep(a.seg)
                if mitad == 0:
                    print("\r  tirada %d/%d — cambio…                " % (i+1, a.tiradas),
                          end="", flush=True)
                    time.sleep(0.8)
            r = input("\r  ¿cuál se oye mejor? [1/2/r/i/q] ").strip().lower()
            if r == "r":
                continue
            break
        if r == "q":
            break
        if tramposa:
            trampas += 1
            if r == "i":
                trampas_ok += 1
            print(f"     (era una TRAMPA: las dos eran iguales. "
                  f"{'bien' if r == 'i' else 'dijiste ' + r})")
            continue
        if r in ("1", "2"):
            n_real += 1
            elegido = primero if r == "1" else 1 - primero
            if elegido == 0:                     # 0 = A = el mapeo/foco correcto
                aciertos += 1
        # sin decir si acerto: saberlo cambia la tirada siguiente

    # dejar el equipo en la configuracion buena
    poner(0, False)
    print()
    print("  " + "=" * 56)
    if n_real:
        p = binomial_p(aciertos, n_real)
        print(f"  {aciertos} de {n_real} tiradas reales.   p = {p:.3f}")
        if p < 0.05:
            print("  SE OYE. Es muy improbable acertar tanto por azar.")
        elif p < 0.20:
            print("  Parece que se oye, pero no alcanza para afirmarlo:")
            print("  hacen falta mas tiradas.")
        else:
            print("  NO se distingue. Con esta cantidad de tiradas, lo que se")
            print("  escucho es indistinguible de tirar una moneda — y eso")
            print("  TAMBIEN es un resultado: dice cuanto cuesta el defecto.")
    if trampas:
        print(f"  trampas: {trampas_ok} de {trampas} contestadas 'igual'.")
        if trampas_ok < trampas:
            print("  ⚠  en alguna tirada donde las dos mitades eran IDENTICAS se")
            print("     percibio una diferencia. Lo que se esta oyendo incluye el")
            print("     CAMBIO en si (el silencio, el clic), no solo la config.")
            print("     Alargar --seg ayuda.")
    print("  el equipo quedo en la configuracion correcta.")


if __name__ == "__main__":
    main()
