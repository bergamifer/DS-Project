# Cálculo de Delays para Beamforming

## Geometría del Array
- Espaciado entre parlantes: X cm
- Configuración: Linear/Circular/Custom

## Fórmulas
delay_samples = (distance_meters / speed_of_sound) * sample_rate

## Ejemplos
Para espaciado de 10cm a 48kHz:
- 90°: 138 samples (~2.9ms)
- 45°: 98 samples (~2.0ms)