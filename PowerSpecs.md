# Sistema de Alimentación - Beamforming Array (Revisado)

## Estrategia de Alimentación

**Premisa**: El STM32 WeAct usa su LDO interno (SY8088AAC 1A) - NO robarle corriente para periféricos externos.

```
12V Switching Supply (común para múltiples sistemas)
  │
  ├─→ STM32 WeAct: 5V o USB (usa su LDO interno)
  │
  └─→ [Módulo alimentación audio] → 5V + 3.3V para DACs/ADC
```

---

## Consumo Real por Sistema

### STM32H743VIT6 (alimentado por WeAct LDO)
- 3.3V @ 300-400mA (desde LDO de placa)
- **NO sumar a cálculos externos**

### PCM1690 × 2 unidades (desde módulo externo)
- **VDD** (3.3V digital): 30mA × 2 = **60mA**
- **AVDD** (5V analógico): 80mA × 2 = **160mA**
- **DVDD** (5V core): 40mA × 2 = **80mA**

### PCM1802 (desde módulo externo)
- **VDD** (3.3V): **10mA**
- **VA** (5V): **20mA**

### Total Módulo Externo
| Rail | Corriente | Margen 30% | Total |
|------|-----------|------------|-------|
| 3.3V | 70mA | +21mA | **100mA** |
| 5V | 260mA | +78mA | **350mA** |

---

## Solución Simplificada: 2× LDO Lineales

**Ventajas**:
- Solo 2 componentes principales
- Bajo ruido (crítico para audio)
- Mínimo espacio PCB
- Sin inductores ni switching noise

### Regulador 5V: AMS1117-5.0
**Mouser P/N**: `595-AMS1117-5.0` (SOT-223)
- Input: 7-15V (desde 12V perfecto)
- Output: 5V @ 1A
- Dropout: 1.3V
- Precio: ~$0.35 USD

**Disipación térmica**:
- (12V - 5V) × 0.35A = **2.45W**
- Requiere copper zone como disipador en PCB

### Regulador 3.3V: AMS1117-3.3
**Mouser P/N**: `595-AMS1117-3.3` (SOT-223)
- Input: 4.75-15V (desde 12V o desde 5V rail)
- Output: 3.3V @ 1A
- Dropout: 1.3V
- Precio: ~$0.35 USD

**Disipación térmica**:
- Opción A desde 12V: (12V - 3.3V) × 0.1A = **0.87W**
- Opción B desde 5V: (5V - 3.3V) × 0.1A = **0.17W** ← Mejor

**Recomendación**: Alimentar LDO 3.3V desde salida del LDO 5V

---

## Esquema Final Simplificado

```
12V_IN
  │
  ├─→ [Protección] ─→ STM32 WeAct 5V pin (para su LDO interno)
  │
  └─→ [AMS1117-5.0] ─→ 5V ─┬─→ PCM1690 AVDD/DVDD
                            ├─→ PCM1802 VA
                            │
                            └─→ [AMS1117-3.3] ─→ 3.3V ─┬─→ PCM1690 VDD
                                                         └─→ PCM1802 VDD
```

---

## BOM Completo con Códigos Mouser

### Reguladores
| Componente | Mouser P/N | Cant | Precio |
|------------|------------|------|--------|
| AMS1117-5.0 (SOT-223) | 595-AMS1117-5.0 | 1 | $0.35 |
| AMS1117-3.3 (SOT-223) | 595-AMS1117-3.3 | 1 | $0.35 |

### Protección Entrada
| Componente | Mouser P/N | Cant | Precio | Notas |
|------------|------------|------|--------|-------|
| Polyfuse 500mA | 650-MF-MSMF050-2 | 1 | $0.25 | 1812 SMD |
| Diodo Schottky SS34 | 833-SS34-E3-5AT | 1 | $0.30 | SMB (3A/40V) |
| TVS 12V | 637-SMBJ12A-TR | 1 | $0.35 | SMB |

### Capacitores (todos cerámicos X7R o X5R)
| Valor | Voltaje | Mouser P/N | Cant | Precio | Uso |
|-------|---------|------------|------|--------|-----|
| 100µF | 25V | 81-GRM31CR61E107ME5L | 2 | $1.20 | Bulk 12V in |
| 22µF | 10V | 81-GRM31CR61A226ME9L | 4 | $1.00 | LDO out |
| 10µF | 10V | 81-GRM21BR61A106KE1L | 6 | $0.90 | Decoupling |
| 100nF | 50V | 81-GRM188R71H104KA3D | 10 | $0.40 | Local decoupling |

### Ferrite Beads (para 5V analógico)
| Componente | Mouser P/N | Cant | Precio |
|------------|------------|------|--------|
| 600Ω@100MHz | 81-BLM18PG601SN1D | 2 | $0.20 |

**Total BOM**: ~$5.50 USD por sistema

---

## Layout PCB y Copper Zones

### Esquema de capas
```
TOP LAYER:
- Señales digitales (I2C, SAI digital)
- Componentes (STM32, reguladores)
- Copper zone: GND_DIGITAL

BOTTOM LAYER:  
- Señales analógicas (salidas DAC)
- Copper zone: GND_ANALOG
```

### Unión de GND: Star Point Único

**NO hagas planos separados**, sino **regiones** unidas en un punto:

```
      ┌──────────────────────────────────┐
      │                                  │
      │  [STM32]    [I2C]   [SAI Logic]  │  ← GND_DIGITAL
      │                                  │
      │            │                     │
      │            │ Star Point          │
      │            ├─────────────────────┼─→ 12V GND Return
      │            │                     │
      │  [5V LDO] [3.3V LDO]            │
      │            │                     │
      │  [PCM1690] [PCM1690] [PCM1802]  │  ← GND_ANALOG
      │                                  │
      └──────────────────────────────────┘
```

### Implementación práctica:

1. **Crea UNA sola copper zone de GND** (no dividas)
2. **Coloca los componentes estratégicamente**:
   - STM32 + I2C en una zona (izquierda)
   - DACs + salidas analógicas en otra zona (derecha)
   - LDOs en el medio
3. **Star point**: Punto de conexión de entrada 12V GND
4. **Evita que corrientes digitales pasen por zona analógica**:
   - Usa trazas de retorno intencionales
   - Los GND vias cerca de STM32 no deben estar cerca de DACs

### Reglas de vias:

| Tipo señal | Regla vias |
|------------|------------|
| I2C, SAI digital | Vias permitidas libremente |
| SAI MCLK/BCK | Minimizar vias (max 2 por traza) |
| Salidas DAC (analógicas) | **SIN vias** - mantener en bottom layer |
| GND | Múltiples vias (stitching) pero agrupadas por zona |

### Via stitching para GND:

```
Zona Digital          Zona Neutral         Zona Analógica
   ○ ○ ○                ○ ○ ○                ○ ○ ○
   ○ ○ ○    ←────→      ○ ○ ○    ←────→      ○ ○ ○
   ○ ○ ○                ○ ○ ○                ○ ○ ○
   
   Spacing: 5-10mm      Spacing: 2-3mm       Spacing: 5-10mm
```

**NO pongas vias de GND entre zonas digital y analógica** - crea un "camino de baja impedancia" pero también un loop para ruido.

---

## Disposición de Componentes (sugerida)

```
[12V IN] ──→ [Protección] ──┬──→ [5V LDO] ──┬──→ [3.3V LDO] ──→ 3.3V_DACs
                             │               │
                             │               └──→ 5V_DACs
                             │
                             └──→ STM32 (5V pin)


TOP VIEW PCB:
┌─────────────────────────────────────────┐
│ 12V IN                                  │
│  ↓                                      │
│ [Protection]                            │
│  ↓                                      │
│ [AMS1117-5.0]──┬───→ [AMS1117-3.3]      │
│                │                        │
│  GND──────────⊕ STAR POINT              │
│                                         │
│ [STM32]    [I2C pullups]   [SAI]        │  ← Digital Zone
│                                         │
│                                         │
│ [PCM1690 #1]  [PCM1690 #2]  [PCM1802]   │  ← Analog Zone
│                                         │
│ [Outputs Ch1-8] [Ch9-16]    [Inputs]    │
└─────────────────────────────────────────┘
```

---

## Decoupling por IC

### AMS1117 LDOs
```
12V IN ─┬─ 100µF ─┬─ [LDO] ─┬─ 22µF ─┬─ 10µF ─→ 5V OUT
        │         │          │        │
       GND       GND        GND      GND
```

### Cada PCM1690
```
5V AVDD ─┬─ 10µF ─┬─ 100nF ─→ Pin AVDD
         │        │
        GND      GND (cerca del pin)

5V DVDD ─┬─ 10µF ─┬─ 100nF ─→ Pin DVDD
         │        │
        GND      GND

3.3V VDD ─┬─ 100nF ─→ Pin VDD
          │
         GND
```

### PCM1802
```
5V VA ─[Ferrite Bead]─┬─ 10µF ─┬─ 100nF ─→ Pin VA
                      │        │
                     GND      GND

3.3V VDD ─┬─ 100nF ─→ Pin VDD
          │
         GND
```

---

## Thermal Management

### AMS1117-5.0 (el que más disipa)

**2.45W de disipación** requiere:

**Opción A**: Copper zone como disipador
- Pad térmico del SOT-223 conectado a copper zone GND
- Mínimo 500 mm² de cobre en top layer
- + vias térmicas a bottom layer (6-8 vias)

**Opción B**: Disipador pequeño
- Aavid Thermalloy 577102B00000G (Mouser: `532-577102B00`)
- Precio: ~$1.50 USD

### Cálculo térmico:
- Rθ(JA) sin disipador: ~50°C/W
- Temperatura rise: 2.45W × 50°C/W = **122°C** ❌
- Con copper zone 500mm²: Rθ ~25°C/W → 61°C rise ✓
- Temperatura final: 25°C + 61°C = **86°C** (dentro de spec)

---

## Consumo Total desde 12V

Por sistema completo:
- STM32 desde 12V: 3.3V × 0.35A / 0.7 (eficiencia LDO WeAct) = **1.65W**
- DACs/ADC desde 12V: (5V × 0.35A + 3.3V × 0.1A) / 0.4 = **4.2W**

**Total**: ~6W por sistema desde 12V → **500mA @ 12V**

### Para múltiples sistemas:
| Sistemas | Corriente 12V | Potencia Total | Fuente recomendada |
|----------|---------------|----------------|-------------------|
| 1-4 | 2A | 24W | Mean Well RS-35-12 (3A/36W) |
| 5-8 | 4A | 48W | Mean Well LRS-100-12 (8.5A/102W) ⭐ |
| 9-16 | 8A | 96W | Mean Well LRS-150-12 (12.5A/150W) |

**Configuración objetivo:** 8 sistemas
- Corriente requerida: 8 × 500mA = **4A @ 12V**
- Potencia total: 8 × 6W = **48W**
- **Fuente seleccionada:** Mean Well LRS-100-12
  - Mouser P/N: `709-LRS10012`
  - Salida: 12V @ 8.5A (102W max)
  - Margen: 102W - 48W = **54W** (112% overhead) ✓

### Distribución de Alimentación
Para 8 sistemas, considerar:
- **Opción A:** Bus de 12V común con protección por polyfuse individual en cada sistema
- **Opción B:** Distribución estrella desde fuente central
- **Recomendación:** Cable 18 AWG mínimo para bus de 4A, con protección individual de 500mA por sistema

---

## Checklist PCB Layout

- [ ] AMS1117 con copper zone térmica ≥500mm²
- [ ] 6-8 vias térmicas debajo de cada LDO
- [ ] Capacitores decoupling <5mm de pines de alimentación
- [ ] Ferrite beads en 5V analógico antes de cada DAC
- [ ] GND star point cerca de entrada 12V
- [ ] Separación física >10mm entre zona digital y analógica
- [ ] Salidas analógicas de DACs SIN vias (bottom layer directo a conectores)
- [ ] Test points: 12V, 5V, 3.3V, GND