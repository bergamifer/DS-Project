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


def analizar(x, rate, shift, peak):
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
    vivos = []
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
        marca = "" if pk >= 2000 else "   (piso de ruido)"
        if pk >= 2000:
            vivos.append(c)
        print(f"{c:>4} {pk:>10} {db:>6.1f}  {dbr:>8.1f}  {f:>10.1f} Hz{marca}")

    if len(vivos) < 2:
        print("\n(hace falta senal en dos entradas o mas para medir retardos "
              "entre canales)")
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
    ap.add_argument("--host", default="domosonica.local")
    ap.add_argument("-n", type=int, default=2048,
                    help="muestras a capturar (64..8192; 2048 = 43 ms)")
    ap.add_argument("-o", "--out", default="captura.wav")
    ap.add_argument("--guardar", metavar="ARCHIVO",
                    help="guardar tambien el .ds crudo")
    ap.add_argument("--timeout", type=float, default=120.0)
    a = ap.parse_args()

    if a.archivo:
        raw = open(a.archivo, "rb").read()
    else:
        raw = bajar(a.host, a.n, a.timeout)
        if a.guardar:
            open(a.guardar, "wb").write(raw)
            print(f"crudo guardado en {a.guardar}")

    x, nch, shift, rate, peak = parsear(raw)
    analizar(x, rate, shift, peak)
    escribir_wav(a.out, x, rate)


if __name__ == "__main__":
    main()
