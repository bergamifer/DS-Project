#!/usr/bin/env python3
"""
Separa el retardo PROPIO de cada parlante del camino geometrico hasta el microfono.

    python3 tools/calfase.py --host 192.168.100.70 -o A.json   # medir y guardar
    python3 tools/calfase.py A.json                            # analizar lo guardado
    python3 tools/calfase.py --comparar A.json B.json          # LA PRUEBA

El AUTOFOCUS (modo 11) mide, parlante por parlante, el pico de la correlacion de
un chirp contra un microfono. Ese numero mezcla tres cosas:

    TOF_k  =  t0  +  tau_k  +  |p_k - m| / c
              |       |         |
              |       |         camino geometrico (se puede calcular)
              |       retardo PROPIO del parlante  <-- lo que buscamos
              instante de disparo (comun a todos)

Como las posiciones p_k de los 32 parlantes son conocidas, se ajustan por minimos
cuadrados las CUATRO incognitas (m, t0) --posicion del microfono y offset de
disparo-- sobre las 32 mediciones. El RESIDUO de ese ajuste es tau_k.

32 ecuaciones, 4 incognitas: 28 grados de libertad. Y no hace falta saber donde
se puso el microfono, lo resuelve el ajuste.

-------------------------------------------------------------------------------
LA PRUEBA QUE DECIDE SI LA CALIBRACION ES REAL

Un ajuste con 28 grados de libertad SIEMPRE devuelve residuos. Que existan no
prueba nada: pueden ser el parlante, o pueden ser una reflexion de la sala y un
error de geometria que el ajuste no supo absorber.

La unica forma de distinguirlos es medir desde DOS posiciones de microfono y
comparar:

    tau_k iguales   -> es una propiedad del parlante, y GENERALIZA.
    tau_k distintos -> es sala o geometria, y NO hay calibracion.

Por eso existe --comparar, y por eso este script no escribe nada en la EEPROM:
un tau_k de una sola corrida no esta validado.

-------------------------------------------------------------------------------
LIMITES, dichos de entrada

- La correlacion de un chirp da RETARDO DE GRUPO: un escalar por parlante, no
  fase contra frecuencia. Si los drivers difieren en la FORMA de su fase
  --probable cerca de la resonancia de 125-180 Hz que aparecio en
  loopback_debug.md 28-- un retardo unico no lo arregla.
- Vale en la banda del chirp del AUTOFOCUS: 500 Hz - 6 kHz.
- La velocidad del sonido se toma fija. A 20 C son 343 m/s y sube ~0,6 m/s por
  grado; 10 C de error son ~1,7 % de escala, que el ajuste absorbe casi todo en
  la distancia al microfono. Se puede pasar con --c.
- La sala sesga el pico de la correlacion. Medir cerca y con absorcion.
- El ajuste ENCOGE tau. Con 4 parametros libres, parte del patron de tau se
  absorbe en la posicion del microfono y en t0: en la prueba sintetica, 6,0 us
  de dispersion real se leen como 4,3. El tau recuperado es un piso, no el
  valor exacto. No cambia el veredicto --lo que decide es si se REPITE-- pero
  si alguien compara contra una medicion directa, la diferencia es esa.

Ver el issue #21 del repositorio. La logica del veredicto esta verificada
contra datos sinteticos: `python3 tools/calfase.py --autoprueba`.
"""

import argparse
import json
import math
import sys
import urllib.request

import numpy as np

# --- geometria del muro, espejo de Beam_InitTxGeometry() en main.c ---
SPK_ROWS = 4
SPK_COLS = 8
SPK_X_PITCH_MM = 80.0
SPK_Y_PITCH_MM = 160.0
SPK_ROW_DX_MM = 40.0        # las filas 2 y 4 corridas: patron de ladrillo

FS_HZ = 48000.0
Q8 = 256.0                  # unidades de tof_q8 por muestra
C_MS = 343.0                # velocidad del sonido, m/s a 20 C
Q_MIN = 100                 # CAL_Q_MIN del firmware: pico/promedio x10


def geometria():
    """Las 32 posiciones en mm, centradas — igual que el firmware."""
    p = np.zeros((SPK_ROWS * SPK_COLS, 3))
    for ch in range(SPK_ROWS * SPK_COLS):
        r, c = divmod(ch, SPK_COLS)
        p[ch, 0] = c * SPK_X_PITCH_MM + (SPK_ROW_DX_MM if (r & 1) else 0.0)
        p[ch, 1] = r * SPK_Y_PITCH_MM
        p[ch, 2] = 0.0
    p[:, 0] -= p[:, 0].mean()
    p[:, 1] -= p[:, 1].mean()
    return p


def bajar(host, timeout=20.0):
    url = f"http://{host}/cal"
    print(f"bajando {url} ...", flush=True)
    with urllib.request.urlopen(url, timeout=timeout) as r:
        d = json.load(r)
    if not d.get("ok"):
        sys.exit(f"la placa contesto: {d.get('message', d)}")
    return d


def ajustar(p_mm, tof_smp, usar, c_ms, iters=60):
    """
    Gauss-Newton sobre (mx, my, mz, t0).

        f_k = t0 + (fs/c) * |p_k - m|      [muestras]

    Devuelve (m_mm, t0_muestras, residuos_muestras, n_iter).
    Sin scipy a proposito: son 4 parametros y la jacobiana es analitica.
    """
    k = FS_HZ / (c_ms * 1000.0)          # muestras por mm de camino

    # Arranque: el microfono al frente del centro del array, a la distancia que
    # sugiere la dispersion de los TOF. Un arranque malo puede caer en el
    # minimo espejado detras del muro, que es el unico riesgo real de este
    # ajuste — por eso z0 arranca positivo y grande.
    m = np.array([0.0, 0.0, 1000.0])
    t0 = float(tof_smp[usar].min()) - k * 1000.0

    idx = np.where(usar)[0]
    for it in range(iters):
        d = p_mm[idx] - m                        # (n,3)
        dist = np.linalg.norm(d, axis=1)
        dist = np.maximum(dist, 1e-6)            # el mic sobre un parlante
        f = t0 + k * dist
        r = tof_smp[idx] - f

        # J[:,0:3] = df/dm = k * (m - p)/|p-m| ;  J[:,3] = df/dt0 = 1
        J = np.empty((len(idx), 4))
        J[:, 0:3] = k * (-d) / dist[:, None]
        J[:, 3] = 1.0

        try:
            step, *_ = np.linalg.lstsq(J, r, rcond=None)
        except np.linalg.LinAlgError:
            break
        m = m + step[0:3]
        t0 = t0 + step[3]
        if np.max(np.abs(step)) < 1e-9:
            break

    d = p_mm - m
    dist = np.maximum(np.linalg.norm(d, axis=1), 1e-6)
    resid = tof_smp - (t0 + k * dist)
    return m, t0, resid, it + 1


def n_mics(d):
    """Cuantos microfonos trae la respuesta. El firmware >= v15 devuelve los dos
    del banco en la misma lista, marcados con "mic"."""
    return max((c.get("mic", 1) for c in d["channels"]), default=1)


def analizar(d, c_ms, mic=1):
    p = geometria()
    canales = [c for c in d["channels"] if c.get("mic", 1) == mic]
    n = len(canales)
    if n != SPK_ROWS * SPK_COLS:
        print(f"aviso: la placa devolvio {n} canales para el microfono {mic} y "
              f"la geometria tiene {SPK_ROWS * SPK_COLS}. Se usan los primeros "
              f"{min(n, len(p))}.")

    tof = np.zeros(len(p))
    qual = np.zeros(len(p), dtype=int)
    sign = np.zeros(len(p), dtype=int)
    visto = np.zeros(len(p), dtype=bool)
    for c in canales:
        k = c["ch"]
        if k < len(p):
            tof[k] = c["tof_q8"] / Q8
            qual[k] = c["quality"]
            sign[k] = c["sign"]
            visto[k] = True

    usar = visto & (qual >= Q_MIN) & (tof > 0)
    if usar.sum() < 6:
        sys.exit(f"solo {usar.sum()} canales con calidad >= {Q_MIN}: "
                 "no alcanza para ajustar 4 incognitas. Revisar el microfono.")

    m, t0, resid, nit = ajustar(p, tof, usar, c_ms)

    # De muestras a las unidades que se entienden
    us = resid / FS_HZ * 1e6
    mm = resid / FS_HZ * c_ms * 1000.0

    rms_us = float(np.sqrt(np.mean(us[usar] ** 2)))
    return dict(p=p, tof=tof, qual=qual, sign=sign, usar=usar, visto=visto,
                m=m, t0=t0, resid=resid, us=us, mm=mm, rms_us=rms_us, nit=nit,
                mic=mic, estado=d.get("state", "?"), ok_count=d.get("ok_count", 0))


def informe(a, c_ms, con_cola=True):
    m = a["m"]
    print()
    print(f"estado del scan: {a['estado']}   canales solidos: {int(a['usar'].sum())}"
          f"/{int(a['visto'].sum())} (calidad >= {Q_MIN})")
    print(f"ajuste: {a['nit']} iteraciones, c = {c_ms:.1f} m/s")
    print(f"microfono ajustado:  x {m[0]:+7.1f}   y {m[1]:+7.1f}   z {m[2]:7.1f}  mm")
    print(f"                     distancia al centro del muro: "
          f"{np.linalg.norm(m):.0f} mm")
    print(f"offset de disparo t0: {a['t0']:.2f} muestras")
    print()
    print(f"  RMS del residuo: {a['rms_us']:.2f} us  "
          f"= {a['rms_us'] * c_ms / 1000.0:.2f} mm de camino")
    print()
    print("  ch  fila col   TOF(smp)  cal  pol   tau(us)   tau(mm)")
    print("  --  ---- ---   --------  ---  ---   -------   -------")
    for k in range(len(a["p"])):
        r, c = divmod(k, SPK_COLS)
        if not a["visto"][k]:
            print(f"  {k:2d}   {r}    {c}      --       --        (sin dato)")
            continue
        flag = "" if a["usar"][k] else "  <- descartado"
        print(f"  {k:2d}   {r}    {c}   {a['tof'][k]:8.2f} {a['qual'][k]:4d} "
              f"{a['sign'][k]:+3d}  {a['us'][k]:+8.2f}  {a['mm'][k]:+8.2f}{flag}")
    print()
    print("  tau es el residuo del ajuste: lo que le sobra o le falta a ese")
    print("  parlante DESPUES de restarle el camino geometrico.")
    print()
    if con_cola:
        print("  UNA SOLA CORRIDA NO PRUEBA NADA. Repetir con el microfono en otro")
        print("  lugar y comparar:   python3 tools/calfase.py --comparar A.json B.json")


def comparar(fa, fb, c_ms):
    """Dos corridas SEPARADAS, con el microfono movido a mano entre una y otra."""
    a = analizar(json.load(open(fa)), c_ms)
    b = analizar(json.load(open(fb)), c_ms)
    _veredicto(a, b, fa, fb, c_ms)


def comparar_mics(d, c_ms, sep_esperada_mm=None):
    """Los DOS microfonos del banco, de la MISMA corrida.

    Es la misma prueba que --comparar y es mejor por tres motivos, todos por
    construccion y ninguno por cuidado del operador:

      - Mismo t0. Los dos microfonos oyen los MISMOS chirps, asi que el instante
        de disparo no es una incognita que cada corrida estima por su cuenta.
      - Misma sala y misma temperatura. Entre dos corridas separadas por minutos
        cambia la velocidad del sonido y, si alguien entro a mover el microfono,
        tambien las reflexiones.
      - Nadie toca nada. El banco es fijo; mover el microfono ERA la prueba, y
        era tambien la principal fuente de error de la prueba.

    Y trae una comprobacion que la version secuencial no puede tener: la
    separacion entre los dos microfonos se conoce con una cinta metrica. El
    ajuste la estima sin saberla, asi que comparar las dos es una prueba de
    respuesta conocida sobre TODA la cadena — geometria, Q8, mapeo y arnes.
    """
    _veredicto(analizar(d, c_ms, mic=1), analizar(d, c_ms, mic=2),
               "microfono 1 (c0)", "microfono 2 (c1)", c_ms,
               sep_esperada_mm=sep_esperada_mm, misma_corrida=True)


def _veredicto(a, b, fa, fb, c_ms, sep_esperada_mm=None, misma_corrida=False):
    amb = a["usar"] & b["usar"]
    if amb.sum() < 6:
        sys.exit("las dos corridas comparten menos de 6 canales solidos.")

    da, db = np.linalg.norm(a["m"]), np.linalg.norm(b["m"])
    print()
    print("=" * 66)
    if misma_corrida:
        print("  LA PRUEBA: los mismos chirps, oidos por los dos microfonos")
    else:
        print("  LA PRUEBA: los mismos parlantes, desde dos posiciones de microfono")
    print("=" * 66)
    print(f"  A: {fa}   microfono a {da:.0f} mm   RMS {a['rms_us']:.2f} us")
    print(f"  B: {fb}   microfono a {db:.0f} mm   RMS {b['rms_us']:.2f} us")
    sep = np.linalg.norm(a["m"] - b["m"])
    print(f"  separacion entre las dos posiciones: {sep:.0f} mm")

    # --- prueba de respuesta conocida ---
    if sep_esperada_mm:
        err = sep - sep_esperada_mm
        tol = max(25.0, 0.10 * sep_esperada_mm)
        print(f"  medida con cinta:                    {sep_esperada_mm:.0f} mm"
              f"   error {err:+.0f} mm")
        if abs(err) <= tol:
            print("  -> el ajuste RECUPERA la separacion real. La geometria del")
            print("     firmware, los Q8 y el mapeo de canales quedan validados.")
        else:
            print(f"  -> !! FUERA DE TOLERANCIA (+-{tol:.0f} mm). El ajuste no")
            print("     recupera una distancia que se conoce, asi que tampoco hay")
            print("     por que creerle los tau. Revisar, en este orden: que los")
            print("     microfonos esten en c0 y c1 y no al reves; la separacion")
            print("     real; y que SPK_*_PITCH_MM de este script coincida con")
            print("     Beam_InitTxGeometry() del firmware.")
    if sep < 300:
        print("  AVISO: menos de 300 mm entre los dos puntos. La prueba es DEBIL,")
        print("         y conviene entender por que: dos microfonos cercanos ven")
        print("         casi la MISMA reflexion, asi que la reflexion aparece")
        print("         igual en los dos y el criterio la confunde con el")
        print("         parlante. O sea: a poca separacion, 'COINCIDEN' es facil")
        print("         de conseguir y significa menos.")
        print("         Separar los microfonos 500 mm en vez de 250 arregla esto")
        print("         y ademas hace medible el contraste por debajo de 1,2 kHz,")
        print("         donde a 250 mm el segundo microfono todavia cae DENTRO")
        print("         del lobulo principal. Ver docs/banco_dos_microfonos.md.")
    print()
    print("  ch   tau A(us)  tau B(us)   diferencia")
    print("  --   ---------  ---------   ----------")
    dif = a["us"] - b["us"]
    for k in np.where(amb)[0]:
        print(f"  {k:2d}   {a['us'][k]:+8.2f}   {b['us'][k]:+8.2f}   {dif[k]:+8.2f}")

    rms_dif = float(np.sqrt(np.mean(dif[amb] ** 2)))
    disp_a = float(np.std(a["us"][amb]))
    disp_b = float(np.std(b["us"][amb]))
    disp = 0.5 * (disp_a + disp_b)

    print()
    print(f"  dispersion de tau dentro de cada corrida: {disp:.2f} us")
    print(f"  RMS de la diferencia entre corridas:      {rms_dif:.2f} us")
    print(f"  = {rms_dif * c_ms / 1000.0:.2f} mm de camino")
    print()

    # El criterio: para que tau sea una propiedad del parlante, tiene que
    # REPETIRSE. Si la diferencia entre corridas es del tamano de la propia
    # dispersion, lo medido es ruido con forma de calibracion.
    if disp < 0.05:
        print("  VEREDICTO: tau es despreciable en las dos corridas "
              f"({disp:.2f} us = {disp * c_ms / 1000.0:.3f} mm).")
        print("  No hay nada que corregir, y eso ES un resultado: los parlantes")
        print("  no se distinguen entre si dentro de la resolucion del metodo.")
    elif rms_dif < 0.5 * disp:
        print("  VEREDICTO: las dos corridas COINCIDEN.")
        print("  tau se repite desde dos puntos distintos, asi que es una")
        print("  propiedad del parlante y generaliza a cualquier foco.")
        print("  La calibracion vale. Promedio de las dos:")
        print()
        prom = 0.5 * (a["us"] + b["us"])
        for k in np.where(amb)[0]:
            q8 = prom[k] * 1e-6 * FS_HZ * Q8
            print(f"    ch {k:2d}   tau {prom[k]:+7.2f} us   "
                  f"({prom[k] * c_ms / 1000.0:+6.2f} mm)   q8 {q8:+8.1f}")
    else:
        print("  VEREDICTO: las dos corridas NO coinciden.")
        print("  La diferencia entre posiciones es del orden de la dispersion,")
        print("  asi que lo medido no es el parlante: es sala, geometria, o el")
        print("  microfono moviendose. NO cargar esto como calibracion.")
        print()
        print("  Que probar: mas cerca, con mas absorcion, y verificando que la")
        print("  geometria del array coincida con la del firmware.")


def autoprueba():
    """
    Verifica la logica del veredicto contra datos sinteticos, donde tau se
    conoce. Existe porque el criterio de --comparar es un umbral, y un umbral
    sin prueba es una opinion.

    Lo que separa los casos es el cociente rms_dif/dispersion: cuando tau es
    real vale 0,1-0,3 (se repite desde las dos posiciones), y cuando es ruido
    vale ~1,2 (dos ruidos independientes dan raiz de 2 veces la dispersion).
    El umbral de 0,5 cae limpio en el medio.
    """
    p = geometria()
    k = FS_HZ / (C_MS * 1000.0)

    def corrida(mic, t0, tau_us, ruido_us, seed):
        rng = np.random.default_rng(seed)
        d = np.linalg.norm(p - np.array(mic, dtype=float), axis=1)
        tof = t0 + k * d + (tau_us + rng.normal(0, ruido_us, len(p))) * 1e-6 * FS_HZ
        return {"ok": True, "state": "done", "ok_count": len(p), "n_channels": len(p),
                "channels": [{"ch": i, "tof_q8": int(round(tof[i] * Q8)),
                              "sign": 1, "quality": 180} for i in range(len(p))]}

    casos = [
        ("tau real 6 us, sin ruido",     np.random.default_rng(7).normal(0, 6, len(p)), 0.0,  "COINCIDEN"),
        ("tau real 6 us, ruido 0,3 us",  np.random.default_rng(7).normal(0, 6, len(p)), 0.3,  "COINCIDEN"),
        ("tau real 2 us, ruido 0,3 us",  np.random.default_rng(7).normal(0, 2, len(p)), 0.3,  "COINCIDEN"),
        ("SIN tau, solo ruido de 1 us",  np.zeros(len(p)),                              1.0,  "NO COINCIDEN"),
        ("SIN tau, solo ruido de 3 us",  np.zeros(len(p)),                              3.0,  "NO COINCIDEN"),
        ("SIN tau y sin ruido",          np.zeros(len(p)),                              0.0,  "DESPRECIABLE"),
    ]

    print("  caso                            disp     rms_dif  ratio  veredicto      esperado")
    print("  " + "-" * 78)
    fallos = 0
    for nombre, tau, ruido, esperado in casos:
        a = analizar(corrida([250, -100, 900], 500.0, tau, ruido, 1), C_MS)
        b = analizar(corrida([-400, 200, 1600], 500.0, tau, ruido, 2), C_MS)
        amb = a["usar"] & b["usar"]
        dif = a["us"] - b["us"]
        rms_dif = float(np.sqrt(np.mean(dif[amb] ** 2)))
        disp = 0.5 * (float(np.std(a["us"][amb])) + float(np.std(b["us"][amb])))
        if disp < 0.05:
            got = "DESPRECIABLE"
        elif rms_dif < 0.5 * disp:
            got = "COINCIDEN"
        else:
            got = "NO COINCIDEN"
        ok = "ok" if got == esperado else "FALLA"
        if got != esperado:
            fallos += 1
        print(f"  {nombre:30s} {disp:6.2f}   {rms_dif:7.2f}  {rms_dif / disp if disp else 0:5.2f}"
              f"  {got:14s} {esperado:14s} {ok}")
    # --- el banco de dos microfonos, sintetico ---
    # Se arma UNA respuesta con los dos microfonos, como la que devuelve el
    # firmware >= v15, y se verifica que el ajuste recupere la separacion que se
    # uso para generarla. Es la prueba de respuesta conocida del banco, corrida
    # sobre datos donde la respuesta se conoce de veras.
    print()
    print("  banco de dos microfonos (sintetico)")
    print("  " + "-" * 78)
    mic_a = [0.0, 0.0, 1000.0]
    fallos_banco = 0
    for sep_real in (250.0, 500.0):
        mic_b = [mic_a[0] - sep_real, mic_a[1], mic_a[2]]
        tau = np.random.default_rng(11).normal(0, 3, len(p))
        ra = corrida(mic_a, 500.0, tau, 0.3, 21)
        rb = corrida(mic_b, 500.0, tau, 0.3, 22)
        d2 = {"ok": True, "state": "done", "ok_count": len(p),
              "n_channels": 2 * len(p),
              "channels": ([dict(c, mic=1) for c in ra["channels"]]
                           + [dict(c, mic=2) for c in rb["channels"]])}
        a2 = analizar(d2, C_MS, mic=1)
        b2 = analizar(d2, C_MS, mic=2)
        sep_med = float(np.linalg.norm(a2["m"] - b2["m"]))
        err = sep_med - sep_real
        ok = abs(err) <= max(25.0, 0.10 * sep_real)
        if not ok:
            fallos_banco += 1
        print(f"  separacion real {sep_real:5.0f} mm  ->  ajustada "
              f"{sep_med:6.1f} mm   error {err:+6.1f}   "
              f"{'ok' if ok else 'FALLA'}")
        assert n_mics(d2) == 2, "n_mics no detecto los dos microfonos"

    print()
    if fallos or fallos_banco:
        print(f"  {fallos + fallos_banco} caso(s) no dieron lo esperado.")
        sys.exit(1)
    print("  Los seis casos dan lo esperado, y el banco recupera la separacion.")
    print("  Nota: el ajuste encoge tau — 6,0 us de dispersion real se leen")
    print("  como ~4,3. El veredicto no depende de eso, pero el valor si.")


def main():
    ap = argparse.ArgumentParser(
        description="Retardo propio de cada parlante, separado de la geometria.")
    ap.add_argument("archivo", nargs="?", help="JSON de una corrida guardada")
    ap.add_argument("--host", help="IP o nombre mDNS del nodo (baja GET /cal)")
    ap.add_argument("-o", "--salida", help="guardar la respuesta cruda")
    ap.add_argument("--comparar", nargs=2, metavar=("A", "B"),
                    help="comparar dos corridas — la prueba de aceptacion")
    ap.add_argument("--c", type=float, default=C_MS,
                    help=f"velocidad del sonido en m/s (default {C_MS})")
    ap.add_argument("--autoprueba", action="store_true",
                    help="verifica la logica del veredicto con datos sinteticos")
    ap.add_argument("--mic", type=int, default=None,
                    help="informar SOLO este microfono (1 o 2) en vez de "
                         "comparar los dos")
    ap.add_argument("--sep", type=float, default=250.0,
                    help="separacion FISICA entre los dos microfonos del banco, "
                         "en mm (default 250). Es la prueba de respuesta "
                         "conocida: el ajuste tiene que recuperarla.")
    args = ap.parse_args()

    if args.autoprueba:
        autoprueba()
        return

    if args.comparar:
        comparar(args.comparar[0], args.comparar[1], args.c)
        return

    if args.host:
        d = bajar(args.host)
        if args.salida:
            json.dump(d, open(args.salida, "w"), indent=1)
            print(f"guardado en {args.salida}")
    elif args.archivo:
        d = json.load(open(args.archivo))
    else:
        ap.error("hace falta --host, un archivo, o --comparar")

    nm = n_mics(d)
    if args.mic:
        informe(analizar(d, args.c, mic=args.mic), args.c)
    elif nm >= 2:
        # Los dos microfonos del banco: se informa cada uno y se compara.
        # Comparar es el punto — un solo informe no valida nada, y con dos
        # entradas la comparacion sale gratis en la misma corrida.
        for k in (1, 2):
            print(f"\n########## microfono {k} ##########")
            informe(analizar(d, args.c, mic=k), args.c, con_cola=False)
        comparar_mics(d, args.c, sep_esperada_mm=args.sep)
    else:
        informe(analizar(d, args.c), args.c)
        print()
        print("  Esta corrida trae UN solo microfono. Con firmware >= v15 y el")
        print("  segundo microfono en c1, la prueba de dos posiciones sale sola")
        print("  en la misma corrida: ver docs/banco_dos_microfonos.md")


if __name__ == "__main__":
    main()
