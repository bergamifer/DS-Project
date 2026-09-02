# App iOS "Domosonica" — control remoto + roadmap espacial

> Creada 2026-07-04. Código en [`ios/`](../ios/). Compila verificado (`xcodebuild`, SDK iphoneos,
> Xcode 26.4.1). Estética = la del sitio público: tinta `#0a0d12` / madera `#c9a06b` (lo físico) /
> teal `#6fe0ce` (lo acústico). Tipografía SF Pro liviana + SF Mono para datos (sin serif).

---

## 1. v1 — IMPLEMENTADA

La app habla **HTTP con el ESP32-C3** (el puente ya existente de la Fase 4); no toca el
protocolo UART binario directamente. Tres pantallas (tabs):

### Control
- **Grilla de 12 modos** (espejo de `ui_states[]` del STM32 / `MODE_NAMES[]` del ESP32) —
  tap → `POST /mode {"mode": idx}` → el ESP32 manda `CMD_SET_MODE` y espera ACK.
- **Haz a medida**: segmentado TX/RX. TX: sliders x/y/z (mm, marco del oyente) →
  `POST /beam {"type":"tx","fx","fy","fz"}`. RX: slider azimuth ±90° → `{"type":"rx","az"}`.
  Envío con **debounce de 250 ms** al soltar el slider (no inunda el UART).
- **Tarjeta AUTOFOCUS** cuando el modo activo es 11: muestra `cal_state`
  (SETTLE → ARM → CAPT → CORR → DONE) con su significado.
- **VU de 8 entradas** desde `adc_pk[8]` (dBFS, piso −60 dB) + **salud**
  (underruns / errores SAI / OVF / uptime).
- **Polling 1 Hz** de `GET /status`; se pausa cuando la app va a background.

### Espacial (preview visual de la v2)
- **SceneKit**: la sala en 3D con el **muro real** (4×8 patrón ladrillo, 80/160 mm,
  filas 2&4 +40 mm — constantes espejadas de `main.c` en `Models.swift/SpeakerWall`).
- **Foco arrastrable** (drag = x/z). Cada parlante se ilumina según su **delay focal**
  — la misma cuenta que `Beam_UpdateTxFocus()`. Readout mono: foco xyz + Δt máx.
- Al soltar, **envía el foco real al instrumento** si hay conexión.
- **Follow-me (demo)**: el foco recorre una Lissajous; con "→ instrumento" activado
  streamea `POST /beam` cada 500 ms (rate limit deliberado).

### Ajustes
- Host/puerto del ESP32 (persistidos), **Probar conexión** (distingue: sin ESP32 /
  ESP32 sin STM32 / cadena completa), ficha del protocolo.

### Estados de conexión
`offline` (rojo) / `bridgeOnly` (madera: ESP32 ok, STM32 timeout) / `online` (teal).

### Detalles técnicos que importan
- `Info.plist`: `NSAppTransportSecurity → NSAllowsLocalNetworking` (iOS bloquea HTTP
  plano por defecto) + `NSLocalNetworkUsageDescription` (iOS 14+ pide permiso de red
  local la primera vez — **si el usuario lo niega, la app no ve al ESP32**;
  se rehabilita en Ajustes → Privacidad → Red local).
- `SWIFT_VERSION = 5.0` (modo lenguaje Swift 5: sin strict concurrency).
- Deployment target iOS 17.
- El catálogo de assets requiere tener descargada la **plataforma iOS** en Xcode
  (Settings → Components); sin ella `actool` falla con "No available simulator runtimes".

---

## 1.5 — v1.5 (2026-07-05): espacial expandida + RIR + multi-nodo

- **Ícono**: campo de interferencia enfocando (generado por script, `icon-1024.png`).
- **Espacial**: cámara orbital (modo Foco/Cámara + pinch zoom), **focos guardados**
  (chips, persistidos), **trayectorias**: grabar el gesto del dedo → reproducir en loop
  (interpolado) → streamear a los nodos; estela 3D de la trayectoria; follow-me demo.
- **Multi-nodo** (`NodesStore.swift`): registro de nodos con pose (x, z, yaw) en la sala,
  editor visual, foco global transformado al **marco local de cada nodo** y enviado en
  fan-out paralelo con rate limit. Los muros se dibujan en la escena con su gradiente
  de delays por nodo. Estrategia completa de escalado: [`multinode.md`](multinode.md).
- **Pestaña Sala (RIR)** (`RIR.swift` + `RIRView.swift`): sweep exponencial (Farina,
  60 Hz-16 kHz, 3 s) por el parlante del iPhone → grabación `.measurement` →
  deconvolución por DFT (vDSP) → impulso, curva de Schroeder, **RT60 broadband y por
  bandas** (250-4k), veredicto "¿esta sala focaliza?". Umbral: RT60 mid <0,6 s seca /
  <1,2 apta / <1,8 reverberante / más: la sala domina. Funciona en el simulador
  (usa mic/parlantes de la Mac). Excitación por el instrumento: TODO firmware.
- `NSMicrophoneUsageDescription` agregada al Info.plist.

---

## 1.6 — v1.6 (2026-08-31): la flota, no el nodo

La app hablaba con **un** nodo (el `host` de Ajustes) y, sólo para mover el foco,
con la lista de `NodesStore`. Eran **dos fuentes de verdad compitiendo**: el modo, el
volumen y los amplificadores iban a un solo nodo, y la lista de nodos existía
únicamente adentro de la pestaña Espacial.

- **`NodesStore` se creó al nivel de la app** (`DomosonicaApp`), no adentro de
  SpatialView. Todas las pantallas ven la misma lista.
- **`FleetController`** (`Fleet.swift`): fan-out de **modo, volumen y amplificadores**
  además del foco, con **resultado por nodo**. Cuando falta alguno se dice **cuál**:
  `"modo 12: 13/15 — sin responder: Nodo 4, Nodo 9"`. Un "13/15" obliga a ir a buscar
  quiénes son los dos, y en una muestra con público eso es tiempo que no hay.
- **Pestaña Flota** (`FleetView.swift`): una fila por nodo con conexión, modo, volumen,
  amps, underruns, errores SAI, CPU, uptime y firmware.
  🔴 **Lo que esta pantalla existe para mostrar es la DIVERGENCIA**: que un nodo quede
  en otro modo o con otro firmware no produce ningún síntoma salvo que la obra suene
  mal, y a esa altura ya nadie sabe por qué. El nodo desviado se marca en madera con
  el detalle de en qué difiere.
- **Polling escalonado**: round-robin, un nodo por tick. Quince a 1 Hz serían quince
  pedidos por segundo sobre el WiFi de la sala, compitiendo con los comandos que sí
  importan.
- **Descubrimiento por mDNS** (`NWBrowser` sobre `_domosonica._tcp`) — el tipo de
  servicio propio que ahora anuncia el ESP32. Con `_http._tcp` a secas la red devuelve
  también impresoras y routers.
- **`ESP32Client`** ganó `/volume`, `/amps`, `/ingain` y `/cal`. Antes implementaba
  cuatro endpoints de los quince que el ESP32 expone.
- **Confirmación para encender los amplificadores**: son 32 por nodo y suenan de
  inmediato, al volumen que haya quedado. Los botones de volumen llegan hasta
  **−15 dB** y no hasta 0, porque ése es el techo útil medido (`loopback_debug.md` §27).
- **La app recuerda la pestaña** (`@AppStorage("ui_tab")`).

### La grilla de modos ya no puede esconder un modo

`DSModes.all` era la **cuarta** copia de la lista de modos y se había quedado en 14 con
22 en el firmware. Ahora la grilla se dibuja desde el `num_modes` que informa el nodo
(`DSModes.grid(for:)`): si el firmware tiene más modos de los que la app conoce,
aparecen igual con su índice en vez de desaparecer sin aviso.

Es una mitigación, no la solución. La duplicación ya causó un bug silencioso —el modo
21 se agregó al STM32, el ESP32 lo rechazaba devolviendo `{"status":"ok"}`, y costó una
escalera de volumen medida en el modo equivocado— y la solución de fondo es que el
firmware publique la lista: **issue #20**.

### Verificado

Con dos nodos falsos locales (uno sano, uno divergente) y una IP muerta, en el
simulador:

- polling escalonado: 12 s, 3 nodos, **4 pedidos a cada uno** — round-robin exacto;
- la cabecera dice "2 de 3 en línea" en madera, no en teal;
- el nodo desviado muestra `distinto al resto: modo 1 · volumen −20 dB · firmware …`;
- sus 3 underruns salen en rojo, y el nodo muerto en rojo con "sin respuesta".

Compila para dispositivo y para simulador. **Contra hardware real no se probó** — la
placa estaba apagada el 31 de agosto.

---

## 2. Cómo instalar en el iPhone

1. Abrir `ios/Domosonica.xcodeproj` en Xcode. Si Xcode ofrece descargar la
   plataforma iOS, aceptar (una vez).
2. Target Domosonica → **Signing & Capabilities** → Team: agregar el Apple ID
   personal (gratis) → Xcode genera el perfil solo.
3. Conectar el iPhone por cable → en el iPhone: confiar en la compu →
   **Run (⌘R)** con el iPhone como destino.
4. Primera vez: iPhone → Ajustes → General → VPN y gestión de dispositivos →
   confiar en el certificado de desarrollador.
5. Al abrir: aceptar el permiso de **red local**, poner la IP del ESP32 en Ajustes.

**Límites de cuenta gratis**: la app expira a los **7 días** (reinstalar = otro ⌘R),
máx. 3 apps sideloaded. **Cuenta paga** (Apple Developer Program, US$99/año): apps
por 1 año, **TestFlight** (hasta 10.000 testers por link — ideal para músicos),
y acceso a App Store.

## 3. App Store — cuán difícil

- **TestFlight** (recomendado primero): subir build → revisión liviana (~1 día) →
  link público de invitación. Perfecto para la comunidad de la convocatoria.
- **App Store**: revisión completa. Para una app que controla hardware propio:
  - El revisor no tiene el instrumento → **la pestaña Espacial funciona sin
    hardware** y sirve como demo mode (requisito práctico de review).
  - Adjuntar **video demo** en las notas de revisión.
  - Necesita: ícono (falta diseñar), screenshots, política de privacidad
    (trivial: no recolecta datos).
  - Riesgo de rechazo bajo (guideline 4.2 "minimum functionality" se cubre con
    la vista espacial + control real). Esfuerzo total: días, no meses.

---

## 4. La física del teléfono como sensor (por qué y cómo)

### El problema
El AUTOFOCUS cableado funciona porque mic y parlantes comparten el **reloj del
STM32** (TOF = delta de muestras coherentes, sub-muestra). El celular **no
comparte reloj**, y el disparo por WiFi tiene jitter de ms. Sonido = 34 cm/ms →
TOF absoluto crudo es inservible.

### La salida: todo relativo
Para definir un foco no hacen falta TOF absolutos — solo los **32 delays
relativos**. Si el STM32 emite el parlante *k* en instante cadenciado conocido
(walker con paso Δ) y el celular graba **continuo**, cada llegada cumple
`a_k = C + TOF_k + k·Δ` donde `C` (offset de reloj + latencias) es **común a
todos** → se cancela al restar el mínimo. El celular parado en el punto deseado
mide los 32 TOF relativos → los delays focales exactos, sin sincronización.

- **Error residual**: drift del reloj del celular durante la captura (~20-50 ppm
  → sub-mm si el barrido dura ≤2 s). El barrido debe ser corto.
- **Mejora clave (TODO firmware)**: emitir los 32 canales **simultáneamente con
  códigos ortogonales** (chirps de pendiente distinta o secuencias PN) y separar
  por correlación en una sola grabación → captura de <1 s, drift despreciable.
- **La precisión alcanza**: el foco de un array de 0,6 m de apertura a
  frecuencias de audio mide decenas de cm — el error de ~cm del teléfono cae
  adentro del propio foco.

### Gotchas de iOS para medición acústica
- `AVAudioSession` en modo **`.measurement`** (apaga AGC/supresión — parcialmente).
- La latencia absoluta de audio de iOS es incierta → usar siempre esquemas que la
  **cancelen** (relativo, o two-way).
- Movimiento durante la medición = Doppler + error → medir quieto, o limitar
  velocidad en follow-me.

---

## 5. Roadmap (por ROI)

| # | Qué | Estado / camino |
|---|---|---|
| v1 | Control remoto nativo (modos, haz, VU, AUTOFOCUS) | ✅ implementada |
| v2a | **Colocación de focos con ARKit**: el iPhone se localiza solo (VIO, ~cm); una única medición acústica ancla el marco AR al marco del array. Caminás la sala y "pintás" focos. | Preview SceneKit ✅; ARKit pendiente |
| v2b | **Foco por micrófono del celular** (walker/códigos + grabación continua, §4) | diseño listo; necesita modo firmware |
| v3 | **Follow-me real**: ranging continuo 2-10 Hz o ARKit tracking → el foco te sigue | demo visual ✅; física pendiente |
| v4 | **Auto-survey de parlantes**: el celular en unos pocos puntos releva las posiciones reales de los ~500 parlantes (multilateración inversa) → llena la tabla de geometría que hoy es placeholder. El mayor valor práctico a escala. | idea especificada |
| v5 | **RIR de sala**: sweep por nodo → RT60/reflexiones → diagnóstico "¿esta sala focaliza?" | fácil, útil pre-instalación |
| v6 | **Multi-teléfono**: un foco por espectador; sensado distribuido del campo real | investigación |

**Referencias**: BeepBeep (Microsoft Research — two-way acoustic ranging COTS ~cm sin
sincronizar relojes), ALPS (CMU — posicionamiento acústico indoor, códigos ortogonales).

### TODOs de firmware que habilitan todo esto
1. Modo **walker de calibración** con cadencia exacta publicada (o trigger por
   comando con timestamp de muestra).
2. **Códigos ortogonales simultáneos** (32 chirps de pendiente distinta) — captura 1 s.
3. ~~`CMD_GET_CAL_RESULT`~~ — hecho: `GET /cal` devuelve los 32 TOF, signos y
   calidades. Ver `docs/calibracion_fase.md` y `tools/calfase.py`.
4. ~~**mDNS en el ESP32**~~ — hecho: el ESP32 anuncia `_domosonica._tcp` y la app lo descubre (v1.6).
5. ~~**La lista de modos, del nodo**~~ — hecho: `GET /modes` la sirve desde
   `ui_states[]` del STM32. La app ya no lleva su propia copia; lo que conserva
   son las etiquetas cortas (el firmware nombra para el cable serie, y
   «BARRIDO VERTICAL 800Hz arriba<->abajo @1m» no entra en un botón). Un modo sin
   etiqueta curada aparece igual, con el nombre del firmware: la app no puede
   esconder un modo que el equipo tiene.
5. `CMD_EVENT` push STM32→ESP32 (cambios de estado sin polling).

---

## 6. Decisiones de diseño de la app

| Decisión | Racional |
|---|---|
| HTTP al ESP32, no UART directo | El puente ya existe y aísla el protocolo binario |
| Debounce 250 ms en sliders | UART a 115200 + ACK de 300 ms: no inundar |
| Geometría del muro espejada en la app | La vista espacial calcula lo mismo que el firmware → lo que ves es lo que suena |
| SceneKit antes que ARKit | Corre en simulador, demo sin hardware, base visual para v2 |
| Modo oscuro forzado | El instrumento vive a oscuras; coherencia con el sitio |
| Sin dependencias externas | Cero SPM/CocoaPods: abrir y correr |
