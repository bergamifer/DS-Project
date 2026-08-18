# AK5538VN — Referencia ADC 8 Canales

**Dispositivo:** AKM AK5538VN
**Proyecto:** Domosonica DS10 (reemplaza PCM1802 del DS9)
**Ref en PCB:** IC3

---

## Especificaciones Clave

| Parámetro | Valor |
|-----------|-------|
| Canales | 8 (4 pares estéreo) |
| Resolución | 32 bits |
| Sampling rate | 8–192 kHz |
| SNR | 123 dB (A-weighted, 48kHz) |
| THD+N | −111 dB |
| Rango dinámico | 123 dB |
| Interfaz digital | TDM / I2S / Left-Justified |
| Interfaz control | I2C (slave) |
| Alimentación VDD | 3.3V |
| LDO interno | Sí (AVDD = 1.8V generado internamente con VDDA = 3.3V) |

---

## Configuración en DS10

### Hardware Strapping (PCB)

| Pin | Estado PCB | Efecto |
|-----|-----------|--------|
| MSN (pin 32) | GND | Modo **esclavo** — STM32 SAI3_B genera BICK y LRCK |
| Otros strapping | N/C o GND | Defaults del datasheet |

El AK5538VN en DS10 es **esclavo de audio**: el STM32 SAI3_B actúa como Master RX generando BICK (PD8) y LRCK (PD10), y el AK5538 responde con datos en SDTO1 (PD9).

### Conexiones STM32

| Señal AK5538 | Pin AK5538 | Pin STM32 | Descripción |
|-------------|-----------|-----------|-------------|
| BICK | 33 | PD8 AF6 (SAI3_SCK_B) | Bit clock — STM32 genera |
| SDTO1 | 36 | PD9 AF6 (SAI3_SD_B) | Audio data — AK5538 envía al STM32 |
| LRCK | 34 | PD10 AF6 (SAI3_FS_B) | Frame sync — STM32 genera |
| SCKI / MCLK | — | PE2 (SAI1_MCLK) | 24.576 MHz via bus MCLK |
| PDN | 44 | PC7 (AK_PDN) | Power-down: HIGH=activo, LOW=PDN |
| DIF1 | — | PC8 (AK_DIF1) | Strapped LOW en PCB (TDM mode) |
| OVF | — | PA15 (AK_OVF) | Overflow flag → input al STM32 |
| SCL | 45 | PA8 AF4 (I2C3_SCL) | I2C3 control |
| SDA | 43 | PC9 AF4 (I2C3_SDA) | I2C3 control |
| VDDA | — | 3.3VA | Analógico (separado del VDD digital) |
| VDD | — | 3.3V | Digital |

### Dirección I2C — VERIFICADA EN HARDWARE (2026-06)

| Parámetro | Valor |
|-----------|-------|
| 7-bit addr | **0x11** (scan I2C3 en placa DS10) |
| 8-bit write | **0x22** |
| Esquema AKM | 0x10 + CAD1·2 + CAD0 → CAD1=0, CAD0=1 en DS10 |

> ⚠️ El design review decía "7-bit 0x14" — **error** (su 5to error documentado).
> La dirección real se encontró con un scan completo del bus en el primer bring-up.

---

## SAI3_B Configuration (STM32 — Master RX TDM256)

```c
hsai_BlockB3.Instance = SAI3_Block_B;
hsai_BlockB3.Init.AudioMode = SAI_MODEMASTER_RX;
hsai_BlockB3.Init.Synchro = SAI_ASYNCHRONOUS;
hsai_BlockB3.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
hsai_BlockB3.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
hsai_BlockB3.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
hsai_BlockB3.Init.DataSize = SAI_DATASIZE_32;
hsai_BlockB3.Init.FirstBit = SAI_FIRSTBIT_MSB;
hsai_BlockB3.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
hsai_BlockB3.Init.Protocol = SAI_FREE_PROTOCOL;
hsai_BlockB3.Init.MonoStereoMode = SAI_STEREOMODE;
hsai_BlockB3.Init.CompandingMode = SAI_NOCOMPANDING;
hsai_BlockB3.FrameInit.FrameLength = 256;
hsai_BlockB3.FrameInit.ActiveFrameLength = 1;
hsai_BlockB3.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
hsai_BlockB3.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
hsai_BlockB3.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;
hsai_BlockB3.SlotInit.FirstBitOffset = 0;
hsai_BlockB3.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
hsai_BlockB3.SlotInit.SlotNumber = 8;
hsai_BlockB3.SlotInit.SlotActive = 0x000000FF;  // slots 0-7
```

**Resultado:** 8 canales 32-bit a 48 kHz, FrameLength=256 bits (= 8 slots × 32 bits).

---

## Secuencia de Inicialización — VERIFICADA EN HARDWARE (2026-06)

> ⚠️ **HALLAZGO CLAVE DEL BRING-UP**: el design review afirmaba que el strapping
> de hardware configuraba TDM256 — **FALSO**. El register dump del primer boot
> mostró `Control2 (R03) = 0x00` → TDM deshabilitado (modo estéreo) → datos
> misframed en el SAI. **La configuración TDM256 DEBE escribirse por I2C.**
> Causa probable: los pines de strap CKS0/CKS2 comparten el bus I2C3, cuyos
> pull-ups los fuerzan a 1 cuando se libera PDN.

Secuencia completa (implementada en main.c):

1. `AK_PDN (PC7) = LOW` — power-down durante boot (resetea los registros)
2. Configurar e iniciar SAI3_B DMA (Master RX) — BICK/LRCK corriendo
3. Asegurar MCLK (PE2, 24.576 MHz) corriendo
4. `AK_PDN (PC7) = HIGH` — liberar power-down, esperar ≥10 ms (POR)
5. **Configurar TDM256 por I2C3 (addr 0x22):**
   ```c
   write R01 = 0x00   // RSTN=0 — reset interno para cambiar modo
   write R03 = 0x40   // Control2: TDM[1:0]=10 → TDM256
   write R01 = 0x01   // RSTN=1 — operación normal
   ```
6. Readback de R01-R03 para verificar (R03 debe leer 0x40)
7. Verificar `AK_OVF (PA15)` — debe ser LOW (sin overflow)

**Importante:** los registros vuelven a default con cada ciclo de PDN — la
config I2C debe re-aplicarse en cada boot (el firmware ya lo hace).

---

## Registros — valores reales observados en DS10

Dump del primer boot (defaults post-PDN):

```
R00: FF  → Power Management: los 8 canales ON
R01: 01  → RSTN=1 (operación normal); bit0 = RSTN
R02: 01  → Control1: HPF habilitado
R03: 00  → Control2: TDM[1:0]=00 = TDM OFF (¡el strap NO configuró TDM!)
R04-R17: 00 (excepto R13=04, sin decodificar)
```

| Registro | Addr | Función confirmada |
|----------|------|--------------------|
| Power Management | 0x00 | Bit por canal (0xFF = 8 canales ON) |
| PM2 / Reset | 0x01 | bit0 = RSTN (0=reset interno, 1=normal) |
| Control 1 | 0x02 | bit0 = HPFE (HPF on/off); resto por confirmar |
| Control 2 | 0x03 | **bits [6:5] = TDM[1:0]: 00=off, 10=TDM256 (=0x40)** |

> El mapa coincide con el del AK5558 (hermano de familia). Para campos aún no
> usados (DIF, CKS, ganancias), confirmar contra el datasheet AK5538VN antes
> de escribir.

**Formato de datos observado**: con TDM256 activo, el chip entrega muestras de
**24 bits MSB-aligned en slots de 32** (byte LSB siempre 0x00 en el RX). El
camino de beamforming hace `>> 8` para llevarlas a 24-bit right-aligned, que
es exactamente lo que espera el TX (SAI DataSize=24 → bits [23:0]).

---

## Datos de Buffer (STM32)

El DMA de SAI3_B llena `adc_buffer` con muestras TDM interleaved:

```
adc_buffer[0]  = ch0 frame N
adc_buffer[1]  = ch1 frame N
adc_buffer[2]  = ch2 frame N
...
adc_buffer[7]  = ch7 frame N
adc_buffer[8]  = ch0 frame N+1
...
```

Procesamiento en `Process_ADC_Half()`:
```c
for (uint32_t i = 0; i < samples; i++) {
    for (uint32_t ch = 0; ch < ADC_CHANNELS; ch++) {  // ADC_CHANNELS = 8
        delay_line[ch][(wp + i) & DELAY_LINE_MASK] = rx_buf[i * ADC_CHANNELS + ch];
    }
}
```


---

## Hallazgos de la lectura completa del datasheet (2026-08-16)

> Fuente: `datasheets/ak5538vn-en-datasheet.pdf` (015099878-E-02, 2022/02).
> Lectura dirigida a **optimizar la DS11**, no a documentar el chip entero.

### 🔴 1. El filtro de diezmado nos costaba 292 µs de latencia — gratis

El AK5538 tiene **cuatro filtros de diezmado** seleccionables (datasheet §12, Tabla 20),
y el **group delay cambia 4×** entre ellos. A fs = 48 kHz:

| filtro | SD | SLOW | group delay | passband | stopband | ΔGD |
|---|---|---|---|---|---|---|
| **Sharp roll-off** ← *era el nuestro* | 0 | 0 | **19/fs = 396 µs** | 22 kHz | 85 dB | 0 |
| Slow roll-off | 0 | 1 | 7/fs = 146 µs | 12,5 kHz | 85 dB | 0 |
| **Short delay sharp** ← *el que conviene* | **1** | **0** | **5/fs = 104 µs** | **22 kHz** | **85 dB** | 2,8/fs |
| Short delay slow | 1 | 1 | 5/fs = 104 µs | 12,5 kHz | 85 dB | 1,2/fs |

**Short delay sharp tiene passband y atenuación IDÉNTICOS al sharp normal.** Lo único
que se pierde es la fase lineal: la distorsión de group delay pasa de 0 a 2,8/fs (58 µs)
como máximo en 0-20 kHz.

**Por qué esa distorsión no afecta al beamforming**: el filtro es el mismo en los 8
canales, así que su retardo es **común**. El patrón del haz lo fijan los delays
**relativos** que aplica el STM32, que no se tocan. Sólo afecta marginalmente al timbre.
Por el mismo argumento tampoco degrada al AUTOFOCUS: la distorsión está presente en
todas las mediciones y se cancela en los delays relativos.

**Aplicado**: `R04 = 0x02` en la secuencia de init, dentro de la ventana `RSTN=0`.

⚠️ **El registro pisa al strapping.** La PCB tiene los pines **SD (IC3.48) y SLOW
(IC3.47) atados a GND**, lo que sugeriría que el filtro no se puede cambiar. Pero el
datasheet §"Operation Mode Control" es explícito:

> *"In serial control mode, register settings are prioritized so that all pin settings
> except the MSN pin setting are ignored."*

y la placa está en modo **I²C** (`I2C`=H en IC3.54, `PSN`=L en IC3.53) según Tabla 22.
El readback de `R04` en el log de boot lo confirma empíricamente: **si lee `0x02` el
registro mandó; si lee `0x00`, hay que volver a contar los 396 µs.**

### 2. El presupuesto de latencia real es mayor que el que reporta el firmware

El heartbeat imprime `Total pipeline: buffer + fill`. **Eso subestima la latencia real**
porque no cuenta los retardos de grupo de los conversores:

| término | antes | con short delay |
|---|---|---|
| buffer (480 muestras) | 10000 µs | 10000 µs |
| fill | 668 µs | 668 µs |
| **group delay del ADC** | **396 µs** | **104 µs** |
| group delay del PCM1690 | (sin medir) | (sin medir) |
| **total (sin el DAC)** | **11064 µs** | **10772 µs** |

**La palanca grande sigue siendo el buffer**: son el 93% de la latencia. Bajar de 480 a
240 muestras ahorraría **5 ms** — diez veces más que el filtro. Antes no era viable con
la CPU al 59%; **ahora que está en 26,6% sí lo es** (subiría a ~30% por el overhead extra
de callbacks). Es la optimización a considerar si la latencia llega a importar.

### 3. Suma de canales: +6 dB de rango dinámico para el mic de medición

`R01` bits MONO2-1 configuran **channel summation** (§12, Tablas 17-19):

| modo | rango dinámico |
|---|---|
| Sin suma (actual) | 111 dB |
| 8-to-4 | 114 dB |
| **8-to-2** | **117 dB** |
| 8-to-1 | 120 dB |

**No sirve para el modo 15** (la intervención necesita las 8 entradas independientes).
**Sí sirve para AUTOFOCUS**: paralelando el mic de medición en 4 entradas y usando
8-to-2 se ganan **6 dB de SNR** en la correlación del chirp, que es justo donde el
ruido de fondo limita la medición. Requiere un cable en Y, nada de firmware más allá
de escribir `R01`.

### 4. Cosas verificadas que están BIEN (no tocar)

- **LDOE (IC3.57) → +3V3** ✅. Con TVDD = 3,3 V el LDO interno **debe** estar ON
  (Tabla 21). Si estuviera a GND, el core digital quedaría sin alimentación válida.
- **DP (IC3.55) → GND** ✅ = modo PCM (no DSD).
- **`I2C`=H, `PSN`=L** ✅ = modo I²C con registros prioritarios.
- **HPF**: la frecuencia de corte está **fija en 1,0 Hz** a 48 kHz — no es configurable,
  sólo on/off por `HPFE`. Ya está en ON.
- **El orden de escritura del firmware es el correcto**: el datasheet exige que el ajuste
  del HPF se cambie **con todos los canales en reset**, y la secuencia ya hace
  `RSTN=0 → R02/R03/R04 → RSTN=1`.
- **OVF tiene el mismo group delay que el ADC** — o sea que la bandera de saturación
  llega retrasada lo mismo que el audio. No es un problema, pero explica por qué no
  sirve para protección instantánea.

### 5. Datos útiles para el bring-up

- **Ciclo de inicialización**: 583/fs en slave (≈12 ms a 48 kHz), 578/fs en master.
  Durante ese tiempo la salida digital es **0**, no ruido. Si al arrancar se ven ceros
  en el ADC durante ~12 ms, es normal.
- **PDN debe estar en L al menos 150 ns** después de que AVDD y TVDD estén arriba.
- Tras salir de power-down, la salida tarda el **group delay** en asentarse.
- **TDM512 sólo existe a fs = 48 kHz**; TDM256 admite 48 y 96 kHz. Estamos en TDM256 a
  48 kHz, o sea con margen para subir a 96 kHz sin cambiar el formato si alguna vez hace
  falta (costaría el doble de CPU en los fills).

---

## Comparación PCM1802 (DS9) vs AK5538VN (DS10)

| Parámetro | PCM1802 (DS9) | AK5538VN (DS10) |
|-----------|---------------|-----------------|
| Canales | 2 (estéreo) | **8** |
| Bits | 24 | **32** |
| SNR | 105 dB | **123 dB** |
| Control | 8 GPIO pines | **I2C3 + 3 GPIO** |
| SAI mode | I2S 2ch | **TDM256 8ch** |
| SAI config | Master RX 2ch | **Master RX 8ch FrameLen=256** |
| Delay lines fw | `delay_line_L/R[1024]` | **`delay_line[8][1024]`** |

---

*AK5538VN Reference — DS10/DS11 — Abril 2026, ampliado 2026-08-16 con la lectura del datasheet*
*Ver [audio_architecture.md](audio_architecture.md) para integración completa*
