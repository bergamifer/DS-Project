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

def analizar(d, mic, mic2=None, c_ms=C_MS, mic_ch=0, umbral_db=20.0, verbose=True,
             orden=None, signo=+1):
    """`orden[k]` = posicion FISICA del canal k. Sin esto, un arnes cruzado se
    lee como si fueran defectos de los parlantes: el error de geometria no
    desaparece, se disfraza de tau_k."""
    p = geometria()
    if orden is not None:
        p = p[np.asarray(orden)]
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
    geom = signo * 2 * math.pi * f[None, :] * (dist / (c_ms * 1000.0))             # [spk][pt]
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
        esperado = signo * 2 * math.pi * f[None, :] * ((d2 - d1) / (c_ms * 1000.0))
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


# ============================================================================
# UBICAR: donde esta el microfono, y que parlante es cual
# ============================================================================
#
# Todo el analisis de fase se apoya en dos cosas que hay que SABER y no suponer:
# donde esta el microfono, y que posicion fisica le corresponde a cada canal del
# firmware. Si cualquiera de las dos esta mal, la fase geometrica que se resta
# esta mal, y lo que queda no es el defecto del parlante sino el error de uno
# mismo.
#
# LA CUENTA. Para una posicion candidata m, la fase medida del parlante k a la
# frecuencia f deberia ser
#
#     phi_k(f) = 2*pi*f*d_k(m)/c  +  comun(f)
#
# donde comun(f) —el retardo del pipeline y el origen del emisor— es el MISMO
# para los 32. Entonces, si m es la correcta, al restarle a cada parlante su
# fase geometrica los 32 residuos quedan APILADOS, y
#
#     coherencia(f) = | sum_k exp(i*(phi_k(f) - 2*pi*f*d_k(m)/c)) | / N
#
# vale 1. Si m esta mal, los residuos se abren y la coherencia cae. Maximizarla
# sobre una grilla de m no necesita conocer `comun` en absoluto, que es
# justamente lo que la hace robusta: el retardo del pipeline se cancela solo.
#
# LA AMBIGUEDAD, dicha antes de usarla. Las diferencias de camino entre
# parlantes llegan a ~71 mm para un microfono a 1 m. A 5 kHz eso es mas de una
# longitud de onda y aparecen maximos falsos; a 1 kHz es un quinto de longitud
# de onda y no hay ninguno. Por eso el ajuste grueso usa SOLO 250-1000 Hz, y las
# frecuencias altas entran despues, a refinar, cuando la posicion ya esta a
# menos de un centimetro.

def _dist(p, m):
    return np.linalg.norm(p - np.asarray(m, float), axis=1)


def _coherencia(ph, viva, f, p, m, orden, signo=+1):
    """Coherencia media sobre las frecuencias, para la posicion m y el mapeo
    `orden` (orden[k] = indice de la posicion fisica del canal k)."""
    d = _dist(p[orden], m) / (C_MS * 1000.0)          # [spk] segundos
    tot = 0.0
    n = 0
    for j in range(len(f)):
        sel = viva[:, j]
        if sel.sum() < 6:
            continue
        z = np.exp(1j * (ph[sel, j] - signo * 2 * math.pi * f[j] * d[sel]))
        tot += abs(z.sum()) / sel.sum()
        n += 1
    return tot / n if n else 0.0


def _coh_lote(ph, viva, f, pos, M, signo=+1):
    """Coherencia para MUCHAS posiciones candidatas a la vez. `pos` son las
    posiciones fisicas ya reordenadas, [K,3]; M es [n,3]. Vectorizado porque la
    grilla gruesa son decenas de miles de puntos y en Python puro tarda
    minutos."""
    d = np.linalg.norm(pos[None, :, :] - M[:, None, :], axis=2) / (C_MS * 1000.0)
    tot = np.zeros(len(M))
    n = 0
    for j in range(len(f)):
        sel = viva[:, j]
        if sel.sum() < 6:
            continue
        e = np.exp(1j * ph[sel, j])[None, :] * np.exp(-signo * 2j * math.pi * f[j] * d[:, sel])
        tot += np.abs(e.sum(axis=1)) / sel.sum()
        n += 1
    return tot / n if n else tot


def _ajustar_pos(ph, viva, f, p, orden, f_max=1000.0,
                 caja=((-700, 700), (-500, 500), (400, 1800)), paso=40.0, signo=+1):
    """Grilla gruesa y despues dos refinamientos. Sin gradiente a proposito: la
    funcion tiene maximos locales por el envolvimiento de la fase, y un
    gradiente cae en el primero que encuentra."""
    usar = f <= f_max
    fj, phj, vj = f[usar], ph[:, usar], viva[:, usar]
    pos = p[orden]
    (x0, x1), (y0, y1), (z0, z1) = caja
    mejor, best = None, -1.0
    for _ in range(3):
        xs = np.arange(x0, x1 + 1e-9, paso)
        ys = np.arange(y0, y1 + 1e-9, paso)
        zs = np.arange(z0, z1 + 1e-9, paso)
        M = np.stack(np.meshgrid(xs, ys, zs, indexing="ij"), -1).reshape(-1, 3)
        c = _coh_lote(phj, vj, fj, pos, M, signo)
        i = int(np.argmax(c))
        best, mejor = float(c[i]), tuple(M[i])
        x0, x1 = mejor[0] - paso, mejor[0] + paso
        y0, y1 = mejor[1] - paso, mejor[1] + paso
        z0, z1 = mejor[2] - paso, mejor[2] + paso
        paso /= 5.0
    return np.array(mejor, float), best


def signo_de_fase(d, ch, orden=None, umbral_db=20.0):
    """Con que signo traduce esta cadena un retardo en fase.

    No es una sutileza de convenio: con el signo cambiado, restar la geometria
    SUMA el doble en vez de cancelar, y sintetizar el array lo desenfoca justo
    donde deberia enfocar. Se decide midiendo — se ajusta la posicion del
    microfono con los dos signos y gana el que da mas coherencia, porque solo
    uno de los dos puede apilar los 32 residuos."""
    p = geometria()
    if orden is not None:
        p = p[np.asarray(orden)]
    f = np.array(d["freqs"], float)
    db = d["db"][:, :, ch]
    ph = d["ph"][:, :, ch]
    ref = np.median(db, axis=0, keepdims=True)
    viva = (db > ref - umbral_db) & (db > -110.0)
    ident = np.arange(len(p))
    m_p, c_p = _ajustar_pos(ph, viva, f, p, ident, signo=+1)
    m_n, c_n = _ajustar_pos(ph, viva, f, p, ident, signo=-1)
    return (+1, m_p, c_p, c_n) if c_p >= c_n else (-1, m_n, c_n, c_p)


def ubicar(d, ch, verbose=True, umbral_db=20.0, signo=+1):
    """Ajusta la posicion del microfono `ch` y decide, PAR POR PAR, si los dos
    canales estan intercambiados."""
    p = geometria()
    f = np.array(d["freqs"], float)
    db = d["db"][:, :, ch]
    ph = d["ph"][:, :, ch]
    ref = np.median(db, axis=0, keepdims=True)
    viva = (db > ref - umbral_db) & (db > -110.0)

    ident = np.arange(len(p))
    pares = ident ^ 1                      # 0<->1, 2<->3, ... 30<->31

    m_i, c_i = _ajustar_pos(ph, viva, f, p, ident, signo=signo)
    m_p, c_p = _ajustar_pos(ph, viva, f, p, pares, signo=signo)

    if verbose:
        print(f"\\n  --- entrada c{ch} ---")
        print(f"  cableado DIRECTO   : microfono en "
              f"({m_i[0]:+7.0f}, {m_i[1]:+7.0f}, {m_i[2]:7.0f}) mm   "
              f"coherencia {c_i:.3f}")
        print(f"  PARES intercambiados: microfono en "
              f"({m_p[0]:+7.0f}, {m_p[1]:+7.0f}, {m_p[2]:7.0f}) mm   "
              f"coherencia {c_p:.3f}")

    orden = pares if c_p > c_i else ident
    m = m_p if c_p > c_i else m_i

    # --- par por par, con la posicion ya fijada ---
    # Un veredicto global esconde el caso mas probable de todos: que se hayan
    # cruzado ALGUNOS y no todos. Con m fija, cada par se decide solo.
    detalle = []
    orden_fin = orden.copy()
    for i in range(0, len(p), 2):
        a_ = orden.copy()
        b_ = orden.copy()
        b_[i], b_[i + 1] = b_[i + 1], b_[i]
        ca = _coherencia(ph, viva, f, p, m, a_)
        cb = _coherencia(ph, viva, f, p, m, b_)
        vivos = int(viva[i].sum() + viva[i + 1].sum())
        if vivos < 8:
            detalle.append((i, None, ca, cb, vivos))
            continue
        if cb > ca:
            orden_fin = b_ if np.array_equal(orden_fin, orden) else orden_fin
            orden_fin[i], orden_fin[i + 1] = orden_fin[i + 1], orden_fin[i]
        detalle.append((i, cb > ca, ca, cb, vivos))

    c_fin = _coherencia(ph, viva, f, p, m, orden_fin)

    # Incertidumbre por eje: cuanto se puede mover el microfono sin que la
    # coherencia caiga un 1%. NO es cosmetico — con el microfono sobre el eje
    # del muro, z apenas cambia las DIFERENCIAS de camino entre parlantes, que
    # es lo unico que el ajuste ve, asi que z queda mucho peor determinado que
    # x e y. Decirlo evita que alguien lea "z = 992" como si fueran milimetros.
    inc = []
    usar = f <= 1000.0
    pos = p[orden_fin]
    for eje in range(3):
        delta = np.zeros((81, 3))
        delta[:, eje] = np.linspace(-200, 200, 81)
        c = _coh_lote(ph[:, usar], viva[:, usar], f[usar], pos, m[None, :] + delta)
        ok = np.where(c >= c.max() * 0.99)[0]
        inc.append(float(delta[ok[-1], eje] - delta[ok[0], eje]) / 2.0)

    return dict(m=m, orden=orden_fin, base=orden, coh=c_fin, inc=np.array(inc),
                coh_ident=c_i, coh_pares=c_p, detalle=detalle, viva=viva)


# ============================================================================
# SINTETIZAR: que hace el array entero, sin volver a medir
# ============================================================================
#
# El barrido de fase mide la respuesta COMPLEJA de cada parlante hasta el
# microfono: amplitud y fase, parlante por parlante y frecuencia por frecuencia.
# Eso incluye la sala —reflexiones, modos, todo— porque el microfono no
# distingue de donde viene lo que le llega.
#
# Y la sala es LINEAL. Entonces, si al parlante k se le pone un retardo t_k, lo
# que llega al microfono es A_k * exp(i*(phi_k - 2*pi*f*t_k)), y lo que mide el
# microfono con los 32 sonando es la SUMA. O sea que se puede calcular, exacto,
# que va a medir el array para CUALQUIER foco, sin volver a emitir un solo tono.
#
# POR QUE IMPORTA ACA. El perfil medido moviendo el foco de a 50 mm tarda 65
# segundos POR FRECUENCIA, y en una sala chica sale sucio: el campo reverberante
# compite con el directo y el perfil de un microfono puede tener el maximo en
# otro lado. Sintetizar da las 14 frecuencias de una, desde una sola corrida de
# 3,5 minutos, y ademas permite hacer algo que fisicamente no se puede: comparar
# el array real contra el MISMO array sin sala.
#
# LO QUE NO ARREGLA: si la sala mete un maximo donde no corresponde, la sintesis
# lo reproduce fielmente, porque es cierto. Lo que da no es "el haz limpio" sino
# "lo que va a medir ese microfono", que es justamente lo que hay que saber.

def sintetizar(d, mic_ch, focos, orden=None, c_ms=C_MS, umbral_db=20.0, signo=+1):
    """Nivel en el microfono `mic_ch` para cada foco de `focos` (Nx3, en mm).

    `orden[k]` = posicion fisica del canal k (arnes cruzado). Devuelve
    [n_focos][n_pts] en dB relativos al maximo de cada frecuencia."""
    p = geometria()
    if orden is not None:
        p = p[np.asarray(orden)]
    f = np.array(d["freqs"], float)
    db = d["db"][:, :, mic_ch]
    ph = d["ph"][:, :, mic_ch]
    ref = np.median(db, axis=0, keepdims=True)
    viva = (db > ref - umbral_db) & (db > -110.0)
    A = np.where(viva, 10 ** (db / 20.0), 0.0)
    H = A * np.exp(1j * ph)                       # [spk][pt] respuesta compleja

    focos = np.asarray(focos, float)
    out = np.zeros((len(focos), len(f)))
    for i, fo in enumerate(focos):
        # Los mismos retardos que Beam_UpdateTxFocus: el mas lejano dispara
        # primero, asi todos los frentes llegan juntos al foco.
        dk = np.linalg.norm(p - fo[None, :], axis=1) / (c_ms * 1000.0)
        t = dk.max() - dk
        for j in range(len(f)):
            # `signo` es el convenio con que la cadena de medicion traduce un
            # RETARDO en fase. No se supone: se determina de los datos con
            # signo_de_fase(), porque equivocarlo no da un error chico sino el
            # foco al reves — el array desenfoca justo donde deberia enfocar.
            z = H[:, j] * np.exp(signo * 2j * math.pi * f[j] * t)
            out[i, j] = 20 * math.log10(max(abs(z.sum()), 1e-12))
    return out, f


def _sintetico(mic, tau=None, inv=None, mudos=(), curva=None,
               t0=1.2e-3, ruido_rad=0.0, freqs=None, mic2=None, seed=0,
               orden=None):
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
    # orden[k] = posicion FISICA del canal k. Sirve para simular un arnes
    # cruzado y comprobar que `ubicar` lo descubre.
    orden = np.arange(n) if orden is None else np.asarray(orden)

    def ph_de(m):
        d = np.linalg.norm(p[orden] - np.asarray(m, float), axis=1) / (C_MS * 1000.0)
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
    ap.add_argument("--pares", action="store_true",
                    help="el arnes esta cruzado de a pares (0<->1, 2<->3, ...)")
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
    orden = (np.arange(SPK_ROWS * SPK_COLS) ^ 1) if args.pares else None
    informe(analizar(d, mic, mic2, args.c, args.ch, args.umbral, orden=orden),
            d, args.mapa)


if __name__ == "__main__":
    main()
