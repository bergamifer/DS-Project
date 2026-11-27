#!/usr/bin/env python3
"""banco_fase2.py — el banco de la fase 2 del plan DS12, en una DS11, de una vez.

POR QUÉ EXISTE
--------------
La fase 2 (identidad de placa y autoarranque, docs/ds12_firmware_plan.md) está
escrita y compilada, pero nunca corrió en una placa. Su prueba es una lista de
cosas para mirar a ojo en el log serie y en /status. Este script las mira por
uno, las compara con lo que el plan dice que tiene que aparecer, y deja todo
—incluido el `reset_rsr` crudo de la medida M8— en un archivo de registro.

LO QUE NUNCA HACE
-----------------
No enciende los amplificadores, no arma ni desarma el autoarranque, no cambia el
modo y no llama a POST /reset (que es un reset de FÁBRICA y deja la placa en el
modo 0, el del acople). Sólo lee /status, sube firmware y reinicia. Igual: con
los parlantes DESENCHUFADOS, como pide el plan.

QUÉ PRUEBA (cada paso se pide con su opción)
  siempre       /status: STM32 vivo, protocolo 23 en los dos chips, placa DS11,
                sin `board_mismatch`, `v5a` y `usb_vbus` en null (la DS11 no los
                mide), y qué firmware tiene contra el del repo.
  --stlink      `make reset` por ST-Link: tiene que volver con RESET: SFT.
  --ota         `make ota` del mismo firmware de la DS11: vuelve con SFT y con
                el fw_id del repo. Si en ese momento el autoarranque tiene los
                amplificadores encendidos (lo armó una persona, no el script),
                además es la prueba "OTA a mitad de un autoarranque": tiene que
                aparecer AUTOARRANQUE: … no cuenta como intento fallido.
  --barreras    el firmware de la DS12 tiene que rebotar:
                  1. `make ota BOARD=ds12` → lo frena el Makefile;
                  2. POST /firmware directo con el .bin de la DS12 → 409 del
                     ESP32, y la placa NO se reinicia.
                La tercera barrera (OTA_End del STM32) sólo se prueba con un
                ESP32 viejo que no chequea: eso queda a mano.
  --serie PORT  además lee el log de UART7 (el puerto del ST-Link, 115200) y
                después de cada reinicio verifica el banner: la línea PLACA, la
                línea RESET y el readback del AK5538.

USO
  tools/banco_fase2.py --host 192.168.100.77 --serie /dev/cu.usbmodem1103 --stlink --ota --barreras
  tools/banco_fase2.py --host 192.168.100.77            # sólo leer /status
  tools/banco_fase2.py --autoprueba                     # sin placa: prueba el script

  Sale 1 si hay alguna ❌. El registro queda en .ds12_audit/banco/.

AUTOPRUEBA
  Contra una placa FALSA (un servidor HTTP local que contesta como el ESP32) y
  contra banners de texto con respuesta conocida:
    - una placa sana da ✅; una con el jumper equivocado, el protocolo viejo,
      la placa DS12, el STM32 mudo o los amplificadores encendidos dan ❌;
    - un ESP32 que ACEPTA el firmware de la DS12 (contesta 200), o que contesta
      409 pero la placa igual se reinicia, da ❌;
    - un banner sin la línea PLACA da ❌, no ✅: que falte una línea no es que
      esté bien.
  Lo que la autoprueba NO cubre: el `make`, el puerto serie real y el
  ST-Link. Eso se ve en la primera corrida en el banco.
"""
import argparse
import datetime
import http.server
import json
import os
import re
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FW = os.path.join(RAIZ, "firmware")
BIN_DS12 = os.path.join(FW, "build_ds12", "ds12_beamforming.bin")

OK, INFO, AVISO, FALLA = "ok", "info", "aviso", "falla"
ICONO = {OK: "✅", INFO: "  ", AVISO: "⚠️ ", FALLA: "❌"}


def proto_del_repo():
    """El PROTO_VERSION que tiene que estar corriendo: el del repo."""
    h = os.path.join(FW, "Core", "Inc", "uart_protocol.h")
    m = re.search(r"#define\s+PROTO_VERSION\s+(\d+)", open(h).read())
    return int(m.group(1))


def git_id():
    """El fw_id que pone el Makefile: los 8 primeros del commit, en mayúscula."""
    try:
        return subprocess.run(["git", "-C", RAIZ, "rev-parse", "--short=8", "HEAD"],
                              capture_output=True, text=True).stdout.strip()[:8].upper()
    except OSError:
        return ""


# ---------------------------------------------------------------------------
# Registro: lo que se ve en pantalla y además todo lo crudo, a un archivo.
# ---------------------------------------------------------------------------
class Registro:
    def __init__(self, ruta=None):
        self.resultados = []
        self.f = open(ruta, "w") if ruta else None
        self.ruta = ruta

    def crudo(self, texto):
        if self.f:
            self.f.write(texto.rstrip("\n") + "\n")
            self.f.flush()

    def titulo(self, texto):
        print(f"\n{texto}")
        self.crudo(f"\n== {texto}")

    def __call__(self, nivel, texto):
        self.resultados.append((nivel, texto))
        print(f"  {ICONO[nivel]} {texto}")
        self.crudo(f"[{nivel}] {texto}")

    def fallas(self):
        return sum(1 for n, _ in self.resultados if n == FALLA)


# ---------------------------------------------------------------------------
# HTTP
# ---------------------------------------------------------------------------
def leer_status(host, timeout=10):
    """El /status como dict, o None si no contesta."""
    try:
        with urllib.request.urlopen(f"http://{host}/status", timeout=timeout) as r:
            return json.loads(r.read().decode("utf-8", "replace"))
    except (OSError, ValueError):
        return None


def post_binario(host, ruta, timeout=240):
    """POST /firmware con el archivo. Devuelve (código HTTP, cuerpo)."""
    datos = open(ruta, "rb").read()
    req = urllib.request.Request(f"http://{host}/firmware?id=BANCO000&build=0", data=datos, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", "replace")
    except OSError as e:
        return None, str(e)


def esperar_reinicio(host, uptime_antes, timeout, reg, paso=2.0):
    """Espera a que el STM32 vuelva con un uptime MENOR que el de antes.
    Devuelve el /status nuevo, o None si no volvió a tiempo."""
    t0 = time.time()
    while time.time() - t0 < timeout:
        time.sleep(paso)
        st = leer_status(host, timeout=5)
        if st and st.get("stm32") == "ok" and st.get("stm32_uptime_ms", 1 << 62) < uptime_antes:
            reg.crudo(f"volvió a los {time.time() - t0:.0f} s: {json.dumps(st)}")
            return st
    return None


# ---------------------------------------------------------------------------
# Lo que se verifica — funciones puras, para la autoprueba.
# ---------------------------------------------------------------------------
def analizar_status(st, proto, fw_repo=None):
    """Lista de (nivel, texto) sobre un /status. `st` None = no contestó."""
    R = []
    if st is None:
        return [(FALLA, "el ESP32 no contesta /status")]
    if st.get("stm32") != "ok":
        return [(FALLA, f"el ESP32 contesta pero el STM32 no (stm32 = {st.get('stm32')!r})")]
    pv, pe = st.get("proto_ver"), st.get("proto_esp")
    R.append((OK if pv == proto else FALLA,
              f"protocolo del STM32: {pv} (el repo: {proto})"
              + ("" if pv == proto else " — falta `make ota`")))
    R.append((OK if pe == proto else FALLA,
              f"protocolo del ESP32: {pe} (el repo: {proto})"
              + ("" if pe == proto else " — falta `make ota-esp`")))
    placa = st.get("board")
    R.append((OK if placa == "DS11" else FALLA, f"placa: {placa} (tiene que ser DS11)"))
    mm = st.get("board_mismatch")
    R.append((OK if mm is False else FALLA,
              "board_mismatch: " + ("no" if mm is False else f"{mm!r} — el jumper PE15 no coincide: "
                                     "amplificadores bloqueados")))
    for campo in ("v5a", "usb_vbus"):
        if campo not in st:
            R.append((FALLA, f"{campo}: no está en /status"))
        else:
            R.append((OK if st[campo] is None else AVISO,
                      f"{campo}: {json.dumps(st[campo])}"
                      + ("" if st[campo] is None else " — la DS11 no lo mide: tendría que ser null")))
    if st.get("amps"):
        R.append((AVISO if st.get("autostart") else FALLA,
                  "amplificadores ENCENDIDOS"
                  + (" (por el autoarranque)" if st.get("autostart") else
                     " — el banco es con los amplificadores apagados: apagalos vos")))
    R.append((INFO, f"reset: {st.get('reset')} (RSR=0x{st.get('reset_rsr')}) · "
                    f"autostart: {st.get('autostart')} · held: {st.get('autostart_held')}"))
    if fw_repo:
        fid = str(st.get("fw_id", "")).upper()
        R.append((OK if fid == fw_repo else INFO,
                  f"fw_id: {fid}" + (" = el commit del repo" if fid == fw_repo else
                                     f" (el repo está en {fw_repo}; puede ser otro build)")))
    return R


RX_PLACA = re.compile(r"PLACA: firmware (\S+), jumper PE15 = (\d) \(espera (\d)\)(.*)")
RX_RESET = re.compile(r"RESET: (\S+) \(RSR=0x([0-9A-Fa-f]{8})\)")
RX_AK = re.compile(r"readback R01=0x([0-9A-F]{2}) R02=0x([0-9A-F]{2}) R03=0x([0-9A-F]{2}) R04=0x([0-9A-F]{2})")


def analizar_banner(texto, reset_esperado=None, descuento_esperado=False):
    """Lista de (nivel, texto) sobre el log de un arranque. Que falte una
    línea es ❌: un banner vacío no puede pasar."""
    R = []
    m = RX_PLACA.search(texto)
    if not m:
        R.append((FALLA, "no apareció la línea PLACA:"))
    else:
        fw, nivel, espera, resto = m.group(1), m.group(2), m.group(3), m.group(4)
        bien = fw == "DS11" and nivel == "1" and espera == "1" and "(ok)" in resto
        R.append((OK if bien else FALLA, m.group(0).strip()))
    m = RX_RESET.search(texto)
    if not m:
        R.append((FALLA, "no apareció la línea RESET:"))
    else:
        tag, rsr = m.group(1), m.group(2).upper()
        if reset_esperado:
            R.append((OK if tag == reset_esperado else FALLA,
                      f"RESET: {tag} (esperado {reset_esperado}) · RSR=0x{rsr} ← M8"))
        else:
            R.append((INFO, f"RESET: {tag} · RSR=0x{rsr} ← M8"))
    m = RX_AK.search(texto)
    if not m:
        R.append((FALLA, "no apareció el readback del AK5538"))
    else:
        bien = m.group(2) == "31" and m.group(3) == "40" and m.group(4) == "02"
        R.append((OK if bien else FALLA,
                  f"AK5538: R02=0x{m.group(2)} R03=0x{m.group(3)} R04=0x{m.group(4)} (want 31/40/02)"))
    descontado = "no cuenta como intento fallido" in texto
    if descuento_esperado:
        R.append((OK if descontado else FALLA,
                  "el OTA a mitad del autoarranque " + ("NO sumó un intento fallido" if descontado
                                                        else "no dejó la línea de descuento")))
    elif descontado:
        R.append((INFO, "AUTOARRANQUE: descontó un intento (reinicio por software)"))
    if "SUSPENDIDO" in texto:
        R.append((AVISO, "AUTOARRANQUE: SUSPENDIDO — mirá el contador de intentos"))
    return R


def barrera_esp32(host, binario, reg, espera_reinicio=60):
    """POST directo del .bin de la DS12: tiene que dar 409 y la placa NO
    tiene que reiniciarse."""
    antes = leer_status(host)
    if not antes or antes.get("stm32") != "ok":
        reg(FALLA, "barrera 2: la placa no contesta antes de probar")
        return
    codigo, cuerpo = post_binario(host, binario)
    reg.crudo(f"POST /firmware (DS12) → {codigo} {cuerpo}")
    if codigo is None:
        # El ESP32 contesta el 409 apenas ve el bloque de identidad, a mitad de
        # la subida; si la conexión se corta antes de leer la respuesta, el
        # código se pierde. Lo que decide entonces es si la placa se reinició.
        reg(AVISO, f"barrera 2 (ESP32): no llegó la respuesta ({cuerpo[:80]}) — decide el reinicio, abajo")
    else:
        reg(OK if codigo == 409 else FALLA,
            f"barrera 2 (ESP32): HTTP {codigo} — {cuerpo.strip()[:120]}")
    # ¿Se reinició igual? Un rechazo que deja la placa reiniciada no es un rechazo.
    t0 = time.time()
    reinicio = None
    while time.time() - t0 < espera_reinicio:
        st = leer_status(host, timeout=5)
        if st and st.get("stm32") == "ok":
            if st.get("stm32_uptime_ms", 0) < antes.get("stm32_uptime_ms", 0):
                reinicio = st
                break
            if time.time() - t0 > min(10, espera_reinicio):
                break
        time.sleep(1)
    despues = reinicio or leer_status(host)
    if reinicio:
        reg(FALLA, f"barrera 2: la placa SE REINICIÓ después del rechazo (placa {reinicio.get('board')}, "
                   f"fw_id {reinicio.get('fw_id')})")
    elif despues and despues.get("fw_id") == antes.get("fw_id") and despues.get("board") == "DS11":
        reg(OK, f"barrera 2: sigue corriendo el mismo firmware ({despues.get('fw_id')}), sin reiniciar")
    else:
        reg(FALLA, f"barrera 2: después del rechazo /status dice {json.dumps(despues)[:160]}")


# ---------------------------------------------------------------------------
# El puerto serie, leído en un hilo aparte para no perder el banner.
# ---------------------------------------------------------------------------
class Serie:
    def __init__(self, puerto, reg):
        try:
            import serial  # pyserial
        except ImportError:
            sys.exit("banco_fase2: --serie necesita pyserial (pip install pyserial)")
        self.s = serial.Serial(puerto, 115200, timeout=0.2)
        self.lineas = []
        self.reg = reg
        self.vivo = True
        threading.Thread(target=self._leer, daemon=True).start()

    def _leer(self):
        resto = b""
        while self.vivo:
            resto += self.s.read(4096)
            while b"\n" in resto:
                linea, resto = resto.split(b"\n", 1)
                txt = linea.decode("utf-8", "replace").rstrip("\r")
                self.lineas.append((time.time(), txt))
                self.reg.crudo(f"serie| {txt}")

    def desde(self, t0):
        return "\n".join(t for ts, t in self.lineas if ts >= t0)


# ---------------------------------------------------------------------------
# El banco
# ---------------------------------------------------------------------------
def make(reg, *args, timeout=600):
    cmd = ["make", "-C", FW] + list(args)
    reg.crudo("$ " + " ".join(cmd))
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    reg.crudo(p.stdout + p.stderr)
    return p.returncode, p.stdout + p.stderr


def tras_reinicio(reg, a, serie, t0, antes, timeout, reset_esperado, descuento=False):
    st = esperar_reinicio(a.host, antes.get("stm32_uptime_ms", 1 << 62), timeout, reg)
    if not st:
        reg(FALLA, f"el STM32 no volvió en {timeout} s")
        return None
    reg(OK if st.get("reset") == reset_esperado else FALLA,
        f"/status después: reset = {st.get('reset')} (esperado {reset_esperado}), RSR=0x{st.get('reset_rsr')}")
    if serie:
        time.sleep(3)   # que termine de salir el banner
        for n, t in analizar_banner(serie.desde(t0), reset_esperado, descuento):
            reg(n, t)
    return st


def banco(a):
    os.makedirs(os.path.join(RAIZ, ".ds12_audit", "banco"), exist_ok=True)
    sello = datetime.datetime.now().strftime("%Y-%m-%d_%H%M")
    reg = Registro(os.path.join(RAIZ, ".ds12_audit", "banco", f"fase2_{re.sub(r'[^A-Za-z0-9.-]', '_', a.host)}_{sello}.log"))
    print("Banco de la fase 2 — con los parlantes DESENCHUFADOS. El script no toca los amplificadores.")
    proto, fw_repo = proto_del_repo(), git_id()
    serie = Serie(a.serie, reg) if a.serie else None

    reg.titulo(f"/status de {a.host}")
    st = leer_status(a.host)
    reg.crudo(json.dumps(st))
    R = analizar_status(st, proto, fw_repo)
    for n, t in R:
        reg(n, t)
    if st is None or st.get("stm32") != "ok":
        return reg
    if st.get("amps") and not st.get("autostart"):
        print("\nNo sigo: los amplificadores están encendidos a mano.")
        return reg

    if a.stlink:
        reg.titulo("make reset (ST-Link) → tiene que volver con SFT")
        t0 = time.time()
        rc, out = make(reg, "reset")
        if rc:
            reg(FALLA, "make reset falló (ver el registro) — ¿está enchufado el ST-Link?")
        else:
            st = tras_reinicio(reg, a, serie, t0, st, 60, "SFT") or st

    if a.ota:
        en_intento = bool(st.get("amps") and st.get("autostart"))
        reg.titulo("make ota del firmware de la DS11"
                   + (" — A MITAD DE UN AUTOARRANQUE" if en_intento else ""))
        t0 = time.time()
        rc, out = make(reg, "-j4", "ota", f"OTA_HOST={a.host}", timeout=900)
        if rc:
            reg(FALLA, "make ota falló: " + out.strip().splitlines()[-1][:120])
        else:
            st2 = tras_reinicio(reg, a, serie, t0, st, 240, "SFT", descuento=en_intento)
            if st2:
                fid = str(st2.get("fw_id", "")).upper()
                reg(OK if fid == fw_repo else FALLA, f"fw_id después del OTA: {fid} (el repo: {fw_repo})")
                st = st2
        if not en_intento:
            reg(INFO, "la prueba 'OTA a mitad de un autoarranque' necesita el autoarranque armado y los "
                      "amplificadores ya encendidos por él; esto lo arma una persona, no el script")

    if a.barreras:
        reg.titulo("Barreras contra el firmware de la DS12")
        rc, out = make(reg, "-j4", "ota", "BOARD=ds12", f"OTA_HOST={a.host}", timeout=900)
        frenado = rc != 0 and "es una DS11" in out
        reg(OK if frenado else FALLA,
            "barrera 1 (Makefile): " + ("`make ota BOARD=ds12` se negó" if frenado else
                                        f"NO se negó como se esperaba (rc={rc})"))
        st = leer_status(a.host) or st
        if not os.path.exists(BIN_DS12):
            reg(FALLA, f"barrera 2: no existe {os.path.relpath(BIN_DS12, RAIZ)} (falló el build de la DS12)")
        elif st.get("proto_esp") != proto:
            reg(AVISO, "barrera 2: el ESP32 no tiene el firmware nuevo — no la pruebo (la única barrera "
                       "sería el STM32). Primero `make ota-esp`")
        else:
            barrera_esp32(a.host, BIN_DS12, reg)
        reg(INFO, "barrera 3 (OTA_End del STM32): sólo con un ESP32 viejo que no chequea — queda a mano")

    if serie:
        serie.vivo = False
    return reg


# ---------------------------------------------------------------------------
# Autoprueba: una placa falsa en localhost
# ---------------------------------------------------------------------------
class PlacaFalsa:
    """Contesta /status y /firmware como el ESP32, con lo que se le diga."""

    def __init__(self, status, firmware=(409, '{"status":"error","message":"este firmware es para DS12 '
                                              'y el equipo es DS11"}'), reinicia_al_rechazar=False):
        self.status, self.firmware, self.reinicia = dict(status), firmware, reinicia_al_rechazar
        falsa = self

        class H(http.server.BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def do_GET(self):
                cuerpo = json.dumps(falsa.status).encode()
                self.send_response(200)
                self.end_headers()
                self.wfile.write(cuerpo)

            def do_POST(self):
                self.rfile.read(int(self.headers.get("Content-Length", 0)))
                codigo, cuerpo = falsa.firmware
                if falsa.reinicia:
                    falsa.status["stm32_uptime_ms"] = 10
                    falsa.status["board"] = "DS12"
                self.send_response(codigo)
                self.end_headers()
                self.wfile.write(cuerpo.encode())

        self.srv = http.server.HTTPServer(("127.0.0.1", 0), H)
        self.host = f"127.0.0.1:{self.srv.server_address[1]}"
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()

    def cerrar(self):
        self.srv.shutdown()


SANA = {"stm32": "ok", "proto_ver": 23, "proto_esp": 23, "board": "DS11", "board_mismatch": False,
        "v5a": None, "usb_vbus": None, "amps": False, "autostart": False, "autostart_held": False,
        "reset": "SFT", "reset_rsr": "01FE0000", "fw_id": "ABCDEF12", "stm32_uptime_ms": 500000}

BANNER_SANO = """
=== DS11 Beamforming Audio (32ch) ===
RESET: SFT (RSR=0x01400000)
  readback R01=0x00 R02=0x31 R03=0x40 R04=0x02 (want R02=0x31 R03=0x40 R04=0x02)
PLACA: firmware DS11, jumper PE15 = 1 (espera 1) (ok)
"""


def autoprueba():
    fallas = []

    def exigir(cond, que):
        print(f"  {'✅' if cond else '❌'} {que}")
        if not cond:
            fallas.append(que)

    def niveles(R):
        return [n for n, _ in R]

    print("/status, con respuestas conocidas:")
    exigir(FALLA not in niveles(analizar_status(SANA, 23, "ABCDEF12"))
           and OK in niveles(analizar_status(SANA, 23)), "placa sana → sin ❌")
    for cambio, que in ((dict(board_mismatch=True), "jumper que no coincide"),
                        (dict(proto_ver=22), "STM32 con el protocolo viejo"),
                        (dict(proto_esp=22), "ESP32 con el protocolo viejo"),
                        (dict(board="DS12"), "una DS12"),
                        (dict(amps=True), "amplificadores encendidos a mano")):
        exigir(FALLA in niveles(analizar_status(dict(SANA, **cambio), 23)), f"{que} → ❌")
    exigir(niveles(analizar_status(None, 23)) == [FALLA], "no contesta → ❌, y nada más")
    R = analizar_status({"stm32": "timeout"}, 23)
    exigir(niveles(R) == [FALLA], "STM32 mudo → ❌, y nada más (ningún ✅)")
    exigir(FALLA in niveles(analizar_status({k: v for k, v in SANA.items() if k != "v5a"}, 23)),
           "un /status sin el campo v5a → ❌ (firmware viejo)")

    print("\nBanner, con respuestas conocidas:")
    exigir(FALLA not in niveles(analizar_banner(BANNER_SANO, "SFT")), "banner sano → sin ❌")
    exigir(FALLA in niveles(analizar_banner(BANNER_SANO, "POR")), "RESET distinto del esperado → ❌")
    exigir(FALLA in niveles(analizar_banner(BANNER_SANO.replace("PE15 = 1", "PE15 = 0")
                                            .replace("(ok)", "→ NO COINCIDE"))), "PE15 en 0 → ❌")
    exigir(FALLA in niveles(analizar_banner(BANNER_SANO.replace("R04=0x02 (", "R04=0x00 ("))),
           "AK5538 que ignora R04 → ❌")
    vacio = analizar_banner("")
    exigir(niveles(vacio) == [FALLA, FALLA, FALLA], "banner vacío → tres ❌, ningún ✅")
    exigir(FALLA in niveles(analizar_banner(BANNER_SANO, "SFT", descuento_esperado=True)),
           "OTA en un autoarranque sin la línea de descuento → ❌")
    exigir(FALLA not in niveles(analizar_banner(
        BANNER_SANO + "AUTOARRANQUE: el reinicio anterior fue por software (OTA/reflasheo): "
                      "no cuenta como intento fallido\n", "SFT", descuento_esperado=True)),
        "con la línea de descuento → sin ❌")

    print("\nBarrera del ESP32, contra una placa falsa:")
    with tempfile.NamedTemporaryFile(suffix=".bin") as b:
        b.write(b"\0" * 2048)
        b.flush()
        for falsa_kw, espera_falla, que in (
                ({}, False, "contesta 409 y no se reinicia → ✅"),
                (dict(firmware=(200, '{"status":"ok"}')), True, "ACEPTA el firmware (200) → ❌"),
                (dict(reinicia_al_rechazar=True), True, "contesta 409 pero se reinicia igual → ❌")):
            p = PlacaFalsa(SANA, **falsa_kw)
            reg = Registro()
            salida = sys.stdout
            sys.stdout = open(os.devnull, "w")
            try:
                barrera_esp32(p.host, b.name, reg, espera_reinicio=2)
            finally:
                sys.stdout.close()
                sys.stdout = salida
                p.cerrar()
            exigir((reg.fallas() > 0) == espera_falla, que)

    print(f"\n{'❌ el script NO pasa su autoprueba' if fallas else '✅ autoprueba completa'} ({len(fallas)} fallas)")
    print("   No cubre: make, el puerto serie real ni el ST-Link — eso se ve en el banco.")
    return 1 if fallas else 0


def main():
    ap = argparse.ArgumentParser(description="Banco de la fase 2 del plan DS12, en una DS11.")
    ap.add_argument("--host", help="IP o nombre del ESP32 de la placa")
    ap.add_argument("--serie", help="puerto serie del ST-Link (UART7, 115200), p.ej. /dev/cu.usbmodem1103")
    ap.add_argument("--stlink", action="store_true", help="probar `make reset` por ST-Link")
    ap.add_argument("--ota", action="store_true", help="reinstalar el firmware de la DS11 por OTA")
    ap.add_argument("--barreras", action="store_true", help="probar que el firmware de la DS12 rebota")
    ap.add_argument("--autoprueba", action="store_true", help="probar el script, sin placa")
    a = ap.parse_args()
    if a.autoprueba:
        sys.exit(autoprueba())
    if not a.host:
        ap.error("falta --host")
    reg = banco(a)
    nf = reg.fallas()
    na = sum(1 for n, _ in reg.resultados if n == AVISO)
    print(f"\n{'❌' if nf else '✅'} {nf} fallas, {na} avisos.  Registro: {os.path.relpath(reg.ruta, RAIZ)}")
    sys.exit(1 if nf else 0)


if __name__ == "__main__":
    main()
