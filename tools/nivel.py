#!/usr/bin/env python3
"""
Lo PRIMERO de la manana: ¿los microfonos andan, y a que nivel?

    python3 tools/nivel.py --host 192.168.100.77 --canales 0 7
    python3 tools/nivel.py --host ... --hasta -30     # techo de volumen distinto
    python3 tools/nivel.py --host ... --solo-piso     # sin emitir nada

Sube el volumen DE A POCO desde el mute y para en cuanto tiene margen suficiente.
No arranca fuerte para despues bajar.

=========================== EL ACOPLE, PRIMERO ===============================
Con microfonos a 1 m del muro hay modos que son un lazo cerrado:

    MODO 0 (ADC monitor)  -> cada entrada sale por su columna en las 4 filas.
                             c0 sale por los parlantes 0/8/16/24. Con ganancia
                             de entrada puesta, eso aulla.
    MODOS 2-6 (beam RX)   -> la suma de los microfonos sale por los 32.
    MODO 15 (MULTIFOCUS de entradas) -> las 8 entradas, cada una a su foco.

Los modos de MEDICION no tienen ese lazo: emiten un tono o un chirp INTERNO y
el microfono solo entra al detector, nunca a la salida. Por eso todo lo que hace
este script pasa por el modo 12 y nunca por el 0 — que es como estaba escrito
antes de que hubiera microfonos conectados, y habria sido un acople en el primer
segundo.

El piso NO se mide en el modo 0 sino con el VOLUMEN EN MUTE: el atenuador del
PCM1690 corta la salida digital y el resto de la cadena queda igual. Asi el piso
que se mide incluye el ruido de los amplificadores encendidos, que es el que va
a estar durante la medicion.
==============================================================================
"""

import argparse
import json
import math
import sys
import time
import urllib.error
import urllib.request

FULL_SCALE = 8388607.0
MODO_TONO = 12                       # tono interno + steering TX a 1 m
MODO_SEGURO = 12
# Modos que llevan la entrada del microfono a los parlantes. Ver el bloque de
# arriba. La lista es del firmware: ui_states[] con .monitor, .beam sin .tone,
# o .mfin.
MODOS_CON_MIC = {0, 2, 3, 4, 5, 6, 15}


def _get(host, path, t=8):
    with urllib.request.urlopen(f"http://{host}{path}", timeout=t) as r:
        return json.loads(r.read().decode())


def _post(host, path, obj, t=8):
    req = urllib.request.Request(
        f"http://{host}{path}", data=json.dumps(obj).encode(),
        headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=t) as r:
        return json.loads(r.read().decode())


def dbfs(c):
    return -999.0 if c <= 0 else 20.0 * math.log10(c / FULL_SCALE)


def leer(host, n=3):
    """Pico por entrada en dBFS. adc_pk se REINICIA al leerse, asi que la
    primera lectura se tira: trae el pico acumulado desde vaya a saber cuando."""
    _get(host, "/status")
    time.sleep(0.35)
    pk = [0] * 8
    for _ in range(n):
        time.sleep(0.7)
        s = _get(host, "/status")
        for c, v in enumerate(s["adc_pk"]):
            pk[c] = max(pk[c], v)
    return [dbfs(v) for v in pk]


def fila(etq, db, canales, cola=""):
    return (f"  {etq:<9}" + "  ".join(f"c{c}={db[c]:>7.1f}" for c in canales)
            + f"   {cola}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.100.77")
    ap.add_argument("--hz", type=int, default=1000)
    ap.add_argument("--canales", type=int, nargs="+", default=[0, 1])
    ap.add_argument("--desde", type=int, default=-58, help="volumen inicial (dB)")
    ap.add_argument("--hasta", type=int, default=-20, help="techo de volumen (dB)")
    ap.add_argument("--paso", type=int, default=4, help="salto de volumen (dB)")
    ap.add_argument("--margen", type=float, default=28.0,
                    help="dB sobre el piso con los que se conforma")
    ap.add_argument("--pico-max", type=float, default=-14.0,
                    help="dBFS de pico que no se pasa, para no recortar")
    ap.add_argument("--solo-piso", action="store_true")
    a = ap.parse_args()

    try:
        st = _get(a.host, "/status")
    except urllib.error.URLError as e:
        sys.exit(f"no se pudo hablar con {a.host}: {e}")

    print(f"placa {st.get('device_id')}   fw {st.get('fw_id')}   "
          f"proto {st.get('proto_ver')}")
    vol0 = st.get("vol_db")
    print(f"volumen {vol0} dB   ganancia de entrada {st.get('ingain_db')} dB   "
          f"amplificadores {'ENCENDIDOS' if st.get('amps') else 'apagados'}")

    # --- lo primero de todo: sacar la placa de un modo con lazo ---
    if st.get("mode") in MODOS_CON_MIC:
        print(f"\n  !! la placa esta en el modo {st['mode']} ({st['mode_name']}),")
        print(f"     que lleva el microfono a los parlantes. La saco de ahi ANTES")
        print(f"     de tocar los amplificadores.")
        _post(a.host, "/mode", {"mode": MODO_SEGURO})
        time.sleep(0.5)

    print()
    # --- piso, con el volumen en mute ---
    _post(a.host, "/mode", {"mode": MODO_TONO})
    _post(a.host, "/volume", {"db": -64})            # <=-64 = mute del PCM1690
    _post(a.host, "/tone", {"hz": a.hz, "dither": 0})
    _post(a.host, "/beam", {"type": "tx", "fx": 0, "fy": 0, "fz": 1000})
    time.sleep(1.0)
    piso = leer(a.host, n=4)
    print(fila("piso", piso, a.canales, "dBFS  (volumen en mute)"))

    if a.solo_piso:
        _post(a.host, "/volume", {"db": vol0})
        return

    if not st.get("amps"):
        print()
        print("  Los amplificadores estan APAGADOS. Encendelos y volve a correr:")
        print(f"    curl -X POST http://{a.host}/amps -d '{{\"on\":true}}'")
        _post(a.host, "/volume", {"db": vol0})
        return

    # --- rampa: de a poco para arriba ---
    print()
    print(f"  subiendo de {a.desde} a {a.hasta} dB de a {a.paso}, hasta tener "
          f"{a.margen:.0f} dB de margen")
    print(f"  {'vol':>6}" + "".join(f"{f'c{c} dBFS':>12}" for c in a.canales)
          + "".join(f"{f'c{c} margen':>12}" for c in a.canales))
    print("  " + "-" * (6 + 24 * len(a.canales)))

    elegido = None
    prev = None
    for v in range(a.desde, a.hasta + 1, a.paso):
        _post(a.host, "/volume", {"db": v})
        time.sleep(0.8)
        db = leer(a.host, n=3)
        m = [db[c] - piso[c] for c in range(8)]
        print(f"  {v:>6}" + "".join(f"{db[c]:>12.1f}" for c in a.canales)
              + "".join(f"{m[c]:>12.1f}" for c in a.canales))

        pico = max(db[c] for c in a.canales)
        if pico > a.pico_max:
            print(f"\n  freno: el pico llego a {pico:.1f} dBFS "
                  f"(techo {a.pico_max:.0f}). Mas arriba se recorta.")
            elegido = v
            break
        if min(m[c] for c in a.canales) >= a.margen:
            elegido = v
            print(f"\n  alcanza: {min(m[c] for c in a.canales):.0f} dB de margen "
                  f"en el peor canal.")
            break
        # Un salto MUCHO mayor que el paso de volumen no es normal: o hay un
        # lazo, o algo golpeo justo. Se avisa y se para.
        if prev is not None:
            salto = max(db[c] - prev[c] for c in a.canales)
            if salto > a.paso + 12:
                print(f"\n  !! el nivel subio {salto:.1f} dB con un paso de "
                      f"{a.paso} dB. Paro acá: puede ser un golpe en la sala, o")
                print(f"     un lazo. Volve a correr; si se repite, revisa que la")
                print(f"     placa no este en un modo con microfono.")
                elegido = v - a.paso
                break
        prev = db
    else:
        elegido = a.hasta
        print(f"\n  llegue al techo de {a.hasta} dB sin juntar {a.margen:.0f} dB "
              f"de margen.")

    _post(a.host, "/volume", {"db": elegido})
    time.sleep(0.6)
    db = leer(a.host, n=4)
    print()
    print(f"  VOLUMEN ELEGIDO: {elegido} dB")
    print(fila("senal", db, a.canales, f"dBFS a {a.hz} Hz"))
    print(fila("margen", [db[c] - piso[c] for c in range(8)], a.canales,
               "dB sobre el piso"))
    print()
    for c in a.canales:
        m = db[c] - piso[c]
        if db[c] < -80:
            v = ("SIN SENAL. J9 es entrada de LINEA y NO tiene bias: un electret "
                 "enchufado directo no suena.")
        elif m < 15:
            v = f"margen POBRE ({m:.0f} dB): subi la ganancia de entrada (/ingain)."
        elif m < 25:
            v = f"margen justo ({m:.0f} dB). Sirve, pero la sala se va a notar."
        else:
            v = f"BIEN: {m:.0f} dB de margen y {-db[c]:.0f} dB de techo."
        print(f"  c{c}: {v}")

    print()
    print(f"  la placa queda en el modo {MODO_TONO} (tono interno, sin lazo con")
    print(f"  el microfono) y a {elegido} dB. Los amplificadores, como estaban.")


if __name__ == "__main__":
    main()
