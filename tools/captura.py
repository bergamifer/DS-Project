#!/usr/bin/env python3
"""
Baja una captura del ADC de la DS11 y la analiza.

    python3 tools/captura.py                    # 2048 muestras, host por defecto
    python3 tools/captura.py -n 8192            # 170 ms, tarda ~11 s
    python3 tools/captura.py --host 192.168.1.5
    python3 tools/captura.py captura.ds         # analiza un archivo ya bajado

Escribe un .wav de 8 canales a 48 kHz —abrible con Audacity o cualquier editor—
y saca por pantalla lo que uno normalmente querria calcular a mano: nivel por
entrada, frecuencia dominante, y el retardo entre canales por correlacion
cruzada.

Por que existe: hasta ahora medir en este equipo era mirar numeros por el cable
serie. El modo 19 hace una medicion completa y devuelve UN pico y UN retardo;
todo lo demas se calculaba adentro del STM32 y se tiraba. Con las muestras
afuera, la placa deja de tener que saber de antemano que se le va a preguntar.

Formato del archivo: 16 bytes de cabecera + int16 little-endian intercalados por
canal. La cabecera es ds_cap_info_t de firmware/Core/Inc/uart_protocol.h.
"""

import argparse
import os
import struct
import sys
import urllib.request
import wave

import numpy as np

FULL_SCALE = 8388607.0          # 2^23 - 1, la escala de 24 bits del AK5538VN
HDR = "<HBBIiI"                 # n_samples, n_channels, shift, rate_hz, peak, total_bytes


def bajar(host, n, timeout):
    url = f"http://{host}/capture?n={n}"
    print(f"bajando {url} ...", flush=True)
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return r.read()


def repetir(host, n, timeout, veces):
    """Toma varias capturas y devuelve la MEDIANA, mas la tabla de dispersion.

    Por que existe: una captura son decenas de milisegundos. Si la placa tiene
    entradas cableadas a una computadora, cualquier sonido suelto —una llamada de
    WhatsApp, una notificacion, un audio— cae dentro de la ventana y se lee como
    piso de ruido. Una sola captura no puede distinguir un piso de un accidente.

    Varias capturas si: el piso se repite y el accidente aparece una vez. Se
    elige la mediana (robusta: un outlier no la mueve) y se imprime el rango por
    entrada, que es la evidencia de que no hubo ninguno. Rangos de 1-2 dB son
    normales; una entrada con decenas de dB de rango tuvo un sonido encima.
    """
    caps = []
    for i in range(veces):
        raw = bajar(host, n, timeout) if i == 0 else bajar(host, n, timeout)
        x, nch, shift, rate, peak = parsear(raw)
        rms = 20 * np.log10(
            np.maximum(np.sqrt((x.astype(np.float64) ** 2).mean(axis=0)), 1e-9)
            / FULL_SCALE)
        caps.append((float(rms.mean()), rms, raw))

    r = np.array([c[1] for c in caps])
    print(f"\n{veces} capturas, RMS por entrada (dBFS):")
    print(f"  {'ent':>4}{'min':>9}{'mediana':>9}{'max':>9}{'rango':>8}")
    print("  " + "-" * 39)
    sospecha = []
    for c in range(r.shape[1]):
        lo, med, hi = r[:, c].min(), float(np.median(r[:, c])), r[:, c].max()
        print(f"  {c:>4}{lo:>9.1f}{med:>9.1f}{hi:>9.1f}{hi - lo:>8.1f}")
        if hi - lo > 6.0:
            sospecha.append(c)
    if sospecha:
        print("  ⚠  entradas con rango >6 dB: "
              + ", ".join(str(c) for c in sospecha))
        print("     algo sono encima del piso. Si esa entrada esta cableada a la")
        print("     computadora, revisar que no haya notificaciones ni audio.")
    else:
        print("  sin outliers: el piso es estable, no hubo sonidos sueltos")

    # La mediana por energia total: una captura real y coherente consigo misma,
    # no un promedio sintetico que mezclaria espectros de instantes distintos.
    orden = sorted(range(veces), key=lambda i: caps[i][0])
    return caps[orden[veces // 2]][2]


def parsear(raw):
    if len(raw) < struct.calcsize(HDR):
        sys.exit("archivo demasiado corto: no entra ni la cabecera")
    n_samples, n_ch, shift, rate, peak, total = struct.unpack_from(HDR, raw, 0)
    cuerpo = raw[struct.calcsize(HDR):]

    if len(cuerpo) < total:
        print(f"⚠  la transferencia quedo TRUNCADA: {len(cuerpo)} de {total} bytes. "
              f"Se analiza lo que llego.")
        n_samples = len(cuerpo) // (n_ch * 2)
        cuerpo = cuerpo[:n_samples * n_ch * 2]

    x = np.frombuffer(cuerpo, dtype="<i2").astype(np.int32)
    x = x.reshape(-1, n_ch)
    # Reconstruir la escala de 24 bits: el firmware desplaza para no recortar y
    # dice cuanto (autorrango). Ver ds_cap_info_t.
    x = x << shift
    return x, n_ch, shift, rate, peak


def escribir_wav(path, x, rate):
    """int16 de 8 canales. Se normaliza al pico para que se vea algo al abrirlo:
    el .wav es para MIRAR la forma de onda, los numeros salen abajo."""
    pk = int(np.abs(x).max()) or 1
    y = (x.astype(np.float64) / pk * 32767.0).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(x.shape[1])
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(y.tobytes())
    print(f"\n{path}  ({x.shape[0]} muestras x {x.shape[1]} canales, "
          f"normalizado al pico)")


def diagnostico_ruido(x, espectro, freqs, mudos, rate, f_red):
    """Desglose por bandas de las entradas que estan en el piso de ruido.

    Por que existe: para ruido, la "frecuencia dominante" es el argmax de un
    espectro aleatorio — no significa nada y desorienta. Lo que SI distingue una
    causa de otra es en que banda esta la energia de mas. Las tres firmas que
    aparecieron midiendo la DS11 (ver docs/protocolo_medicion.md §D):

      linea de red fuerte  -> cable con la punta al aire (antena de alta impedancia)
      pico en ultrasonico  -> noise shaping del DAC sigma-delta de la fuente
      banda ancha pareja   -> el piso propio del conversor

    Una entrada con el cable suelto se lee como una placa defectuosa si no se
    mira el espectro. Este desglose es la diferencia entre las dos lecturas.
    """
    n = x.shape[0]
    # Correccion de ganancia coherente de la ventana Hann, para que los niveles
    # absolutos sean comparables entre bandas y entre capturas de largo distinto.
    cg = np.hanning(n).sum() / n
    amp = espectro / (n * cg / 2.0)

    def db(v):
        return 20 * np.log10(np.maximum(v, 1e-9) / FULL_SCALE)

    def linea(f0, bw=8.0):
        """Pico dentro de +-bw Hz: una raya espectral, no una banda."""
        m = (freqs >= f0 - bw) & (freqs <= f0 + bw)
        return db(amp[m].max(axis=0)) if m.any() else np.full(x.shape[1], -999.0)

    def banda(lo, hi):
        """RMS integrado en la banda."""
        m = (freqs >= lo) & (freqs < hi)
        return db(np.sqrt((amp[m] ** 2).sum(axis=0) / 2.0)) if m.any() else \
            np.full(x.shape[1], -999.0)

    # Con ventanas cortas no hay resolucion para separar la red de la continua.
    ciclos = n / rate * f_red
    filas = []
    if ciclos >= 3:
        filas += [(f"red {f_red:.0f} Hz", linea(f_red)),
                  (f"3er armonico", linea(3 * f_red))]
    filas += [("LF 20-400 Hz", banda(20, 400)),
              ("medios 0,4-5 kHz", banda(400, 5000)),
              ("altos 5-20 kHz", banda(5000, 20000)),
              ("ultra 20-24 kHz", banda(20000, 24000))]

    print(f"\npiso de ruido por banda, dBFS "
          f"(entradas en el piso: {', '.join(str(c) for c in mudos)})")
    print(f"  {'banda':<18}" + "".join(f"{c:>8}" for c in range(x.shape[1])))
    print("  " + "-" * (18 + 8 * x.shape[1]))
    for nombre, v in filas:
        print(f"  {nombre:<18}" + "".join(f"{d:>8.1f}" for d in v))
    if ciclos < 3:
        print(f"  (captura de {ciclos:.1f} ciclos de red: muy corta para medir "
              f"zumbido, usar -n 8192)")


def analizar(x, rate, shift, peak, f_red=50.0):
    n, nch = x.shape
    print(f"\n{n} muestras x {nch} canales @ {rate} Hz "
          f"({n / rate * 1000:.1f} ms)   autorrango: >>{shift}")
    print(f"pico absoluto: {peak} de {int(FULL_SCALE)} "
          f"({20 * np.log10(max(peak, 1) / FULL_SCALE):.1f} dBFS)\n")

    # Ventana Hann para que el espectro no invente lineas por el corte del bloque.
    w = np.hanning(n)
    espectro = np.abs(np.fft.rfft(x.astype(np.float64) * w[:, None], axis=0))
    freqs = np.fft.rfftfreq(n, 1.0 / rate)

    print(f"{'ent':>4} {'pico':>10} {'dBFS':>7} {'RMS dBFS':>9} {'f dominante':>13}")
    print("  " + "-" * 46)
    vivos, mudos, dom = [], [], {}
    for c in range(nch):
        pk = int(np.abs(x[:, c]).max())
        rms = float(np.sqrt(np.mean(x[:, c].astype(np.float64) ** 2)))
        db = 20 * np.log10(max(pk, 1) / FULL_SCALE)
        dbr = 20 * np.log10(max(rms, 1e-9) / FULL_SCALE)
        # El primer bin se ignora: es la continua, y el offset del ADC siempre gana.
        k = int(np.argmax(espectro[1:, c])) + 1
        f = freqs[k]
        # 2000 cuentas es el mismo umbral que usa el firmware para decir
        # "hay senal, no solo ruido" (MIC_LIVE_THRESH).
        if pk >= 2000:
            vivos.append(c)
            dom[c] = f
            marca, fcol = "", f"{f:>10.1f} Hz"
        else:
            mudos.append(c)
            # Para ruido el argmax es azar: mostrarlo invita a leer una causa
            # donde no hay ninguna. El desglose por bandas de abajo es lo util.
            marca, fcol = "   (piso de ruido)", f"{'--':>10}   "
        print(f"{c:>4} {pk:>10} {db:>6.1f}  {dbr:>8.1f}  {fcol}{marca}")

    if mudos:
        diagnostico_ruido(x, espectro, freqs, mudos, rate, f_red)

    if len(vivos) < 2:
        print("\n(hace falta senal en dos entradas o mas para medir retardos "
              "entre canales)")
        return

    # La correlacion cruzada solo tiene sentido entre entradas que comparten la
    # senal. Con tonos DISTINTOS por canal (el truco para identificar cableado,
    # ver docs/protocolo_medicion.md §D) el pico cae en un lag arbitrario y el
    # resultado es basura con unidades de milimetros — peor que no imprimir nada.
    # Asi que se agrupan por frecuencia dominante y se mide dentro del grupo mas
    # grande, en vez de descartar la medicion entera por un canal distinto.
    grupos = []
    for c in sorted(vivos, key=lambda c: dom[c]):
        if grupos and abs(dom[c] - dom[grupos[-1][0]]) <= 0.02 * dom[c]:
            grupos[-1].append(c)
        else:
            grupos.append([c])
    grupos.sort(key=len, reverse=True)
    fuera = [c for g in grupos[1:] for c in g]
    if fuera:
        print("\nfuera de la medicion de retardos, por tener otra frecuencia: "
              + ", ".join(f"ent {c} ({dom[c]:.1f} Hz)" for c in sorted(fuera)))
    vivos = grupos[0]
    if len(vivos) < 2:
        print("\nno se miden retardos: no hay dos entradas con la misma senal")
        return

    # Correlacion cruzada contra la primera entrada con senal. El pico dice
    # cuantas muestras esta corrida una respecto de la otra; a 48 kHz cada
    # muestra son 20,8 us, que en aire son 7,1 mm.
    print(f"\nretardo respecto de la entrada {vivos[0]} (correlacion cruzada):")
    ref = x[:, vivos[0]].astype(np.float64)
    ref -= ref.mean()
    for c in vivos[1:]:
        sig = x[:, c].astype(np.float64)
        sig -= sig.mean()
        cc = np.correlate(sig, ref, mode="full")
        lag = int(np.argmax(np.abs(cc))) - (n - 1)
        us = lag / rate * 1e6
        mm = us * 343.0 / 1000.0        # 343 m/s = 0,343 mm/us
        signo = "+" if lag >= 0 else ""
        print(f"  entrada {c}: {signo}{lag} muestras = {signo}{us:.1f} us "
              f"= {signo}{mm:.1f} mm de camino acustico")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("archivo", nargs="?",
                    help="analizar un .ds ya bajado en vez de pedir uno nuevo")
    # Cada placa se anuncia con un nombre UNICO derivado de su MAC
    # (ds-1e1f30.local). "domosonica.local" ya no existe: con dos placas
    # encendidas resolvia a cualquiera de las dos. Ver ds11_bringup_checklist §6.
    ap.add_argument("--host", default=os.environ.get("DS_HOST", "ds-1e1f30.local"),
                    help="nombre o IP de la placa (o la variable DS_HOST)")
    ap.add_argument("-n", type=int, default=2048,
                    help="muestras a capturar (64..8192; 2048 = 43 ms)")
    ap.add_argument("-o", "--out", default="captura.wav")
    ap.add_argument("--guardar", metavar="ARCHIVO",
                    help="guardar tambien el .ds crudo")
    ap.add_argument("--timeout", type=float, default=120.0)
    ap.add_argument("--repetir", type=int, default=1, metavar="N",
                    help="tomar N capturas y analizar la mediana. Obligatorio "
                         "para medir piso de ruido si hay entradas cableadas a "
                         "una computadora: delata sonidos sueltos")
    ap.add_argument("--red", type=float, default=50.0, metavar="Hz",
                    help="frecuencia de red para el diagnostico de zumbido "
                         "(50 en Argentina, 60 en America del Norte)")
    a = ap.parse_args()

    if a.archivo:
        raw = open(a.archivo, "rb").read()
    elif a.repetir > 1:
        raw = repetir(a.host, a.n, a.timeout, a.repetir)
        if a.guardar:
            open(a.guardar, "wb").write(raw)
            print(f"crudo (la mediana) guardado en {a.guardar}")
    else:
        raw = bajar(a.host, a.n, a.timeout)
        if a.guardar:
            open(a.guardar, "wb").write(raw)
            print(f"crudo guardado en {a.guardar}")

    x, nch, shift, rate, peak = parsear(raw)
    analizar(x, rate, shift, peak, a.red)
    escribir_wav(a.out, x, rate)


if __name__ == "__main__":
    main()
