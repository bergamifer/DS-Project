#!/usr/bin/env python3
"""
Capturar posiciones con los microfonos, y despues usarlas como FOCOS.

    # 1. poner los microfonos donde se quieren los focos, y medir
    python3 tools/focos.py --host 192.168.100.77 --medir A B

    # 2. sacar los microfonos y poner cada fuente en su punto
    python3 tools/focos.py --host 192.168.100.77 --aplicar A B

    python3 tools/focos.py --listar
    python3 tools/focos.py --host ... --medir C D --agregar   # sumar dos mas

EL FLUJO, QUE ES LA PREGUNTA
-------------------------------------------------------------------------------
1. Se ponen los DOS microfonos donde se quieren dos focos.
2. `--medir` corre el AUTOFOCUS (modo 11, ~2 min): 32 chirps, uno por parlante,
   y de los 32 tiempos de vuelo se ajusta por minimos cuadrados DONDE ESTA cada
   microfono. Sin decirle nada: lo resuelve el ajuste.
3. Se guardan las dos posiciones en puntos.json, con nombre.
4. Se SACAN los microfonos.
5. `--aplicar` pone cada fuente del MULTIFOCUS en su punto guardado.

Para mas de dos focos: se repite el paso 2 moviendo los microfonos, con
`--agregar`. Cada corrida suma dos puntos a la lista.

POR QUE SE GUARDA LA POSICION Y NO LOS RETARDOS
-------------------------------------------------------------------------------
El AUTOFOCUS ya sabia enfocar en el punto donde estaba el microfono: guarda
`cal_delay_q8[]` y el modo 18 los reproduce. Pero eso es UN foco y son NUMEROS
DE RETARDO, que no sirven para otra cosa.

Una POSICION en milimetros, en cambio:
  - vale para cualquiera de las 8 fuentes del MULTIFOCUS, a la vez;
  - se puede editar, mover, interpolar entre dos puntos;
  - se puede leer, discutir y dibujar;
  - no depende de la geometria del array, asi que sobrevive a un cambio de
    firmware o a otro nodo.

La medicion esta validada por tres caminos independientes que coinciden dentro
de 9 mm: el ajuste de tiempos de vuelo, la imagen de haz (tools/imagen.py), y la
cinta metrica. Ver docs/banco_dos_microfonos.md §6.2 y §8.1.

⚠️ DOS COSAS QUE HAY QUE SABER
-------------------------------------------------------------------------------
- Los focos del MULTIFOCUS **NO se guardan en la EEPROM**: se pierden al
  reiniciar. Para la instalacion de quince nodos eso hay que resolverlo; por
  ahora `--aplicar` se vuelve a correr despues de cada arranque.
- Sacar el microfono no cambia el foco. Un microfono chico en un pie no altera
  la acustica de forma apreciable, asi que la posicion medida sigue valiendo.
"""

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import calfase

ARCHIVO = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "docs", "datos", "puntos.json")
MODO_AUTOFOCUS = 11
MODO_SEGURO = 12
MODOS_CON_MIC = {0, 2, 3, 4, 5, 6, 15}


def _get(host, path, t=25):
    with urllib.request.urlopen(f"http://{host}{path}", timeout=t) as r:
        return json.loads(r.read().decode())


def _post(host, path, obj, t=10):
    req = urllib.request.Request(
        f"http://{host}{path}", data=json.dumps(obj).encode(),
        headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=t) as r:
        return json.loads(r.read().decode())


def cargar():
    try:
        return json.load(open(ARCHIVO))
    except Exception:
        return {"puntos": []}


def guardar(d):
    os.makedirs(os.path.dirname(ARCHIVO), exist_ok=True)
    json.dump(d, open(ARCHIVO, "w"), indent=1, ensure_ascii=False)


def medir(host, nombres, mics, sep, c_ms, pares):
    st = _get(host, "/status", 8)
    if st.get("proto_ver", 0) < 17:
        sys.exit(f"proto_ver={st.get('proto_ver')} — hace falta >=17 (POST /mics)")
    if not st.get("amps"):
        sys.exit(f"amplificadores apagados:\n"
                 f"  curl -X POST http://{host}/amps -d '{{\"on\":true}}'")

    _post(host, "/mode", {"mode": MODO_SEGURO})
    _post(host, "/mics", {"m1": int(mics[0]), "m2": int(mics[1])})
    print(f"placa {st.get('device_id')}   microfonos en c{mics[0]} y c{mics[1]}")
    print(f"AUTOFOCUS: 32 chirps, uno por parlante. Unos dos minutos.\n")
    _post(host, "/mode", {"mode": MODO_AUTOFOCUS})
    t0 = time.time()
    while True:
        time.sleep(3)
        s = _get(host, "/status", 8)
        print(f"\r  {s['cal_prog']}/32 parlantes, {s['cal_ok']} con senal solida   ",
              end="", flush=True)
        if s["cal_prog"] >= 32 and s["cal_state"] >= 5:
            break
        if time.time() - t0 > 300:
            sys.exit("\nel scan no termino en 5 minutos")
    print(f"\n  ({time.time() - t0:.0f} s)")
    _post(host, "/mode", {"mode": MODO_SEGURO})

    cal = _get(host, "/cal")
    orden = (np.arange(32) ^ 1) if pares else None
    out = []
    for i, nombre in enumerate(nombres):
        a = calfase.analizar(cal, c_ms, mic=i + 1, orden=orden)
        m = a["m"]
        solidos = int(a["usar"].sum())
        out.append({"nombre": nombre, "x": round(float(m[0]), 1),
                    "y": round(float(m[1]), 1), "z": round(float(m[2]), 1),
                    "rms_us": round(a["rms_us"], 2), "solidos": solidos,
                    "mic": mics[i]})
        print(f"  {nombre}: ({m[0]:+7.0f}, {m[1]:+7.0f}, {m[2]:7.0f}) mm   "
              f"residuo {a['rms_us']:.2f} us   {solidos}/32 parlantes")
        if a["rms_us"] > 15:
            print(f"     ⚠ residuo alto. Con el arnes bien mapeado deberia dar "
                  f"~4 us. ¿Falta --pares? ¿El microfono estaba tapado?")

    if len(out) == 2:
        d = ((out[0]["x"] - out[1]["x"]) ** 2 + (out[0]["y"] - out[1]["y"]) ** 2
             + (out[0]["z"] - out[1]["z"]) ** 2) ** 0.5
        err = d - sep
        print(f"\n  separacion entre los dos: {d:.0f} mm   "
              f"(cinta: {sep:.0f})   error {err:+.0f} mm")
        if abs(err) > max(25.0, 0.10 * sep):
            print("  ⚠ FUERA DE TOLERANCIA. No cargues estos puntos sin revisar:")
            print("    ¿estan los microfonos en c%d y c%d y no al reves? ¿la"
                  % (mics[0], mics[1]))
            print("    separacion real es la que dijiste? ¿el arnes esta cruzado?")
        else:
            print("  ✓ el ajuste recupera la separacion real: los puntos son de fiar.")
    return out


def aplicar(host, d, nombres):
    st = _get(host, "/status", 8)
    if st.get("mode") in MODOS_CON_MIC:
        print(f"  la placa esta en el modo {st['mode']}, que lleva el microfono a")
        print(f"  los parlantes. La saco de ahi primero.")
        _post(host, "/mode", {"mode": MODO_SEGURO})
        time.sleep(0.5)
    idx = {p["nombre"]: p for p in d["puntos"]}
    faltan = [n for n in nombres if n not in idx]
    if faltan:
        sys.exit(f"no estan guardados: {', '.join(faltan)}.  "
                 f"Guardados: {', '.join(idx) or '(ninguno)'}")
    print(f"placa {st.get('device_id')}   aplicando {len(nombres)} foco(s)\n")
    for src, n in enumerate(nombres):
        p = idx[n]
        r = _post(host, "/beam", {"type": "mf", "src": src,
                                  "fx": int(round(p["x"])),
                                  "fy": int(round(p["y"])),
                                  "fz": int(round(p["z"]))})
        print(f"  fuente {src} -> {n}  ({p['x']:+.0f}, {p['y']:+.0f}, {p['z']:.0f}) mm"
              f"   {r.get('status')}")
    print()
    print("  Los focos NO se guardan en la EEPROM: se pierden al reiniciar la")
    print("  placa. Hay que volver a correr --aplicar despues de cada arranque.")
    print()
    print("  Para que suene algo en esos focos:")
    print("    modo 14 = cuatro TONOS internos, uno por foco (para verificar)")
    print("    modo 15 = las ocho ENTRADAS del ADC, una por foco (audio real)")
    print("    ⚠ el modo 15 lleva las entradas a los parlantes: con microfonos")
    print("      puestos en c0/c1 eso es un ACOPLE. Sacalos, o usa c2 en adelante.")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.100.77")
    ap.add_argument("--medir", nargs=2, metavar=("NOMBRE1", "NOMBRE2"),
                    help="medir dos puntos con los dos microfonos y guardarlos")
    ap.add_argument("--aplicar", nargs="+", metavar="NOMBRE",
                    help="poner cada fuente del MULTIFOCUS en un punto guardado")
    ap.add_argument("--listar", action="store_true")
    ap.add_argument("--borrar", nargs="+", metavar="NOMBRE")
    ap.add_argument("--agregar", action="store_true",
                    help="sumar a la lista en vez de reemplazarla")
    ap.add_argument("--mics", type=int, nargs=2, default=[0, 1])
    ap.add_argument("--sep", type=float, default=250.0,
                    help="separacion REAL entre los microfonos, para verificar")
    ap.add_argument("--c", type=float, default=343.0)
    ap.add_argument("--pares", action="store_true", default=True,
                    help="el arnes esta cruzado de a pares (default: si)")
    ap.add_argument("--sin-pares", dest="pares", action="store_false")
    a = ap.parse_args()

    d = cargar()

    if a.listar or not (a.medir or a.aplicar or a.borrar):
        if not d["puntos"]:
            print("no hay puntos guardados. Medi con:  --medir NOMBRE1 NOMBRE2")
            return
        print(f"{len(d['puntos'])} punto(s) en {os.path.normpath(ARCHIVO)}:\n")
        print(f"  {'nombre':<14}{'X':>8}{'Y':>8}{'Z':>8}{'residuo':>10}{'parlantes':>11}")
        for p in d["puntos"]:
            print(f"  {p['nombre']:<14}{p['x']:>+8.0f}{p['y']:>+8.0f}{p['z']:>8.0f}"
                  f"{p['rms_us']:>9.1f}us{p['solidos']:>8d}/32")
        return

    if a.borrar:
        d["puntos"] = [p for p in d["puntos"] if p["nombre"] not in a.borrar]
        guardar(d)
        print(f"borrados. Quedan {len(d['puntos'])}.")
        return

    try:
        if a.medir:
            nuevos = medir(a.host, a.medir, a.mics, a.sep, a.c, a.pares)
            if not a.agregar:
                d["puntos"] = []
            d["puntos"] = [p for p in d["puntos"]
                           if p["nombre"] not in a.medir] + nuevos
            guardar(d)
            print(f"\n  guardados en {os.path.normpath(ARCHIVO)}")
            print(f"  Ahora saca los microfonos y corre:")
            print(f"    python3 tools/focos.py --host {a.host} --aplicar "
                  f"{' '.join(a.medir)}")
        if a.aplicar:
            aplicar(a.host, d, a.aplicar)
    except urllib.error.URLError as e:
        sys.exit(f"no se pudo hablar con {a.host}: {e}")


if __name__ == "__main__":
    main()
