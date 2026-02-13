# DS_Amp - Hat Amplificador 32 Canales

Hat amplificador para el sistema de beamforming DS9. Amplifica 32 canales diferenciales del PCM1690 y alimenta 32 parlantes reciclados de TV (0.5-1W, 8 ohm).

## Arquitectura

```
DS9 Board                    DS_Amp Hat                     Parlantes
+-----------+                +-------------------------+
| PCM1690x4 | J9 (32 pins)  | PAM8302A x 32           |    32x TV speakers
| VOUT+/OUT-|==============>| (mono Class-D filterless)|===> 0.5-1W / 8 ohm
|           | J10 (32 pins) |                          |
+-----------+==============>| PSU: 5V / 10A separada   |
                             +-------------------------+

Escalado: ~16 pares DS9+Amp = ~512 parlantes
```

## IC Amplificador: PAM8302A

| Parametro | Valor |
|-----------|-------|
| IC | PAM8302A (Diodes Inc) |
| LCSC | C113367 (PAM8302AASCR, $0.10) |
| Canales | 1 por IC (mono) |
| Potencia | ~1.5W @ 8 ohm / 5V |
| Entrada | Diferencial verdadera (IN+ / IN-) |
| Salida | BTL (VO+ / VO-) |
| Topologia | Class-D filterless (sin inductores) |
| Package | MSOP-8 |
| Supply | 2.5V - 5.5V |
| Eficiencia | ~88% |
| Cantidad | 32 ICs por hat |

### Pinout PAM8302A (MSOP-8)

```
        +---U---+
   SD  1|       |8  VO-
   NC  2|       |7  GND
  IN+  3|       |6  VDD
  IN-  4|       |5  VO+
        +-------+
```

## Circuito por Canal

```
PCM1690 VOUT+ --[0.1uF]-- IN+               VO+ ---- Speaker +
                                PAM8302A
PCM1690 VOUT- --[0.1uF]-- IN-  (MSOP-8)     VO- ---- Speaker -

                      VDD ---- SD (enable)
                      VDD --[10uF + 1uF]-- GND
```

### Componentes por IC (solo 4 pasivos)

| Componente | Valor | Funcion |
|------------|-------|---------|
| C_in+ | 0.1uF 0402 | Coupling entrada IN+ |
| C_in- | 0.1uF 0402 | Coupling entrada IN- |
| C_bypass1 | 10uF 0805 | Bypass VDD (baja freq) |
| C_bypass2 | 1uF 0402 | Bypass VDD (alta freq) |

SD conectado directo a VDD (enable permanente).

## Estructura del Esquematico

```
DS_Amp.kicad_sch (raiz)
|-- Power.kicad_sch              -> Entrada 5V, bulk caps, proteccion
|-- Amp_Bank.kicad_sch x 4       -> 8ch cada uno (8x PAM8302A)
|   |-- Bank_A: Ch 1-8   (DAC0, desde J9 pins 1-16)
|   |-- Bank_B: Ch 9-16  (DAC1, desde J9 pins 17-32)
|   |-- Bank_C: Ch 17-24 (DAC2, desde J10 pins 1-16)
|   +-- Bank_D: Ch 25-32 (DAC3, desde J10 pins 17-32)
|-- Connectors_In.kicad_sch      -> 2x PinHeader 2x16 macho (mates J9/J10)
+-- Connectors_Out.kicad_sch     -> 32x salida parlantes
```

Amp_Bank es una hoja jerarquica compartida, instanciada 4 veces.

## Conectores

### Entrada (desde DS9)

| Conector | Tipo | Pines | Funcion |
|----------|------|-------|---------|
| J1 | PinHeader 2x16 macho | 32 | Audio ch 1-16 (mates J9 DS9) |
| J2 | PinHeader 2x16 macho | 32 | Audio ch 17-32 (mates J10 DS9) |
| J_GND | Terminal block / cable | 1-2 | GND compartido robusto |

Pinout J9/J10: pines impares = VOUT+, pines pares = VOUT-

### Alimentacion

| Conector | Tipo | Funcion |
|----------|------|---------|
| J_PWR | Terminal block 2-pin 5.08mm | Entrada 5V / 10A |

### Salida (a parlantes)

32 salidas BTL. NO conectar ningun terminal del parlante a GND.

## Alimentacion

| Tension | Fuente | Corriente | Uso |
|---------|--------|-----------|-----|
| 5V | PSU externa separada | ~8-10A | PVDD amplificadores |

NO usar 5V de DS9 (no tiene capacidad para los amps).

Calculo: 32ch x 1W / 0.88 eficiencia / 5V = ~7.3A

PSU recomendada: 5V / 10A switching (~$5-8)

## BOM Estimado (por hat de 32ch)

| Componente | Qty | LCSC | Precio/u | Subtotal |
|------------|-----|------|----------|----------|
| PAM8302A | 32 | C113367 | $0.10 | $3.20 |
| Cap 0.1uF 0402 | 64 | C14663 | $0.002 | $0.13 |
| Cap 10uF 0805 | 32 | C15850 | $0.006 | $0.19 |
| Cap 1uF 0402 | 32 | C52923 | $0.002 | $0.06 |
| PinHeader 2x16 | 2 | - | $0.20 | $0.40 |
| Terminal blocks | ~16 | - | $0.10 | $1.60 |
| Terminal PWR | 1 | - | $0.15 | $0.15 |
| **Total componentes** | | | | **~$6** |
| PCB (JLCPCB) | 1 | | | ~$5-10 |
| Assembly (JLCPCB) | 1 | | | ~$10-15 |
| **Total por hat** | | | | **~$21-31** |

## PCB Design Notes

### Layout
- Tamano estimado: ~80x120mm (2 capas)
- 4 bancos de 8x PAM8302A
- Flujo unidireccional: IN -> IC -> OUT

### Reglas de routing
1. **Ground pour** continuo en bottom layer (prioridad maxima)
2. **Bypass caps** a menos de 3mm de pines VDD/GND de cada IC
3. **No cruzar** pistas de output sobre pistas de input
4. **Inputs** (IN+/IN-) como pares diferenciales, lado opuesto a outputs
5. **Power pour** ancho o pistas de minimo 2mm para 5V
6. Separacion minima 1mm entre pares de output de canales distintos

### Netclasses (en .kicad_pro)
- Default: 0.2mm track, 0.2mm clearance
- Power (+5V, GND): 2.0mm track, 0.3mm clearance

## Notas Importantes

- PAM8302A es producto **activo** (no NRND)
- Entrada diferencial verdadera: conecta directo a PCM1690 VOUT+/VOUT-
- Salida BTL: ambos terminales del parlante son activos, NO conectar a GND
- Verificar BOM vs Pick&Place antes de subir a JLCPCB
- Separar cada componente con part number distinto en lineas individuales de BOM

## Historial de Decisiones

| Fecha | Decision | Razon |
|-------|----------|-------|
| 2026-02-05 | PAM8403 descartado | NRND + entrada single-ended (no diferencial) |
| 2026-02-05 | PAM8406 descartado | Entrada single-ended |
| 2026-02-05 | PAM8302A elegido | Diferencial verdadera, $0.10, activo, MSOP-8 |
| 2026-02-05 | TPA3116D2 descartado | Excesivo para 0.5-1W, caro |

---

*DS_Amp v1.0 - Domosonica*
*KiCad 9.0 - Febrero 2026*
