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
3. `CMD_GET_CAL_RESULT`: exponer los 32 delays+signos medidos por AUTOFOCUS
   (hoy solo van al serial).
4. **mDNS en el ESP32** (`domosonica.local`) — la app deja de necesitar IP manual.
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
