#!/usr/bin/env python3
"""
Con que FASE llega cada parlante al punto de escucha, frecuencia por frecuencia.

    python3 tools/fase.py --host 192.168.100.77 -o fase.json   # bajar y analizar
    python3 tools/fase.py fase.json                            # re-analizar
    python3 tools/fase.py fase.json --mapa                     # el mapa completo
    python3 tools/fase.py --comparar A.json B.json             # repetibilidad

QUE MIDE, Y POR QUE NINGUN MODO ANTERIOR PODIA MEDIRLO
-------------------------------------------------------------------------------
El modo 22 emite un tono puro por UN parlante por vez y detecta coherentemente
en las ocho entradas. Por cada par (parlante, frecuencia) quedan amplitud y
FASE.

  - El AUTOFOCUS (modo 11) tambien recorre los parlantes de a uno, pero
    correlaciona un chirp: devuelve UN escalar por parlante, el retardo de grupo
    promediado sobre 500 Hz-6 kHz. Si un parlante llega bien en 1 kHz y mal en
    3 kHz, ese promedio no describe ninguna de las dos.
  - El BARRIDO DE FRECUENCIA (modo 20) si da fase contra frecuencia, pero con
    los cuatro bancos sonando a la vez: mide la SUMA en el microfono, y una suma
    no se puede desarmar. Dos parlantes en contrafase dan lo mismo que dos
    parlantes mudos.

LA CUENTA
-------------------------------------------------------------------------------
La fase con que el parlante k llega al microfono a la frecuencia f es

    phi_k(f) = 2*pi*f*d_k/c  +  comun(f)  +  delta_k(f)
               |                |            |
               camino (se calcula)           lo que sobra: EL DEFECTO
                                el retardo del pipeline, igual para todos

Restando el camino geometrico y despues el promedio circular sobre los 32
parlantes, queda delta_k(f). Si el muro esta sano, delta es cero para todos.

    La DISPERSION de delta entre parlantes, frecuencia por frecuencia, es
    literalmente "cuanto se desalinean". Y no hace falta desenvolver nada:
    es una comparacion a f fija.

Y despues, por parlante, se busca el retardo tau_k que mejor explica su
delta_k(f). No por regresion —la fase esta envuelta y una regresion sobre
angulos envueltos da cualquier cosa— sino MAXIMIZANDO LA SUMA COHERENTE sobre
una grilla de tau. Eso es inmune al envolvimiento por construccion.

Lo que queda DESPUES de sacarle el mejor tau es la respuesta a la pregunta cara:

    residuo chico  -> es retardo puro, y UN SOLO NUMERO por parlante lo corrige.
    residuo grande -> ningun retardo lo corrige. La calibracion por retardos
                      tiene un techo, y conviene saberlo antes de construir
                      encima.

POLARIDAD CONTRA RETARDO: lo que el chirp NO distingue
-------------------------------------------------------------------------------
Un parlante invertido tiene pi de desfase A TODAS las frecuencias.
Un parlante con el retardo mal tiene un desfase que CRECE con f.
En el retardo de grupo del AUTOFOCUS los dos se ven parecidos; aca se separan
solos, porque uno es constante y el otro es una recta.

REQUISITOS
-------------------------------------------------------------------------------
- Firmware >= v16 y el modo 22 corrido hasta el final.
- La posicion del microfono. Por defecto se toma el nominal del banco
  (0, 0, 1000 mm); con --cal se lee la que AJUSTO calfase.py, que es mejor
  porque no depende de la cinta metrica.
"""

import argparse
import json
import math
import struct
import sys
import urllib.request

import numpy as np

# geometria del muro — espejo de Beam_InitTxGeometry() en main.c
SPK_ROWS, SPK_COLS = 4, 8
SPK_X_PITCH_MM, SPK_Y_PITCH_MM = 80.0, 160.0
SPK_ROW_DX_MM = 40.0
C_MS = 343.0
HDR = "<BBBBBBI" + "H" * 16          # ds_phase_info_t: 6 + 4 + 32 = 42 bytes


def geometria():
    p = np.zeros((SPK_ROWS * SPK_COLS, 3))
    for ch in range(SPK_ROWS * SPK_COLS):
        r, c = divmod(ch, SPK_COLS)
        p[ch, 0] = c * SPK_X_PITCH_MM + (SPK_ROW_DX_MM if (r & 1) else 0.0)
        p[ch, 1] = r * SPK_Y_PITCH_MM
    p[:, 0] -= p[:, 0].mean()
    p[:, 1] -= p[:, 1].mean()
    return p


# ------------------------------------------------------------------ transporte

def bajar(host, timeout=90.0):
    url = f"http://{host}/fase"
    print(f"bajando {url} ...", flush=True)
    with urllib.request.urlopen(url, timeout=timeout) as r:
        raw = r.read()
    if raw[:1] == b"{":
        d = json.loads(raw.decode())
        sys.exit(f"la placa contesto: {d.get('message', d)}\n"
                 f"  estado: {d.get('state')}  progreso: {d.get('progress')}/"
                 f"{d.get('n_spk')}")
    return raw


def parsear(raw):
    n = struct.calcsize(HDR)
    if len(raw) < n:
        sys.exit("archivo demasiado corto: no entra ni la cabecera")
    f = struct.unpack_from(HDR, raw, 0)
    n_spk, n_pts, n_ch, state, prog, _rsv, total = f[:7]
    freqs = list(f[7:7 + n_pts])
    cuerpo = raw[n:]
    if len(cuerpo) < total:
        sys.exit(f"la transferencia quedo TRUNCADA: {len(cuerpo)} de {total} "
                 f"bytes. Volve a pedirla; el modo no hay que correrlo de nuevo.")
    v = np.frombuffer(cuerpo[:total], dtype="<i2").reshape(n_spk, n_pts, n_ch, 2)
    db = v[..., 0].astype(np.float64) / 100.0
    ph = (v[..., 1].astype(np.uint16).astype(np.float64)
          * (2.0 * math.pi / 65536.0))
    return dict(n_spk=n_spk, n_pts=n_pts, n_ch=n_ch, state=state, progress=prog,
                freqs=freqs, db=db, ph=ph)


def a_json(d):
    return {"n_spk": d["n_spk"], "n_pts": d["n_pts"], "n_ch": d["n_ch"],
            "state": int(d["state"]), "progress": int(d["progress"]),
            "freqs": d["freqs"], "db": d["db"].tolist(), "ph": d["ph"].tolist()}


def de_json(j):
    j = dict(j)
    j["db"] = np.array(j["db"], dtype=np.float64)
    j["ph"] = np.array(j["ph"], dtype=np.float64)
    return j


# -------------------------------------------------------------------- angulos

def env(x):
    """Envuelve a (-pi, pi]."""
    return (x + math.pi) % (2 * math.pi) - math.pi


def media_circular(a, peso=None, eje=0):
    z = np.exp(1j * a)
    if peso is not None:
        z = z * peso
    m = z.sum(axis=eje)
    return np.angle(m), np.abs(m) / (np.abs(peso).sum(axis=eje) if peso is not None
                                     else a.shape[eje])


def desvio_circular(a):
    """Desvio circular en radianes. Con R -> 1 tiende al desvio comun."""
    R = np.abs(np.exp(1j * np.asarray(a)).mean())
    R = min(max(R, 1e-12), 1.0)
    return math.sqrt(-2.0 * math.log(R))


# ------------------------------------------------------------------- analisis

def analizar(d, mic, mic2=None, c_ms=C_MS, mic_ch=0, umbral_db=20.0, verbose=True):
    p = geometria()
    n_spk, n_pts = d["n_spk"], d["n_pts"]
    f = np.array(d["freqs"], dtype=float)
    db = d["db"][:, :, mic_ch]                 # [spk][pt]
    ph = d["ph"][:, :, mic_ch]

    # --- que medidas son creibles ---
    # El piso se estima como el percentil 10 de TODO el barrido: con 448 medidas
    # y un muro sano, el 10% mas bajo es piso y no senal. Es robusto a que haya
    # unos pocos parlantes mudos, que es justamente lo que se quiere detectar.
    # QUE MEDIDAS SON CREIBLES. Dos puertas, y las dos hacen falta.
    #
    # (1) RANGO DINAMICO, frecuencia por frecuencia. La referencia es la MEDIANA
    #     de los 32 parlantes a esa frecuencia, y se descarta lo que este mas de
    #     `umbral_db` por debajo. La mediana y no el percentil bajo: un percentil
    #     bajo estima el piso a partir de la propia senal, asi que con 32
    #     parlantes PAREJOS —que es el caso sano— el "piso" cae adentro de la
    #     senal y no sobrevive nada. Y no el maximo, porque un solo canal
    #     anomalo y fuerte arrastraria el umbral. Por frecuencia y no global
    #     porque la respuesta de la sala varia 20 dB o mas entre 250 Hz y 5 kHz.
    #
    # (2) PISO ABSOLUTO. Una puerta relativa siempre deja pasar "algo": si TODO
    #     es ruido, el ruido de arriba pasa el umbral del ruido de abajo y el
    #     analisis sale igual, con tablas perfectamente convincentes hechas de
    #     nada. Paso el 2026-09-03 con los amplificadores apagados: -138 dBFS de
    #     piso del ADC, y el informe reportaba "dispersion 143°, NO SUMA" como
    #     si fuera una medida. -110 dBFS estan ~25 dB sobre el piso del AK5538 y
    #     muy por debajo de cualquier medicion util.
    ref = np.median(db, axis=0, keepdims=True)          # [1][pt]
    viva = (db > ref - umbral_db) & (db > -110.0)
    piso = float(np.min(ref))

    # --- fase geometrica ---
    dist = np.linalg.norm(p[:, None, :] - np.asarray(mic)[None, None, :], axis=2)  # [spk][1]
    geom = 2 * math.pi * f[None, :] * (dist / (c_ms * 1000.0))                     # [spk][pt]
    psi = env(ph - geom)

    # --- termino comun: promedio circular sobre los parlantes vivos ---
    delta = np.zeros_like(psi)
    disp = np.full(n_pts, np.nan)
    n_viva = viva.sum(axis=0)
    for j in range(n_pts):
        sel = viva[:, j]
        if sel.sum() < 4:
            continue
        comun = np.angle(np.exp(1j * psi[sel, j]).sum())
        delta[:, j] = env(psi[:, j] - comun)
        disp[j] = desvio_circular(delta[sel, j])

    # --- retardo por parlante: suma coherente sobre una grilla de tau ---
    # Sin regresion: la fase esta envuelta y una regresion sobre angulos
    # envueltos no significa nada. Rango +-1 ms = +-343 mm, paso 1 us.
    taus = np.arange(-1000e-6, 1000e-6 + 1e-9, 1e-6)
    tau_k = np.zeros(n_spk)
    inv_k = np.zeros(n_spk, dtype=bool)
    res_k = np.full(n_spk, np.nan)
    coh_k = np.zeros(n_spk)
    for k in range(n_spk):
        sel = viva[k]
        if sel.sum() < 4:
            continue
        e = np.exp(1j * delta[k, sel])                       # [pts]
        w = np.exp(-2j * math.pi * np.outer(taus, f[sel]))   # [taus][pts]
        score = np.abs((e[None, :] * w).sum(axis=1))
        i = int(np.argmax(score))
        tau_k[k] = taus[i]
        coh_k[k] = score[i] / sel.sum()                      # 1 = perfecto
        r = env(delta[k, sel] - 2 * math.pi * f[sel] * taus[i])
        # Una inversion es pi a TODAS las frecuencias; un retardo crece con f.
        # Si el residuo esta centrado en pi, el parlante esta invertido.
        if abs(np.angle(np.exp(1j * r).sum())) > math.pi / 2:
            inv_k[k] = True
            r = env(r - math.pi)
        res_k[k] = desvio_circular(r)

    # Nivel tipico de lo que se dio por vivo. Es el numero que decide si vale la
    # pena mirar el resto.
    nivel_med = float(np.median(db[viva])) if viva.any() else -999.0
    out = dict(nivel_med=nivel_med, f=f, db=db, ph=ph, viva=viva, piso=piso,
               delta=delta, disp=disp,
               tau=tau_k, inv=inv_k, res=res_k, coh=coh_k, n_viva=n_viva,
               p=p, mic=np.asarray(mic, dtype=float), c_ms=c_ms, mic_ch=mic_ch)

    # --- control cruzado con el segundo microfono ---
    if mic2 is not None and d["n_ch"] > 1:
        ph2 = d["ph"][:, :, 1]
        db2 = d["db"][:, :, 1]
        # La misma regla que para el microfono 1, y con su PROPIA mediana: los
        # dos canales pueden tener preamplificadores de ganancia distinta, y
        # compartir umbral haria que el mas bajo quedara entero afuera.
        ref2 = np.median(db2, axis=0, keepdims=True)
        v2 = (db2 > ref2 - umbral_db) & (db2 > -110.0) & viva
        d1 = np.linalg.norm(p[:, None, :] - np.asarray(mic)[None, None, :], axis=2)
        d2 = np.linalg.norm(p[:, None, :] - np.asarray(mic2)[None, None, :], axis=2)
        esperado = 2 * math.pi * f[None, :] * ((d2 - d1) / (c_ms * 1000.0))
        medido = env(ph2 - ph)
        err = env(medido - esperado)
        # El retardo del pipeline es el MISMO para los dos microfonos (misma
        # trama TDM), asi que no hay termino comun que sacar. Lo unico que puede
        # quedar es una diferencia fija de los preamplificadores.
        sesgo = np.angle(np.exp(1j * err[v2]).sum()) if v2.sum() else np.nan
        out["cruce_err"] = env(err - sesgo)
        out["cruce_sesgo"] = sesgo
        out["cruce_viva"] = v2
        out["cruce_rms"] = (desvio_circular(env(err[v2] - sesgo))
                            if v2.sum() >= 4 else float("nan"))
    return out


# --------------------------------------------------------------------- salida

def mm(rad, f_hz, c_ms=C_MS):
    """De radianes a milimetros de camino equivalente, a esa frecuencia."""
    return rad * c_ms * 1000.0 / (2 * math.pi * f_hz)


def informe(a, d, mapa=False):
    f = a["f"]
    n_spk = a["db"].shape[0]

    # LA PUERTA. Todo lo que sigue son cuentas sobre angulos, y las cuentas
    # sobre angulos de ruido dan angulos: numeros con la forma exacta de una
    # medicion y ningun contenido. Cortar aca, y no matizar mas abajo, es la
    # unica forma de que nadie lea una tabla que no significa nada.
    if a["nivel_med"] < -90.0 or a["viva"].sum() < 0.25 * a["viva"].size:
        print()
        print("  " + "!" * 74)
        print("  NO HAY SENAL. No se analiza nada.")
        print("  " + "!" * 74)
        print(f"  nivel tipico de lo que llego: {a['nivel_med']:.1f} dBFS")
        print(f"  medidas por encima del umbral: {int(a['viva'].sum())} "
              f"de {a['viva'].size}")
        print()
        print("  Eso es el piso del conversor, no un muro sonando. Revisar, en")
        print("  este orden:")
        print("    1. los amplificadores:  curl -X POST http://<ip>/amps "
              "-d '{\"on\":true}'")
        print("    2. el microfono y su alimentacion — J9 es entrada de LINEA y")
        print("       NO tiene bias: un electret enchufado directo no suena")
        print("    3. la ganancia de entrada:  POST /ingain")
        print("    4. el volumen maestro:      POST /volume")
        print()
        print("  Y despues volver a correr el modo 22: son otros 3 minutos.")
        print()
        print("  (Este corte existe porque el 2026-09-03 una corrida con los amps")
        print("   apagados produjo un informe entero —dispersiones, retardos,")
        print("   polaridades invertidas— a partir de -138 dBFS de ruido. Un")
        print("   umbral relativo siempre deja pasar 'algo'.)")
        return
    print()
    print(f"  microfono en x {a['mic'][0]:+.0f}  y {a['mic'][1]:+.0f}  "
          f"z {a['mic'][2]:.0f} mm     c = {a['c_ms']:.1f} m/s     "
          f"entrada c{a['mic_ch']}")
    print(f"  umbral: se descarta lo que este >20 dB por debajo de la mediana "
          f"de su frecuencia")
    print(f"  utilizables: {int(a['viva'].sum())} de {a['viva'].size} medidas")

    # ---------- 1. diagnostico del muro ----------
    print()
    print("  " + "=" * 74)
    print("  1. DIAGNOSTICO — nivel de cada parlante en el microfono")
    print("  " + "=" * 74)
    med = np.median(a["db"][a["viva"]]) if a["viva"].any() else float("nan")
    nivel = np.array([np.median(a["db"][k][a["viva"][k]]) if a["viva"][k].any()
                      else -999.0 for k in range(n_spk)])
    mudos = [k for k in range(n_spk) if a["viva"][k].sum() < 4]
    flojos = [k for k in range(n_spk) if k not in mudos and nivel[k] < med - 6]
    fuertes = [k for k in range(n_spk) if k not in mudos and nivel[k] > med + 6]
    print(f"  nivel mediano del muro: {med:.1f} dBFS")
    print(f"  dispersion entre parlantes: {np.std(nivel[nivel > -900]):.1f} dB")
    if mudos:
        print(f"  !! MUDOS o por debajo del piso: {', '.join(f'ch{k}' for k in mudos)}")
    if flojos:
        print(f"  !  mas de 6 dB por debajo de la mediana: "
              f"{', '.join(f'ch{k}' for k in flojos)}")
    if fuertes:
        print(f"  !  mas de 6 dB por encima: {', '.join(f'ch{k}' for k in fuertes)}")
    if not (mudos or flojos or fuertes):
        print("  los 32 parlantes suenan y ninguno se va mas de 6 dB. OK")

    # ---------- 2. arribo en fase ----------
    print()
    print("  " + "=" * 74)
    print("  2. ARRIBO EN FASE — cuanto se desalinean los 32, frecuencia a frecuencia")
    print("  " + "=" * 74)
    print(f"  {'Hz':>6}{'vivos':>7}{'dispersion':>13}{'':>4}{'= camino':>11}"
          f"   {'veredicto'}")
    print("  " + "-" * 74)
    for j, fj in enumerate(f):
        if np.isnan(a["disp"][j]):
            print(f"  {int(fj):>6}{a['n_viva'][j]:>7}       (menos de 4 parlantes vivos)")
            continue
        dd = a["disp"][j]
        camino = mm(dd, fj)
        # El criterio no es arbitrario: media longitud de onda es cancelacion.
        # Un cuarto de longitud de onda (pi/2) ya cuesta 3 dB en la suma.
        v = ("OK" if dd < math.pi / 8 else
             "aceptable" if dd < math.pi / 4 else
             "MALO — pierde suma" if dd < math.pi / 2 else
             "NO SUMA")
        print(f"  {int(fj):>6}{a['n_viva'][j]:>7}{math.degrees(dd):>10.1f}°"
              f"{'':>4}{camino:>9.1f} mm   {v}")
    print()
    print("  pi/8 = 22,5° cuesta ~0,2 dB en la suma de 32; pi/4 = 45° cuesta ~1 dB;")
    print("  pi/2 = 90° cuesta ~3 dB y arriba de eso la suma se empieza a cancelar.")

    # ---------- 3. alcanza un retardo? ----------
    print()
    print("  " + "=" * 74)
    print("  3. ¿ALCANZA UN RETARDO POR PARLANTE?")
    print("  " + "=" * 74)
    ok = ~np.isnan(a["res"])
    if ok.sum() < 4:
        print("  muy pocos parlantes utilizables.")
    else:
        print(f"  {'ch':>4}{'tau(us)':>10}{'= mm':>8}{'polaridad':>11}"
              f"{'residuo':>10}{'coherencia':>12}   ")
        print("  " + "-" * 62)
        for k in range(n_spk):
            if not ok[k]:
                print(f"  {k:>4}       (sin datos utilizables)")
                continue
            print(f"  {k:>4}{a['tau'][k] * 1e6:>+10.1f}"
                  f"{a['tau'][k] * a['c_ms'] * 1000:>+8.1f}"
                  f"{('INVERTIDA' if a['inv'][k] else 'normal'):>11}"
                  f"{math.degrees(a['res'][k]):>9.1f}°"
                  f"{a['coh'][k]:>12.2f}")
        r = a["res"][ok]
        print()
        print(f"  residuo mediano tras quitar el mejor retardo: "
              f"{math.degrees(np.median(r)):.1f}°")
        if np.median(r) < math.pi / 8:
            print("  -> Es RETARDO PURO. Un solo numero por parlante lo corrige,")
            print("     y esa correccion vale para cualquier foco y cualquier")
            print("     frecuencia de la banda medida.")
        elif np.median(r) < math.pi / 4:
            print("  -> Mayormente retardo. Un numero por parlante corrige casi todo;")
            print("     lo que queda es del orden de 1 dB en la suma.")
        else:
            print("  -> NO es retardo puro. Ningun retardo unico lo corrige, asi que")
            print("     una calibracion por retardos tiene un techo. Mirar el mapa")
            print("     (--mapa): si el residuo se concentra en las frecuencias")
            print("     bajas, es la resonancia del driver (loopback_debug.md §28)")
            print("     y no hay nada que el pipeline pueda hacer.")
        inv = np.where(a["inv"] & ok)[0]
        if len(inv):
            print()
            print(f"  POLARIDAD INVERTIDA en {len(inv)} parlante(s): "
                  f"{', '.join(f'ch{k}' for k in inv)}")
            print("  (pi de desfase a TODAS las frecuencias — un retardo mal puesto")
            print("   daria un desfase que crece con f, y no es el caso)")

    # ---------- 4. control cruzado ----------
    if "cruce_rms" in a:
        print()
        print("  " + "=" * 74)
        print("  4. CONTROL CRUZADO CON EL SEGUNDO MICROFONO")
        print("  " + "=" * 74)
        print("  La diferencia de fase entre c0 y c1 tiene que ser la que impone la")
        print("  diferencia de camino, para los 32 parlantes y las 14 frecuencias.")
        print("  Los dos microfonos comparten trama TDM, asi que no hay retardo de")
        print("  pipeline que restar: es una prueba de respuesta conocida.")
        print()
        print(f"  sesgo fijo entre los dos canales: "
              f"{math.degrees(a['cruce_sesgo']):+.1f}°  (preamplificadores)")
        print(f"  dispersion despues de sacarlo:    "
              f"{math.degrees(a['cruce_rms']):.1f}°")
        if a["cruce_rms"] < math.pi / 6:
            print("  -> COINCIDE. El signo y la escala de la fase estan bien, y la")
            print("     posicion de los microfonos tambien.")
        else:
            print("  -> NO coincide. Antes de creerle a nada de lo de arriba,")
            print("     revisar: la posicion real de los dos microfonos, que esten")
            print("     en c0 y c1 y no al reves, y la separacion.")

    if mapa:
        print()
        print("  " + "=" * 74)
        print("  MAPA: desviacion de fase de cada parlante, en grados")
        print("  " + "=" * 74)
        print("       " + "".join(f"{int(x):>6}" for x in f))
        for k in range(n_spk):
            fila = ""
            for j in range(len(f)):
                if not a["viva"][k, j]:
                    fila += "     ."
                else:
                    fila += f"{math.degrees(a['delta'][k, j]):>+6.0f}"
            r, c = divmod(k, SPK_COLS)
            print(f"  {k:>2} {'ABCD'[r]}{c}{fila}")
        print()
        print("  '.' = por debajo del umbral. Una COLUMNA entera grande es una")
        print("  frecuencia problematica (sala, resonancia); una FILA entera grande")
        print("  es un parlante problematico.")


def comparar(fa, fb, mic, c_ms):
    a = analizar(de_json(json.load(open(fa))), mic, c_ms=c_ms, verbose=False)
    b = analizar(de_json(json.load(open(fb))), mic, c_ms=c_ms, verbose=False)
    print()
    print("  Dos corridas del mismo muro. Sin esto no se sabe si una diferencia")
    print("  significa algo: la sala tambien cambia entre corrida y corrida.")
    print()
    print(f"  {'Hz':>6}{'disp A':>10}{'disp B':>10}{'diferencia':>13}")
    print("  " + "-" * 39)
    for j, fj in enumerate(a["f"]):
        da, dbv = a["disp"][j], b["disp"][j]
        if np.isnan(da) or np.isnan(dbv):
            continue
        print(f"  {int(fj):>6}{math.degrees(da):>9.1f}°{math.degrees(dbv):>9.1f}°"
              f"{math.degrees(dbv - da):>+12.1f}°")
    ok = ~np.isnan(a["res"]) & ~np.isnan(b["res"])
    dt = (b["tau"] - a["tau"])[ok] * 1e6
    print()
    print(f"  retardo por parlante: cambio {np.std(dt):.1f} us RMS entre corridas")
    print(f"  = {np.std(dt) * 1e-6 * C_MS * 1000:.2f} mm de camino")
    print()
    if np.std(dt) < 5:
        print("  Los retardos REPITEN. Lo medido es del equipo, no de la sala.")
    else:
        print("  Los retardos NO repiten. Lo medido tiene mucha sala adentro:")
        print("  medir mas cerca, con mas absorcion, y sin nadie caminando.")


def _sintetico(mic, tau=None, inv=None, mudos=(), curva=None,
               t0=1.2e-3, ruido_rad=0.0, freqs=None, mic2=None, seed=0):
    """Arma una corrida como la que devuelve la placa, con la respuesta conocida.

        phi_k(f) = 2*pi*f*(d_k/c + t0 + tau_k) + pi*inv_k + curva_k(f)

    t0 es el retardo del pipeline: comun a los 32 y por lo tanto invisible para
    el analisis, que lo saca con el promedio circular. Esta puesto justamente
    para verificar que se va."""
    rng = np.random.default_rng(seed)
    p = geometria()
    n = len(p)
    f = np.array(freqs if freqs is not None else
                 [250, 315, 400, 500, 630, 800, 1000,
                  1250, 1600, 2000, 2500, 3150, 4000, 5000], dtype=float)
    tau = np.zeros(n) if tau is None else np.asarray(tau, dtype=float)
    inv = np.zeros(n, bool) if inv is None else np.asarray(inv, dtype=bool)

    def ph_de(m):
        d = np.linalg.norm(p - np.asarray(m, float), axis=1) / (C_MS * 1000.0)
        a = 2 * math.pi * f[None, :] * (d[:, None] + t0 + tau[:, None])
        a = a + np.where(inv, math.pi, 0.0)[:, None]
        if curva is not None:
            a = a + curva(f)[None, :] * np.ones((n, 1))
        if ruido_rad:
            a = a + rng.normal(0, ruido_rad, a.shape)
        return a % (2 * math.pi)

    n_ch = 8
    db = np.full((n, len(f), n_ch), -40.0)
    ph = np.zeros((n, len(f), n_ch))
    ph[:, :, 0] = ph_de(mic)
    if mic2 is not None:
        ph[:, :, 1] = ph_de(mic2)
    else:
        db[:, :, 1] = -150.0
    db[:, :, 2:] = -150.0
    for k in mudos:
        db[k, :, :] = -150.0
    return dict(n_spk=n, n_pts=len(f), n_ch=n_ch, state=2, progress=n,
                freqs=[int(x) for x in f], db=db, ph=ph)


def autoprueba():
    """Verifica el analisis contra casos donde la respuesta se conoce.

    Existe porque este script convierte angulos en veredictos, y un veredicto
    sin prueba es una opinion. Y porque la unica forma de saber que el analisis
    esta bien ANTES de tener microfonos es construirle la realidad."""
    mic = [0.0, 0.0, 1000.0]
    mic2 = [-250.0, 0.0, 1000.0]
    n = SPK_ROWS * SPK_COLS
    fallos = 0

    def chk(nombre, cond, detalle=""):
        nonlocal fallos
        if not cond:
            fallos += 1
        print(f"  {'ok  ' if cond else 'FALLA'} {nombre:52s} {detalle}")

    print("\n  1. muro perfecto")
    a = analizar(_sintetico(mic), mic)
    chk("dispersion despreciable en las 14 frecuencias",
        np.nanmax(a["disp"]) < math.radians(1),
        f"max {math.degrees(np.nanmax(a['disp'])):.3f}°")
    chk("ningun retardo espurio", np.max(np.abs(a["tau"])) < 3e-6,
        f"max {np.max(np.abs(a['tau'])) * 1e6:.2f} us")
    chk("ninguna polaridad invertida", not a["inv"].any())
    chk("el t0 del pipeline (1,2 ms) no se cuela", np.max(np.abs(a["tau"])) < 3e-6)

    print("\n  2. un parlante INVERTIDO (ch13)")
    inv = np.zeros(n, bool); inv[13] = True
    a = analizar(_sintetico(mic, inv=inv), mic)
    chk("detecta ch13 invertido", bool(a["inv"][13]))
    chk("no marca ningun otro", int(a["inv"].sum()) == 1,
        f"marcados: {list(np.where(a['inv'])[0])}")
    chk("no lo confunde con un retardo", abs(a["tau"][13]) < 20e-6,
        f"tau {a['tau'][13] * 1e6:+.1f} us")

    print("\n  3. un parlante con RETARDO de +150 us (ch5)")
    tau = np.zeros(n); tau[5] = 150e-6
    a = analizar(_sintetico(mic, tau=tau), mic)
    rel = a["tau"] - np.median(a["tau"])
    chk("recupera +150 us en ch5", abs(rel[5] - 150e-6) < 8e-6,
        f"{rel[5] * 1e6:+.1f} us")
    chk("no lo confunde con una inversion", not a["inv"][5])
    chk("los otros 31 quedan en cero",
        np.max(np.abs(np.delete(rel, 5))) < 8e-6)

    print("\n  4. retardos al azar de 30 us RMS en los 32")
    rng = np.random.default_rng(3)
    tau = rng.normal(0, 30e-6, n)
    a = analizar(_sintetico(mic, tau=tau), mic)
    rel = a["tau"] - a["tau"].mean()
    chk("recupera los 32 retardos", np.std(rel - (tau - tau.mean())) < 8e-6,
        f"error {np.std(rel - (tau - tau.mean())) * 1e6:.1f} us RMS")
    chk("la dispersion CRECE con la frecuencia",
        a["disp"][-1] > 3 * a["disp"][0],
        f"{math.degrees(a['disp'][0]):.1f}° -> "
        f"{math.degrees(a['disp'][-1]):.1f}°")
    chk("residuo chico: dictamina RETARDO PURO",
        np.median(a["res"]) < math.pi / 8,
        f"{math.degrees(np.median(a['res'])):.1f}°")

    print("\n  5. defecto que NO es retardo (fase que se curva)")
    # Un exceso de fase que crece con log(f): ningun retardo unico lo endereza.
    tau5 = rng.normal(0, 10e-6, n)
    curva_k = rng.normal(0, 1.0, n)
    d5 = _sintetico(mic, tau=tau5)
    fv = np.array(d5["freqs"], float)
    d5["ph"][:, :, 0] = (d5["ph"][:, :, 0]
                         + curva_k[:, None] * np.log(fv / 250.0)[None, :]) % (2 * math.pi)
    a = analizar(d5, mic)
    chk("residuo grande: NO es retardo puro",
        np.median(a["res"]) > math.pi / 8,
        f"{math.degrees(np.median(a['res'])):.1f}°")

    print("\n  6. tres parlantes mudos (ch2, ch17, ch31)")
    a = analizar(_sintetico(mic, mudos=(2, 17, 31)), mic)
    vivos = a["viva"].sum(axis=1)
    chk("los tres quedan fuera", all(vivos[k] == 0 for k in (2, 17, 31)))
    chk("los otros 29 siguen adentro",
        sum(1 for k in range(n) if vivos[k] > 0) == n - 3)
    chk("no inventa dispersion", np.nanmax(a["disp"]) < math.radians(1))

    print("\n  7. control cruzado con el segundo microfono a 250 mm")
    a = analizar(_sintetico(mic, tau=tau, mic2=mic2), mic, mic2)
    chk("las dos posiciones son coherentes",
        a["cruce_rms"] < math.radians(3),
        f"{math.degrees(a['cruce_rms']):.2f}°")
    b = analizar(_sintetico(mic, tau=tau, mic2=mic2), mic, [-400.0, 0.0, 1000.0])
    chk("y si se le miente la posicion, PROTESTA",
        b["cruce_rms"] > math.radians(6),
        f"{math.degrees(b['cruce_rms']):.1f}° con mic2 mal puesto")

    print("\n  8. ruido de fase de 10° encima de todo")
    a = analizar(_sintetico(mic, tau=tau, ruido_rad=math.radians(10), seed=9), mic)
    rel = a["tau"] - a["tau"].mean()
    chk("los retardos sobreviven al ruido",
        np.std(rel - (tau - tau.mean())) < 15e-6,
        f"error {np.std(rel - (tau - tau.mean())) * 1e6:.1f} us RMS")

    print()
    if fallos:
        print(f"  {fallos} comprobacion(es) fallaron.")
        sys.exit(1)
    print("  Todas las comprobaciones dan lo esperado.")


def main():
    ap = argparse.ArgumentParser(
        description="Fase de cada parlante en el punto de escucha, por frecuencia.",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("archivo", nargs="*", help="JSON ya bajado")
    ap.add_argument("--host")
    ap.add_argument("-o", "--out")
    ap.add_argument("--mic", type=float, nargs=3, default=[0.0, 0.0, 1000.0],
                    metavar=("X", "Y", "Z"),
                    help="posicion del microfono 1 en mm (default 0 0 1000)")
    ap.add_argument("--mic2", type=float, nargs=3, default=None,
                    metavar=("X", "Y", "Z"),
                    help="posicion del microfono 2; activa el control cruzado")
    ap.add_argument("--cal", help="cal.json de calfase.py: toma de ahi las "
                                  "posiciones AJUSTADAS de los microfonos, que "
                                  "son mejores que la cinta metrica")
    ap.add_argument("--ch", type=int, default=0, help="entrada del ADC a analizar")
    ap.add_argument("--c", type=float, default=C_MS)
    ap.add_argument("--umbral", type=float, default=20.0,
                    help="rango dinamico en dB: se descarta lo que este mas de "
                         "esto por debajo de la mediana de su frecuencia "
                         "(default 20)")
    ap.add_argument("--mapa", action="store_true", help="el mapa parlante x frecuencia")
    ap.add_argument("--comparar", nargs=2, metavar=("A.json", "B.json"))
    ap.add_argument("--autoprueba", action="store_true",
                    help="verifica el analisis con casos de respuesta conocida")
    args = ap.parse_args()

    if args.autoprueba:
        autoprueba()
        return

    mic, mic2 = list(args.mic), args.mic2
    if args.cal:
        sys.path.insert(0, "tools")
        import calfase
        cd = json.load(open(args.cal))
        mic = list(calfase.analizar(cd, args.c, mic=1)["m"])
        print(f"  microfono 1 tomado de {args.cal}: "
              f"{mic[0]:+.0f} {mic[1]:+.0f} {mic[2]:.0f} mm")
        if calfase.n_mics(cd) >= 2:
            mic2 = list(calfase.analizar(cd, args.c, mic=2)["m"])
            print(f"  microfono 2: {mic2[0]:+.0f} {mic2[1]:+.0f} {mic2[2]:.0f} mm")

    if args.comparar:
        comparar(args.comparar[0], args.comparar[1], mic, args.c)
        return

    if args.host:
        d = parsear(bajar(args.host))
        if args.out:
            json.dump(a_json(d), open(args.out, "w"))
            print(f"guardado en {args.out}")
    elif args.archivo:
        d = de_json(json.load(open(args.archivo[0])))
    else:
        ap.error("hace falta --host, un archivo, o --comparar")

    print(f"\n  {d['n_spk']} parlantes x {d['n_pts']} frecuencias x "
          f"{d['n_ch']} entradas   ({d['freqs'][0]}-{d['freqs'][-1]} Hz)")
    informe(analizar(d, mic, mic2, args.c, args.ch, args.umbral), d, args.mapa)


if __name__ == "__main__":
    main()
