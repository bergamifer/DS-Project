# Cálculo de Delays para Beamforming

## Geometría del Array
- Espaciado entre parlantes: **Por definir** (depende del diseño mecánico)
- Configuración: Linear/Circular/Custom (por definir)
- Delay máximo permitido: **100 ms**

## Fórmulas
```c
delay_samples = (distance_meters / speed_of_sound) * sample_rate
delay_samples_max = 0.100 * 48000 = 4800 samples @ 48kHz
```

## Memoria Requerida por Canal
- Delay máximo: 4800 samples
- Formato: 24-bit → 3 bytes por sample
- Memoria por canal: 4800 × 3 = **14.4 KB**
- 16 canales: 16 × 14.4 = **230.4 KB**
- Asignado en SRAM1/2: **307 KB** (margen: 76 KB)

## Ejemplos de Delay vs Ángulo
Para espaciado de 10cm a 48kHz (velocidad del sonido = 343 m/s):
- 90°: ~138 samples (~2.9ms)
- 45°: ~98 samples (~2.0ms)
- 30°: ~69 samples (~1.4ms)

**Nota:** Los valores reales dependerán del espaciado final del array.