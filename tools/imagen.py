#!/usr/bin/env python3
"""
Imagen de haz: barre el foco por una GRILLA del espacio y mapea el nivel.

    python3 tools/imagen.py --host 192.168.100.77 --plano xz -o img_xz.json
    python3 tools/imagen.py --host ... --plano xy --z 950 -o img_xy.json
    python3 tools/imagen.py img_xz.json                      # re-dibujar

QUE ES ESTO
-------------------------------------------------------------------------------
Un array enfocado puede apuntar a cualquier punto del espacio. Si se barre el
foco por una grilla y se mide cuanto llega a un microfono en cada caso, el mapa
resultante dice DONDE ESTA EL MICROFONO — no por calculo, sino porque ahi es
donde el array logra concentrar la energia. Es la misma idea que una imagen de
radar de apertura sintetica, con dos diferencias: el microfono es un receptor
cooperativo (no un eco) y el array transmite en vez de recibir.

Y hay una respuesta CONOCIDA con la que compararlo: `calfase.py` ya ubica los
microfonos ajustando los 32 tiempos de vuelo del AUTOFOCUS. Dos metodos
independientes sobre el mismo objeto — uno geometrico, otro por energia. Que
coincidan es la prueba; que no, es informacion.

POR QUE CON EL CHIRP Y NO CON TONO
-------------------------------------------------------------------------------
Porque en una sala chica el tono continuo mide la suma del directo y la
reverberacion, y el mapa que sale es el de la sala. Medido el 2026-09-03: el
mismo barrido daba 2,8 dB de contraste con tono y 17,7 con chirp enfocado. Ver
docs/banco_dos_microfonos.md §7.

LO QUE MAS INTERESA DEL MAPA: LA PROFUNDIDAD
-------------------------------------------------------------------------------
El ancho del foco en X y en Y se calcula con lambda*z/D y esta medido. La
extension en Z —cuan largo es el foco hacia adelante y hacia atras— NO se midio
nunca, y es lo que decide si "un punto de la sala" es de veras un punto o es una
linea de dos metros. Para la instalacion de quince nodos, esa diferencia es la
obra entera.
"""

import argparse
import json
import math
import sys
import time
import urllib.error
import urllib.request

FULL_SCALE = 8388607.0
MODO_CHIRP = 23


def _get(host, path, t=12):
    with urllib.request.urlopen(f"http://{host}{path}", timeout=t) as r:
        return json.loads(r.read().decode())


def _post(host, path, obj, t=10):
    req = urllib.request.Request(
        f"http://{host}{path}", data=json.dumps(obj).encode(),
        headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=t) as r:
        return json.loads(r.read().decode())


def medir(host, fx, fy, fz, espera=8.0):
    """Nivel del DIRECTO en los dos microfonos, con el foco en (fx,fy,fz).

    Espera a que `seq` avance dos veces Y a que la placa informe el foco pedido.
    Sin las dos cosas se lee la medicion del foco anterior y el mapa entero sale
    corrido, sin que nada avise."""
    d0 = _get(host, "/foco")
    _post(host, "/beam", {"type": "tx", "fx": int(fx), "fy": int(fy), "fz": int(fz)})
    t0 = time.time()
    while time.time() - t0 < espera:
        time.sleep(0.2)
        d = _get(host, "/foco")
        if (((d["seq"] - d0["seq"]) % 65536) >= 2
                and d["focus"]["fx"] == int(fx)
                and d["focus"]["fy"] == int(fy)
                and d["focus"]["fz"] == int(fz)):
            return ([20 * math.log10(max(d["peak"][m], 1) / FULL_SCALE)
                     for m in range(2)],
                    [d["q10"][m] for m in range(2)], d.get("retries", 0))
    return ([float("nan")] * 2, [0, 0], -1)


def barrer(host, plano, ejes, fijo, mics):
    st = _get(host, "/status")
    if st.get("proto_ver", 0) < 19:
        sys.exit("hace falta proto_ver >= 19 (chirp enfocado)")
    if not st.get("amps"):
        sys.exit(f"amplificadores apagados:\n"
                 f"  curl -X POST http://{host}/amps -d '{{\"on\":true}}'")
    _post(host, "/mics", {"m1": int(mics[0]), "m2": int(mics[1])})
    _post(host, "/mode", {"mode": MODO_CHIRP})
    time.sleep(2.0)

    a_vals, b_vals = ejes
    n = len(a_vals) * len(b_vals)
    print(f"placa {st.get('device_id')}  vol {st.get('vol_db')} dB   "
          f"plano {plano.upper()}, {len(a_vals)}x{len(b_vals)} = {n} puntos "
          f"(~{n * 1.3 / 60:.1f} min)")
    datos = []
    t0 = time.time()
    hecho = 0
    ret0 = _get(host, "/foco").get("retries", 0)
    for b in b_vals:
        fila = []
        for a in a_vals:
            if plano == "xz":
                fx, fy, fz = a, fijo, b
            elif plano == "xy":
                fx, fy, fz = a, b, fijo
            else:
                fx, fy, fz = fijo, a, b        # yz
            db, q, _ = medir(host, fx, fy, fz)
            fila.append([float(db[0]), float(db[1]), int(q[0]), int(q[1])])
            hecho += 1
            print(f"\r  {hecho}/{n}  ({fx:+5.0f},{fy:+5.0f},{fz:5.0f})  "
                  f"m1={db[0]:6.1f}  m2={db[1]:6.1f}   ", end="", flush=True)
        datos.append(fila)
    ret = _get(host, "/foco").get("retries", 0) - ret0
    print(f"\n  ({time.time() - t0:.0f} s, {ret} reintento(s) del perro guardian)")
    if ret:
        print(f"  ⚠ {ret} ciclo(s) se perdieron y se reintentaron. Los puntos de")
        print(f"    este mapa NO son estrictamente comparables entre si.")
    return {"host": host, "plano": plano, "fijo": fijo, "mics": list(mics),
            "a": [float(x) for x in a_vals], "b": [float(x) for x in b_vals],
            "datos": datos, "retries": ret,
            "vol_db": st.get("vol_db"), "device_id": st.get("device_id")}


def mapa(d, mic=0):
    """Mapa en ASCII, con el maximo marcado. Diez niveles de 3 dB."""
    import numpy as np
    A = np.array([[f[mic] for f in fila] for fila in d["datos"]], dtype=float)
    top = np.nanmax(A)
    esc = " .:-=+*#%@"
    ejes = {"xz": ("X", "Z"), "xy": ("X", "Y"), "yz": ("Y", "Z")}[d["plano"]]
    print(f"\n  microfono {mic + 1} (entrada c{d['mics'][mic]})   "
          f"maximo {top:.1f} dBFS   cada caracter = 3 dB")
    print(f"  {'':>7}" + "".join(f"{int(x):>5}" for x in d["a"]) + f"   <- {ejes[0]} (mm)")
    for j, b in enumerate(d["b"]):
        fila = ""
        for i in range(len(d["a"])):
            v = A[j, i] - top
            k = int(max(0, min(9, 9 + v / 3.0)))
            fila += esc[k] * 5
        print(f"  {int(b):>6} {fila}")
    print(f"  ^ {ejes[1]} (mm)")
    # maximo con refinamiento parabolico en los dos ejes
    j, i = np.unravel_index(int(np.nanargmax(A)), A.shape)
    def refina(v0, v1, v2, x0, x1, x2):
        den = v0 - 2 * v1 + v2
        if den >= -1e-9:
            return x1
        return x1 + 0.5 * (v0 - v2) / den * (x2 - x1)
    ai = d["a"][i]
    bj = d["b"][j]
    if 0 < i < len(d["a"]) - 1:
        ai = refina(A[j, i - 1], A[j, i], A[j, i + 1],
                    d["a"][i - 1], d["a"][i], d["a"][i + 1])
    if 0 < j < len(d["b"]) - 1:
        bj = refina(A[j - 1, i], A[j, i], A[j + 1, i],
                    d["b"][j - 1], d["b"][j], d["b"][j + 1])
    print(f"  MAXIMO en {ejes[0]}={ai:+.0f} mm, {ejes[1]}={bj:+.0f} mm")
    # anchos a -6 dB por el maximo
    for eje, vec, arr in ((ejes[0], d["a"], A[j, :]), (ejes[1], d["b"], A[:, i])):
        idx = np.where(arr >= top - 6)[0]
        if len(idx) >= 2:
            ancho = vec[idx[-1]] - vec[idx[0]]
            borde = " (TOCA EL BORDE del barrido)" if (idx[0] == 0 or idx[-1] == len(vec) - 1) else ""
            print(f"    ancho -6 dB en {eje}: {ancho:.0f} mm{borde}")
    return ai, bj, top


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("archivo", nargs="?")
    ap.add_argument("--host", default="192.168.100.77")
    ap.add_argument("--plano", choices=["xz", "xy", "yz"], default="xz")
    ap.add_argument("--mics", type=int, nargs=2, default=[0, 1])
    ap.add_argument("--x", type=float, nargs=3, default=[-500, 300, 50],
                    metavar=("DESDE", "HASTA", "PASO"))
    ap.add_argument("--y", type=float, nargs=3, default=[-300, 300, 50],
                    metavar=("DESDE", "HASTA", "PASO"))
    ap.add_argument("--z", type=float, nargs=3, default=[500, 1600, 100],
                    metavar=("DESDE", "HASTA", "PASO"))
    ap.add_argument("--fijo", type=float, default=None,
                    help="valor del eje que no se barre (default: -50 para Y, 950 para Z)")
    ap.add_argument("-o", "--out")
    a = ap.parse_args()

    if a.archivo:
        d = json.load(open(a.archivo))
    else:
        rng = lambda t: [t[0] + i * t[2] for i in range(int((t[1] - t[0]) / t[2]) + 1)]
        if a.plano == "xz":
            ejes, fijo = (rng(a.x), rng(a.z)), (a.fijo if a.fijo is not None else -50)
        elif a.plano == "xy":
            ejes, fijo = (rng(a.x), rng(a.y)), (a.fijo if a.fijo is not None else 950)
        else:
            ejes, fijo = (rng(a.y), rng(a.z)), (a.fijo if a.fijo is not None else 0)
        try:
            d = barrer(a.host, a.plano, ejes, fijo, a.mics)
        except urllib.error.URLError as e:
            sys.exit(f"no se pudo hablar con {a.host}: {e}")
        if a.out:
            json.dump(d, open(a.out, "w"))
            print(f"  guardado en {a.out}")
    for m in (0, 1):
        mapa(d, m)


if __name__ == "__main__":
    main()
