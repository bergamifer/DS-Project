#!/usr/bin/env python3
"""
Lo PRIMERO de la manana: ¿los microfonos andan, y a que nivel?

    python3 tools/nivel.py --host 192.168.100.77          # con tono, 1 kHz
    python3 tools/nivel.py --host ... --silencio          # solo el piso de sala
    python3 tools/nivel.py --host ... --hz 2000 --seguir  # continuo, para ajustar

Por que existe: todo lo demas —perfil, fase, autofocus— depende de que las
entradas esten en un nivel razonable, y ese es el paso que traba la manana. Con
la ganancia baja, la deteccion coherente mide ruido y los informes salen vacios;
con la ganancia alta, el ADC recorta y las fases que salen son las de la
distorsion. En el medio hay unos 40 dB de margen y ninguna forma comoda de
encontrarlos leyendo JSON a mano.

Mide DOS cosas y las resta, que es lo unico que importa:

    piso     = lo que entra con el muro callado  (sala + preamplificador)
    senal    = lo que entra con el muro sonando
    margen   = senal - piso

Con menos de 20 dB de margen no vale la pena medir nada: hay que subir la
ganancia de entrada, subir el volumen, o acercar el microfono. Con mas de 55 dB
sobra, y conviene bajar antes de que un transitorio recorte.

NO enciende los amplificadores por su cuenta. Ninguna herramienta de este repo
lo hace: encender 32 amplificadores es una accion fisica y la decide una
persona.
"""

import argparse
import json
import sys
import time
import urllib.error
import urllib.request

FULL_SCALE = 8388607.0          # 2^23 - 1, la escala de 24 bits del AK5538VN
MODO_TONO = 12                  # tono interno + steering TX a 1 m


def _get(host, path, t=8):
    with urllib.request.urlopen(f"http://{host}{path}", timeout=t) as r:
        return json.loads(r.read().decode())


def _post(host, path, obj, t=8):
    req = urllib.request.Request(
        f"http://{host}{path}", data=json.dumps(obj).encode(),
        headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=t) as r:
        return json.loads(r.read().decode())


def dbfs(cuentas):
    return -999.0 if cuentas <= 0 else 20.0 * __import__("math").log10(cuentas / FULL_SCALE)


def leer(host, n=3):
    """Pico por entrada, en dBFS. adc_pk se REINICIA al leerse, asi que la
    primera lectura se tira: trae el pico acumulado desde vaya a saber cuando."""
    _get(host, "/status")
    time.sleep(0.4)
    pk = [0] * 8
    for _ in range(n):
        time.sleep(0.6)
        s = _get(host, "/status")
        for c, v in enumerate(s["adc_pk"]):
            pk[c] = max(pk[c], v)
    return [dbfs(v) for v in pk], pk


def linea(etq, db, canales):
    return f"  {etq:<10}" + "  ".join(
        f"c{c}={db[c]:>7.1f}" for c in canales)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.100.77")
    ap.add_argument("--hz", type=int, default=1000)
    ap.add_argument("--canales", type=int, nargs="+", default=[0, 1])
    ap.add_argument("--silencio", action="store_true",
                    help="no emitir: medir solo el piso")
    ap.add_argument("--seguir", action="store_true",
                    help="repetir hasta Ctrl-C, para ajustar la ganancia en vivo")
    a = ap.parse_args()

    try:
        st = _get(a.host, "/status")
    except urllib.error.URLError as e:
        sys.exit(f"no se pudo hablar con {a.host}: {e}")

    print(f"placa {st.get('device_id')}   fw {st.get('fw_id')}   "
          f"proto {st.get('proto_ver')}")
    print(f"volumen {st.get('vol_db')} dB   ganancia de entrada "
          f"{st.get('ingain_db')} dB   amplificadores "
          f"{'ENCENDIDOS' if st.get('amps') else 'apagados'}")
    print()

    # --- piso ---
    _post(a.host, "/mode", {"mode": 0})
    time.sleep(0.6)
    piso, piso_pk = leer(a.host)
    print(linea("piso", piso, a.canales) + "   dBFS (muro callado)")

    if a.silencio:
        return

    if not st.get("amps"):
        print()
        print("  Los amplificadores estan APAGADOS: sin ellos no hay senal que")
        print("  medir. Encendelos y volve a correr:")
        print(f"    curl -X POST http://{a.host}/amps -d '{{\"on\":true}}'")
        return

    # --- senal ---
    _post(a.host, "/mode", {"mode": MODO_TONO})
    _post(a.host, "/tone", {"hz": a.hz, "dither": 0})
    _post(a.host, "/beam", {"type": "tx", "fx": 0, "fy": 0, "fz": 1000})
    time.sleep(1.0)

    try:
        while True:
            sen, sen_pk = leer(a.host)
            print(linea("senal", sen, a.canales) +
                  f"   dBFS ({a.hz} Hz, foco al frente)")
            print(linea("margen", [sen[c] - piso[c] for c in range(8)], a.canales)
                  + "   dB sobre el piso")

            st2 = _get(a.host, "/status")
            ig = st2.get("ingain_db", 0)
            print()
            for c in a.canales:
                m = sen[c] - piso[c]
                if sen[c] < -80:
                    v = ("SIN SENAL. J9 es entrada de LINEA y NO tiene bias: un "
                         "electret enchufado directo no suena. Revisa el "
                         "preamplificador y el cable.")
                elif sen[c] > -3:
                    v = (f"RECORTANDO o al borde. Baja la ganancia de entrada: "
                         f"POST /ingain {{\"db\":{max(0, ig - 6)}}}")
                elif m < 20:
                    subir = min(36, ig + int(min(24, 25 - m)))
                    v = (f"margen POBRE ({m:.0f} dB). Subi la ganancia: "
                         f"POST /ingain {{\"db\":{subir}}}  — y si ya esta en 36, "
                         f"subi el volumen o acerca el microfono.")
                elif sen[c] > -12:
                    v = (f"margen {m:.0f} dB, pero queda poco techo "
                         f"({-sen[c]:.0f} dB). Baja unos 6 dB de ganancia.")
                else:
                    v = f"BIEN: {m:.0f} dB de margen y {-sen[c]:.0f} dB de techo."
                print(f"  c{c}: {v}")

            if not a.seguir:
                break
            print("\n  (Ctrl-C para salir)\n")
    except KeyboardInterrupt:
        print()
    finally:
        _post(a.host, "/mode", {"mode": 0})
        print("\n  modo 0 — el muro quedo callado.")


if __name__ == "__main__":
    main()
