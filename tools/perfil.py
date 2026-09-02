#!/usr/bin/env python3
"""
Perfil del haz: mueve el foco y mide el nivel en los DOS microfonos a la vez.

    python3 tools/perfil.py --host 192.168.100.77 --hz 2000 -o perfil_2k.json
    python3 tools/perfil.py --host ... --hz 800 1200 2000 3000 -o barrido.json
    python3 tools/perfil.py perfil_2k.json          # re-analiza lo guardado
    python3 tools/perfil.py --comparar A.json B.json

QUE MIDE, Y POR QUE HACE FALTA UN BANCO DE DOS MICROFONOS
-------------------------------------------------------------------------------
Hasta ahora "el foco funciona" se verificaba caminando la sala y comparando dos
modos de memoria. Eso alcanza para una demostracion y no alcanza para decidir
hardware: no da un numero, no se repite, y no distingue "el foco se movio" de
"el volumen subio".

Con DOS microfonos fijos, uno enfrente del centro y otro corrido, el foco se
barre POR SOFTWARE y los dos microfonos miden a la vez, en la misma sala, con la
misma temperatura y el mismo instante. De cada corrida salen dos perfiles.

    A ────────── el foco pasa por A, pico
              \\
    B ──────────── el mismo recorrido, visto 250 mm mas alla

LA PRUEBA DE ACEPTACION, QUE TIENE RESPUESTA CONOCIDA
-------------------------------------------------------------------------------
Los dos microfonos ven EL MISMO barrido desde posiciones distintas, asi que sus
perfiles tienen que ser la misma curva CORRIDA por la separacion fisica:

    pico(B) - pico(A)  ==  separacion real entre los microfonos

Si el banco dice 250 mm y la medicion dice 250 mm, toda la cadena quedo validada
de una vez: la tabla de geometria, el calculo de retardos, los Q8, el mapeo de
canales y el arnes. Si dice 400 mm, hay un error de escala en la geometria; si
dice 0, el foco no se esta moviendo y lo que cambia es el volumen.

Es la unica parte de este script que puede FALLAR con un numero conocido de
antemano, y por eso es la que vale.

QUE SALE ADEMAS
-------------------------------------------------------------------------------
- Ancho del lobulo principal a -3 y a -6 dB, en mm. Es "el tamano del foco",
  medido en vez de calculado.
- Contraste: cuanto cae el nivel en B cuando el foco esta clavado en A. Es el
  numero del que depende la obra —15 nodos que suenan a la vez— y hasta ahora
  no existia. Ver docs/instalacion_15_nodos.md.
- Contraste SIMETRICO: se enfoca en A y se lee B, y despues al reves. El
  promedio de los dos cocientes cancela TODO lo que sea asimetria fija
  (sensibilidad distinta de los microfonos, ganancia distinta del preamplificador,
  un modo de la sala en uno de los dos puntos). Es lo que hace que no haga falta
  aparear los microfonos.

COMO DETECTA
-------------------------------------------------------------------------------
Goertzel a la frecuencia exacta del tono, no RMS. La ventana de 1024 muestras da
unos 30 dB de ganancia de proceso: el ruido de sala, el zumbido de red y el piso
del preamplificador se promedian a cero y queda solo el tono. Con RMS a secas el
piso de la sala tapa los -20 dB de contraste que justamente se quieren medir.

REQUISITOS
-------------------------------------------------------------------------------
- Firmware con PROTO_VERSION >= 15 (POST /tone). Sin el, el tono lleva el LFO
  de +-5% en 3 Hz y ninguna medicion repite.
- Los amplificadores encendidos: POST /amps {"on":true}.
- Ganancia de entrada acorde a los microfonos (POST /ingain).
"""

import argparse
import json
import math
import struct
import sys
import time
import urllib.error
import urllib.request

import numpy as np

FULL_SCALE = 8388607.0
HDR = "<HBBIiI"
RATE = 48000

# Modo con tono interno + steering TX enfocado a 1 m. La frecuencia y el dither
# se pisan despues con POST /tone; el foco, con POST /beam.
MODO_TONO = 12


# ------------------------------------------------------------------ transporte

def _post(host, path, obj, timeout=8):
    req = urllib.request.Request(
        f"http://{host}{path}", data=json.dumps(obj).encode(),
        headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def _get(host, path, timeout=60):
    with urllib.request.urlopen(f"http://{host}{path}", timeout=timeout) as r:
        return r.read()


def estado(host):
    return json.loads(_get(host, "/status", timeout=8).decode())


def capturar(host, n):
    raw = _get(host, f"/capture?n={n}", timeout=120)
    n_samples, n_ch, shift, rate, peak, total = struct.unpack_from(HDR, raw, 0)
    cuerpo = raw[struct.calcsize(HDR):]
    if len(cuerpo) < total:
        n_samples = len(cuerpo) // (n_ch * 2)
        cuerpo = cuerpo[:n_samples * n_ch * 2]
    x = (np.frombuffer(cuerpo, dtype="<i2").astype(np.float64)
         * float(1 << shift)).reshape(-1, n_ch)
    return x, rate


# ------------------------------------------------------------------- detector

def goertzel_db(x, f_hz, rate=RATE):
    """Amplitud a f_hz, por canal, en dBFS. Ventana de Hann para que la fuga
    espectral no meta el ruido de al lado dentro del bin.

    La frecuencia se toma tal cual la EMITE la placa, no la nominal: el
    incremento de fase es entero (inc = hz<<16/1000), asi que 1600 pedidos son
    1599,98 emitidos. La diferencia no cambia la amplitud, pero usar el nominal
    en un detector coherente si desalinea la fase sobre ventanas largas."""
    n = x.shape[0]
    w = np.hanning(n)[:, None]
    t = np.arange(n)[:, None]
    ref = np.exp(-2j * math.pi * f_hz * t / rate)
    # 2/sum(w): normaliza a AMPLITUD de pico del seno, no a RMS ni a energia,
    # asi el 0 dBFS del detector es el mismo 0 dBFS del resto del proyecto.
    a = np.abs((x * w * ref).sum(axis=0)) * 2.0 / w.sum()
    return 20 * np.log10(np.maximum(a, 1e-9) / FULL_SCALE)


def hz_real(hz):
    """La frecuencia que el STM32 emite de verdad. Ver Tone_NextSample."""
    return ((int(hz) << 16) // 1000) * 1000.0 / 65536.0


# --------------------------------------------------------------------- medida

def medir_punto(host, eje, pos_mm, fz_mm, hz, n, settle):
    foco = {"type": "tx", "fx": 0, "fy": 0, "fz": fz_mm}
    foco["fx" if eje == "x" else "fy"] = int(pos_mm)
    _post(host, "/beam", foco)
    time.sleep(settle)
    x, rate = capturar(host, n)
    return goertzel_db(x, hz_real(hz), rate)


def barrer(host, hz_list, eje, desde, hasta, paso, fz, n, settle, canales):
    st = estado(host)
    if st.get("proto_ver", 0) < 15:
        sys.exit(f"proto_ver={st.get('proto_ver')} — hace falta >=15 (POST /tone).\n"
                 f"  make -C firmware ota OTA_HOST={host} && "
                 f"make -C firmware ota-esp OTA_HOST={host}")
    if not st.get("amps"):
        sys.exit('los amplificadores estan APAGADOS: curl -X POST '
                 f'http://{host}/amps -d \'{{"on":true}}\'')

    posiciones = list(range(int(desde), int(hasta) + 1, int(paso)))
    print(f"placa {st.get('device_id')}  fw {st.get('fw_id')}  "
          f"vol {st.get('vol_db')} dB  ingain {st.get('ingain_db')} dB")
    print(f"eje {eje.upper()}  {len(posiciones)} posiciones de {desde} a {hasta} "
          f"cada {paso} mm  a z={fz} mm")
    print(f"{len(hz_list)} frecuencia(s), {n} muestras/punto "
          f"({n / RATE * 1000:.0f} ms de ventana)\n")

    _post(host, "/mode", {"mode": MODO_TONO})
    out = {"host": host, "device_id": st.get("device_id"), "fw_id": st.get("fw_id"),
           "vol_db": st.get("vol_db"), "ingain_db": st.get("ingain_db"),
           "eje": eje, "fz_mm": fz, "n": n, "canales": canales,
           "posiciones_mm": posiciones, "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ",
                                                             time.gmtime()),
           "barridos": []}

    for hz in hz_list:
        _post(host, "/tone", {"hz": int(hz), "dither": 0})
        time.sleep(0.5)
        niveles = []
        t0 = time.time()
        for i, p in enumerate(posiciones):
            db = medir_punto(host, eje, p, fz, hz, n, settle)
            niveles.append([float(v) for v in db])
            print(f"\r  {hz:>5} Hz  {i + 1:>3}/{len(posiciones)}  "
                  f"{p:>+6} mm   " +
                  "  ".join(f"c{c}={db[c]:>6.1f}" for c in canales) + "   ",
                  end="", flush=True)
        print(f"\n  ({time.time() - t0:.0f} s)")
        out["barridos"].append({"hz": int(hz), "hz_real": hz_real(hz),
                                "niveles_db": niveles})

    # Piso de ruido: el MISMO barrido no hace falta, alcanza un punto con los
    # amplificadores apagados. Sin esto no se puede decir si un -45 dB es
    # contraste real o el piso del preamplificador.
    print("\nmidiendo el piso (amps apagados)...")
    _post(host, "/amps", {"on": False})
    time.sleep(1.0)
    x, rate = capturar(host, n)
    out["piso_db"] = [float(v) for v in goertzel_db(x, hz_real(hz_list[-1]), rate)]
    _post(host, "/amps", {"on": True})
    print("  piso: " + "  ".join(f"c{c}={out['piso_db'][c]:>6.1f}" for c in canales))
    return out


# ------------------------------------------------------------------- analisis

def _interp_cruce(pos, db, pico_i, nivel, hacia):
    """Posicion donde el perfil cruza `nivel`, interpolando lineal en dB."""
    rng = range(pico_i, len(pos) - 1) if hacia > 0 else range(pico_i, 0, -1)
    for i in rng:
        j = i + hacia
        if db[j] <= nivel <= db[i]:
            f = (db[i] - nivel) / (db[i] - db[j]) if db[i] != db[j] else 0.0
            return pos[i] + f * (pos[j] - pos[i])
    return None


def _pico_parabolico(pos, db):
    """Pico con refinamiento parabolico sobre los tres puntos de arriba. El paso
    del barrido son decenas de mm y el pico casi nunca cae justo en uno."""
    i = int(np.argmax(db))
    if 0 < i < len(db) - 1:
        y0, y1, y2 = db[i - 1], db[i], db[i + 1]
        den = y0 - 2 * y1 + y2
        if den < -1e-9:
            d = 0.5 * (y0 - y2) / den
            d = max(-0.5, min(0.5, d))
            return pos[i] + d * (pos[1] - pos[0]), i
    return float(pos[i]), i


def analizar(d, sep_esperada_mm=None, verbose=True):
    pos = np.array(d["posiciones_mm"], dtype=float)
    canales = d["canales"]
    piso = d.get("piso_db")
    res = []

    for b in d["barridos"]:
        niv = np.array(b["niveles_db"], dtype=float)     # [pos][canal]
        fila = {"hz": b["hz"], "mics": {}}
        if verbose:
            print(f"\n=== {b['hz']} Hz "
                  f"(emitidos {b.get('hz_real', b['hz']):.1f}) "
                  f"— eje {d['eje'].upper()} ===")
        for c in canales:
            db = niv[:, c]
            pk_mm, pk_i = _pico_parabolico(pos, db)
            pk_db = float(db[pk_i])
            i3 = _interp_cruce(pos, db, pk_i, pk_db - 3, -1)
            d3 = _interp_cruce(pos, db, pk_i, pk_db - 3, +1)
            i6 = _interp_cruce(pos, db, pk_i, pk_db - 6, -1)
            d6 = _interp_cruce(pos, db, pk_i, pk_db - 6, +1)
            m = {"pico_mm": pk_mm, "pico_db": pk_db,
                 "ancho3_mm": (d3 - i3) if (i3 is not None and d3 is not None) else None,
                 "ancho6_mm": (d6 - i6) if (i6 is not None and d6 is not None) else None,
                 "min_db": float(db.min()), "rango_db": float(db.max() - db.min())}
            if piso:
                m["margen_sobre_piso_db"] = pk_db - piso[c]
            fila["mics"][c] = m
            if verbose and pk_db < -90.0:
                print(f"  c{c}: !! PICO EN {pk_db:.1f} dBFS — eso es el piso del "
                      f"ADC, no una senal. El microfono no esta conectado, no "
                      f"tiene alimentacion, o los amplificadores no suenan. "
                      f"Todo lo que sigue para este canal es ruido.")
            if verbose:
                a3 = f"{m['ancho3_mm']:.0f}" if m["ancho3_mm"] else "—(no baja 3 dB)"
                a6 = f"{m['ancho6_mm']:.0f}" if m["ancho6_mm"] else "—"
                print(f"  c{c}: pico en {pk_mm:>+7.0f} mm  {pk_db:>6.1f} dBFS   "
                      f"ancho -3dB {a3:>14} mm   -6dB {a6:>6} mm   "
                      f"excursion {m['rango_db']:>4.1f} dB")

        # --- la prueba de aceptacion ---
        if len(canales) >= 2:
            ca, cb = canales[0], canales[1]
            sep = fila["mics"][cb]["pico_mm"] - fila["mics"][ca]["pico_mm"]
            fila["separacion_medida_mm"] = sep
            if verbose:
                linea = (f"  separacion medida entre c{ca} y c{cb}: "
                         f"{sep:>+7.0f} mm")
                if sep_esperada_mm:
                    err = sep - sep_esperada_mm
                    fila["error_separacion_mm"] = err
                    ok = abs(err) <= max(25.0, 0.10 * abs(sep_esperada_mm))
                    linea += (f"   esperada {sep_esperada_mm:+.0f}   "
                              f"error {err:+.0f} mm   "
                              + ("OK" if ok else "!! FUERA DE TOLERANCIA"))
                print(linea)

            # --- contraste simetrico ---
            # A en su pico: cuanto mas bajo esta B. Y al reves. El promedio
            # cancela la asimetria fija de los dos canales.
            ia = int(np.argmin(np.abs(pos - fila["mics"][ca]["pico_mm"])))
            ib = int(np.argmin(np.abs(pos - fila["mics"][cb]["pico_mm"])))
            k_a = niv[ia, ca] - niv[ia, cb]      # foco en A: A sobre B
            k_b = niv[ib, cb] - niv[ib, ca]      # foco en B: B sobre A
            fila["contraste_db"] = float(0.5 * (k_a + k_b))
            fila["contraste_asimetria_db"] = float(k_a - k_b)
            if verbose:
                print(f"  contraste: foco en c{ca} -> {k_a:+.1f} dB;  "
                      f"foco en c{cb} -> {k_b:+.1f} dB;  "
                      f"SIMETRICO {fila['contraste_db']:+.1f} dB")
                if abs(k_a - k_b) > 6:
                    print(f"     asimetria de {k_a - k_b:+.1f} dB entre los dos "
                          f"sentidos: microfonos muy dispares, o un modo de la "
                          f"sala en uno de los dos puntos. El simetrico ya lo "
                          f"cancela, pero conviene saberlo.")
            if piso:
                margen = min(fila["mics"][c]["margen_sobre_piso_db"] for c in (ca, cb))
                if margen < fila["contraste_db"] + 6:
                    if verbose:
                        print(f"     !! el contraste ({fila['contraste_db']:.1f} dB) "
                              f"esta a menos de 6 dB del piso "
                              f"({margen:.1f} dB de margen): el numero es un "
                              f"LIMITE INFERIOR, no una medida. Subi el volumen "
                              f"o la ganancia de entrada.")
                    fila["limitado_por_piso"] = True
        res.append(fila)

    if verbose and len(res) > 1:
        print("\n=== resumen ===")
        print(f"  {'Hz':>6}{'ancho -3dB':>13}{'contraste':>12}{'separacion':>13}")
        for f in res:
            c0 = f["mics"][d["canales"][0]]
            a = f"{c0['ancho3_mm']:.0f} mm" if c0["ancho3_mm"] else "—"
            print(f"  {f['hz']:>6}{a:>13}"
                  f"{f.get('contraste_db', float('nan')):>+11.1f} dB"
                  f"{f.get('separacion_medida_mm', float('nan')):>+10.0f} mm")
    return res


def graficar(d, res):
    """Perfiles en ASCII: la forma se ve mucho antes que los numeros."""
    pos = d["posiciones_mm"]
    for b, f in zip(d["barridos"], res):
        niv = np.array(b["niveles_db"])
        top = niv.max()
        print(f"\n{b['hz']} Hz — perfiles (cada linea = una posicion del foco, "
              f"0 dB = {top:.1f} dBFS)")
        for i, p in enumerate(pos):
            fila = ""
            for c in d["canales"]:
                v = niv[i, c] - top
                n = int(max(0, 40 + v))          # 40 columnas = 40 dB de rango
                fila += f" c{c}|" + "#" * n + " " * (40 - n) + f"{v:>6.1f}"
            print(f"  {p:>+6} mm{fila}")


def comparar(a, b):
    """Dos corridas del mismo banco: cuanto se repite. Sin esto no se sabe si
    una diferencia de 2 dB entre dos configuraciones significa algo."""
    ra = analizar(a, verbose=False)
    rb = analizar(b, verbose=False)
    print(f"{'Hz':>6}{'d pico c0':>12}{'d ancho':>10}{'d contraste':>13}")
    for fa, fb in zip(ra, rb):
        if fa["hz"] != fb["hz"]:
            continue
        c = a["canales"][0]
        dp = fb["mics"][c]["pico_mm"] - fa["mics"][c]["pico_mm"]
        wa, wb = fa["mics"][c]["ancho3_mm"], fb["mics"][c]["ancho3_mm"]
        dw = (wb - wa) if (wa and wb) else float("nan")
        dc = fb.get("contraste_db", float("nan")) - fa.get("contraste_db", float("nan"))
        print(f"{fa['hz']:>6}{dp:>+11.0f} mm{dw:>+9.0f}{dc:>+12.1f} dB")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("archivo", nargs="*", help="JSON ya medido, para re-analizar")
    ap.add_argument("--host", default="192.168.100.77")
    ap.add_argument("--hz", type=int, nargs="+", default=[2000],
                    help="frecuencias del tono (Hz)")
    ap.add_argument("--eje", choices=["x", "y"], default="x")
    ap.add_argument("--desde", type=int, default=-700)
    ap.add_argument("--hasta", type=int, default=700)
    ap.add_argument("--paso", type=int, default=50)
    ap.add_argument("--fz", type=int, default=1000, help="distancia del foco (mm)")
    ap.add_argument("-n", type=int, default=1024,
                    help="muestras por punto (1024 = 21 ms, ~3 s de bajada)")
    ap.add_argument("--settle", type=float, default=0.35,
                    help="espera tras mover el foco, en segundos")
    ap.add_argument("--canales", type=int, nargs="+", default=[0, 1],
                    help="entradas del ADC donde estan los microfonos")
    ap.add_argument("--sep", type=float, default=250.0,
                    help="separacion FISICA entre los microfonos, en mm; el "
                         "signo dice para que lado esta el segundo")
    ap.add_argument("-o", "--out", help="donde guardar el JSON")
    ap.add_argument("--comparar", nargs=2, metavar=("A.json", "B.json"))
    ap.add_argument("--grafico", action="store_true")
    a = ap.parse_args()

    if a.comparar:
        comparar(json.load(open(a.comparar[0])), json.load(open(a.comparar[1])))
        return

    if a.archivo:
        d = json.load(open(a.archivo[0]))
        res = analizar(d, a.sep)
        if a.grafico:
            graficar(d, res)
        return

    try:
        d = barrer(a.host, a.hz, a.eje, a.desde, a.hasta, a.paso, a.fz,
                   a.n, a.settle, a.canales)
    except urllib.error.URLError as e:
        sys.exit(f"no se pudo hablar con {a.host}: {e}")
    d["sep_esperada_mm"] = a.sep
    if a.out:
        json.dump(d, open(a.out, "w"), indent=1)
        print(f"\nguardado en {a.out}")
    res = analizar(d, a.sep)
    if a.grafico:
        graficar(d, res)


if __name__ == "__main__":
    main()
