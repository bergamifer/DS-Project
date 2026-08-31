# DS11 — Errata de la tirada de producción (2026-08)

> **Afecta a TODAS las placas de la tirada.** Dos cortocircuitos de cobre con la
> misma causa raíz: los gerbers se plotearon con los rellenos de zona
> desactualizados. Ambos localizados con el DRC de KiCad y reparados a mano;
> la placa quedó funcionando el 2026-08-04.

## Causa raíz común: gerbers ploteados sin re-rellenar zonas

En DS11 se agregaron vias de stitching de GND y se retocaron pistas que no
existían en DS10. Al plotear, KiCad usó **el cobre almacenado en el archivo**,
que era anterior a esos cambios — así que los planos nunca abrieron el
clearance alrededor de los objetos nuevos.

Evidencia de la cadena completa:

| Verificación | Resultado |
|---|---|
| DRC con rellenos almacenados | clearance **0.0000 mm** en ambos puntos |
| DRC con `--refill-zones` | las violaciones **desaparecen** |
| Drill de producción (`DS-PTH.drl` línea 660) | la via **se fabricó** |

O sea: **el diseño estaba bien, el ploteo no.** Por eso el defecto es idéntico
en todas las placas y no se ve con lupa (el corte está en capa interna en un
caso, y bajo un componente en el otro).

### Regla de proceso (obligatoria antes de mandar a fabricar)

1. **Refill all zones** (tecla `B` en pcbnew) inmediatamente antes de plotear.
2. Correr DRC **con** y **sin** `--refill-zones` y comparar: si difieren, los
   rellenos están desactualizados.
3. El warning *"zone fills are out-of-date"* que KiCad muestra al plotear **no
   es cosmético** — ahora sabemos que cuesta una tirada entera.

```bash
kicad-cli pcb drc --format json --output drc.json --severity-error --severity-warning DS.kicad_pcb
kicad-cli pcb drc --format json --output drc_refill.json --refill-zones --severity-error --severity-warning DS.kicad_pcb
# si drc.json tiene violaciones que drc_refill.json no tiene -> NO plotear
```

---

## Falla #1 — Via de GND cortocircuitando el plano de +3V3

- **Ubicación**: (136.815, 36.619) mm en el layout — esquina del conector
  **J3 (USB-C del ESP32)**, a ~2 mm de **R9**. Es la **única via** en 4 mm a la
  redonda, así que no hay riesgo de confundirla.
- **Qué es**: via de stitching de GND **suelta** (`free`, size 0.8 / drill 0.4)
  que atraviesa el relleno de **+3V3 en la capa interna In2** sin anillo de
  clearance. El barril metálico toca cobre vivo de +3V3.
- **Síntoma**: **+3V3 a GND ≈ 0.3 Ω**. La placa no arranca por ningún camino de
  alimentación (20 V, ST-Link o USB del ESP32).
- **Por qué era invisible**: el corto vive en una capa interna; no se ve con
  lupa y no hay un componente que se caliente de forma localizable.

**Reparación**: taladrar la via con mecha de **0.9–1.0 mm** desde arriba, a baja
velocidad, comiéndose el barril completo. Es una via de stitching entre muchas:
eliminarla no afecta la continuidad de masa. El plano de +3V3 queda con un
agujero de 1 mm, irrelevante.

**Verificación**: TP1 (`+3V3`) ↔ TP3 (`GND`) debe pasar de 0.3 Ω a cientos de Ω
o kΩ.

## Falla #2 — Pista de +5VA sobre cobre de GND, bajo C290

- **Ubicación**: zona (176–178, 123–125) mm, en el riel de los amplificadores,
  bajo/junto a **C290**.
- **Qué es**: pistas de `+5VA` en **F.Cu** con **0.0000 mm** de clearance contra
  las zonas `Ground` y `5V Analogicos`, más el pad 1 de C290 tocando ambas.
  Mismo mecanismo de relleno viejo, pero en la **cara superior**.
- **Síntoma**: **+5VA a GND en corto**. No lo detecta la medición de `+5V`
  (TP4), porque **+5VA es otro net y no tiene test point** — se mide en el pad
  `+` de C290.

**Reparación aplicada**: se **removió C290** para acceder a la pista y abrir el
cortocircuito con bisturí.

### ⚠️ Consecuencia a monitorear: bulk reducido en el riel de amplificadores

`+5VA` alimenta los **32 PAM8302A** y tenía **4× 220 µF** (C290, C291, C292,
C293) = 880 µF de bulk. Con C290 fuera quedan **660 µF**.

Esto se cruza con el riesgo ya conocido del **buck MP2315S al límite** (ver
`ds11_upgrades.md`): los 32 amplificadores **en fase** —que es exactamente lo
que hace un beam enfocado— son el peor caso de demanda instantánea, y el bulk
es justo lo que absorbe ese transitorio.

**Qué vigilar** al subir volumen con muchos canales activos:
- Reinicios o cuelgues del STM32 (brown-out del riel compartido).
- Distorsión que aparece sólo a volumen alto y con muchos canales.
- Si aparece: **volver a poblar C290** (220 µF / 16 V, LCSC C25503905) una vez
  reparada la pista, o agregar bulk equivalente cerca de FB3.

---

## Incidente derivado — reinicio al primer tono (2026-08-04)

Al lanzar el **channel walker** con la placa ya reparada, se escuchó *"el primer
tono con un fade in y después se apagó"*. La telemetría lo confirma: la secuencia
de `underruns` leída por HTTP fue **0, 0, 6, 7, 0, 0, 5** — los underruns
**se resetean a 0 en cada boot**, o sea que el STM32 estaba en **loop de
reinicio**. El modo tampoco cambiaba (`mode` siempre 0) porque el guardado en
EEPROM tiene debounce de 3 s y la placa moría antes de persistirlo.

Interpretación: **brown-out por demanda de los amplificadores**, sobre un buck
MP2315S ya al límite y ahora con **25% menos de bulk** (660 µF tras remover
C290). El rail se hunde, arrastra el rail derivado del MCU, y reinicia.

### Mitigación en firmware: los amps arrancan APAGADOS

Desde este incidente el pin **SD (PD3)** ya **no** se pone en alto
automáticamente al final del boot. Los 32 PAM8302A arrancan apagados y se
habilitan por comando:

```bash
curl -X POST http://<IP>/amps -d '{"on":true}'    # encender
curl -X POST http://<IP>/amps -d '{"on":false}'   # apagar
```

(también hay botones en la UI web). Nuevo comando de protocolo:
**`CMD_SET_AMPS` (0x0A)**, payload `uint8` 1/0.

**Por qué importa como herramienta**: con los amps apagados corre **todo** el
pipeline —SAI, DMA, beamforming, los 4 bancos— en silencio absoluto. Eso separa
de forma limpia dos hipótesis que de otro modo se confunden:

- Walker estable con amps OFF y reinicia con amps ON → **es alimentación**.
- Reinicia igual con amps OFF → **no es alimentación**, hay que buscar en otro
  lado (firmware, rail digital, watchdog).

Y evita el peor escenario de diagnóstico: una placa en loop de reinicio que no
se deja interrogar.

### Instrumentación: la placa ahora dice POR QUÉ se reinició (2026-08-28)

La mitigación de arriba separa "es alimentación" de "no es alimentación", pero
no distingue **qué tipo** de problema de alimentación. El 2026-08-27 la placa se
reinició sola con los amps encendidos y hubo que discutir si fue un corte de luz
o el hundimiento del riel — sin poder resolverlo, porque el log no lo decía.

El firmware ahora latchea `RCC->RSR` en `main()` **antes de `HAL_Init()`**, lo
decodifica y lo informa:

```
RESET: SFT (RSR=0x01460000)                      ← en el banner de arranque
[19s] Half: 100/s, ... | ESP: 192.168.100.70 | rst:SFT   ← y en CADA heartbeat
```

| tag | qué pasó |
|---|---|
| **POR** | se cortó la alimentación **o** se hundió el riel — ⚠️ **no se separan**, ver abajo |
| **PIN** | NRST: el ST-Link o el pulsador |
| **SFT** | reinicio por software (el nuestro, tras un OTA) |
| **LPWR** | salida ilegal de un modo de bajo consumo |
| ~~BOR~~ | rama inalcanzable — ver abajo |
| ~~IWDG / WWDG~~ | ramas muertas: no se arranca ningún watchdog |

> ### 🔴 Corrección del 2026-08-31 — lo que este dato NO resuelve
>
> La primera versión de esta sección prometía separar el corte de luz del
> hundimiento del riel mirando **BOR sin POR**. **No funciona**, y conviene
> saberlo antes de usarlo para decidir si repoblar C290.
>
> El umbral del brown-out sale del option byte `BOR_LEV`, cuyo valor de fábrica
> deja el BOR **apagado** — el nivel efectivo queda en el del PDR, ~1,6 V. Y
> este proyecto **no programa option bytes a propósito**: el bootloader propio
> de `ota.c` vive sin tocarlos. Con BOR apagado, cualquier hundimiento capaz de
> resetear el micro cruza también el PDR y levanta `PORRSTF`, así que la rama de
> BOR nunca se alcanza.
>
> Peor que inútil: el mensaje decía `<- la alimentacion llego a cero` ante un
> POR, dándole autoridad impresa a la conclusión equivocada justo en el caso que
> se quería diagnosticar. Ya está corregido — ahora dice
> `corte de alimentacion O hundimiento del riel (no se separan)`.
>
> **Para que la distinción exista** habría que programar `BOR_LEV` en nivel 2 o
> 3 (~2,4-2,7 V), una vez por placa. Es una decisión aparte: tocar option bytes
> en una placa que arranca desde un bootloader propio merece su propia
> verificación, no un efecto secundario de un cambio de diagnóstico.
>
> **Lo que el tag sí separa** —`PIN`, `SFT` y `LPWR` contra todo lo demás— sigue
> valiendo, y ya es bastante más de lo que había. Para medir la caída del riel,
> el camino que no depende de nada de esto es el tester en +5VA **mientras**
> suena (issue #16).
>
> **Método**: la tabla original se escribió razonando desde los flags que
> `RCC_RSR` *tiene*, sin verificar que el hardware los *produzca*. Un flag que
> existe en el datasheet no es un flag que vaya a levantarse. Lo encontró un
> code review, no una medición — y es la misma forma de error que
> `loopback_debug.md` §22.6 y §27.4.

Tres detalles que hacen que esto funcione, y que costaron encontrarlos:

1. **Va en el heartbeat, no sólo en el banner.** En el reinicio del 2026-08-27
   el banner **se perdió**: el log salta de `[86447s]` a `[3s]` sin banner,
   porque el VCP del ST-Link tarda en volver tras el reset. La telemetría
   periódica no se pierde.
2. **Los flags son ACUMULATIVOS.** `RCC->RSR` no se borra solo: conserva todo
   desde el último encendido real hasta que se escribe `RMVF`. Por eso el
   firmware los limpia en cada arranque — pero **la primera lectura después de
   instalar esto trae la historia entera y hay que descartarla** (acá informó
   `POR`, que era el encendido original de la placa).
3. **El orden del decodificador importa.** Un POR deja además `PIN` y `BOR` en
   alto, y los watchdogs dejan `PIN`. Se pregunta de lo más específico a lo más
   genérico; `PIN` queda último.

**Validado** el 2026-08-28: un reinicio por OTA informa `SFT` con
`RSR=0x01460000` (bit 24 alto, bit 23 `PORRSTF` **bajo**), o sea que el borrado
funciona y el decodificador discrimina.

> **Qué hacer con esto ahora**: dejar la placa con los amps encendidos y el modo
> 20 corriendo, y mirar el `rst:` del heartbeat. Si aparece **BOR**, es esta
> falla y hay que repoblar C290. Si aparece **POR**, es la red eléctrica y C290
> no lo va a arreglar. Hasta hoy eso era una discusión; ahora es una lectura.

---

## Estado

- ✅ Ambas fallas reparadas en la placa de trabajo; **compila, flashea y
  arranca** (2026-08-04).
- ⬜ El resto de las placas de la tirada necesita **las dos reparaciones**.
- ⬜ Para una eventual re-tirada: regenerar gerbers **con refill** y verificar
  con el doble DRC de arriba.

## Nota independiente — J7 pin 1 (3.3 V del ST-Link)

No es un defecto de fabricación sino de esquemático, pero conviene tenerlo
junto: el **pin 1 de J7 va directo al riel `+3V3`**, mientras que el pin 10
(5 V) sí pasa por el Schottky **D11**. Sin esa protección, el ST-Link intenta
alimentar toda la placa (STM32H743 + 4 PCM1690 + AK5538 + ESP32 ≈ 700 mA),
colapsa y se apaga su LED.

**Mitigación**: cortar el conductor del **pin 1** en el cable del ST-Link. Para
programar alcanza con SWDIO (2), SWCLK (3), GND (4), NRST (6) y —opcional—
UART7 (7, 8). **Para DS12**: poner un Schottky en serie en el pin 1, igual que
el D11 del pin 10.
