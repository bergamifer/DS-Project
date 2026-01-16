🛠 Domosonica (DS) - Hardware & Schematic Specifications
Este repositorio contiene el diseño electrónico de Domosonica, una plataforma de desarrollo basada en el microcontrolador de alto rendimiento STM32H743. El diseño está optimizado para la gestión de interfaces visuales de alta velocidad y procesamiento de señales.

📋 Resumen de Componentes Clave
MCU: STM32H743VIT6 (LQFP-100).

Display: TFT LCD 0.96" IPS (Controlador ST7735).

Passives: Capacitores de desacople de 100nF y 4.7uF para estabilidad de carril de potencia del H7.

User Interface: Pulsador táctil con red de resistencia de pull-down.

🔍 Análisis de Bloques del Esquemático
1. Núcleo de Procesamiento (MCU Block)

El corazón del diseño es el STM32H743. Se ha prestado especial atención a los pines de alimentación y la integridad de señal en los buses de alta frecuencia.

VCC/GND: Distribución de planos de potencia para minimizar el ruido conmutado.

Pin PE3 (Timing/Debug): Salida directa para instrumentación externa. Es la "pata" principal para medir físicamente latencias de hardware.

2. Bloque de Interfaz Visual (Display ST7735)

Este bloque gestiona la comunicación con el display TFT. Se utiliza el bus SPI4 por su disposición física en los puertos E, facilitando el ruteado paralelo.

Pines Críticos (Netlist): | Etiqueta | Pin MCU | Función | Notas de Hardware | | :--- | :--- | :--- | :--- | | LCD_CS | PE13 | Chip Select | Control de habilitación del periférico. | | LCD_RS | PE12 | Register Select | Selecciona entre bus de datos o comandos. | | LCD_RES | PE11 | Reset | Línea de inicialización física del ST7735. | | SCL | PE14 | SPI Clock | Línea de sincronía de datos. | | SDA | PE15 | SPI MOSI | Línea de datos (Master Out). |

3. Control de Retroiluminación (Backlight)

El control del brillo se realiza mediante hardware dedicado para evitar parpadeos y ruidos en la línea de 3.3V.

Pin PE10 (TIM1_CH2N): Utiliza un canal de timer avanzado para generar una señal de control de alta precisión hacia el ánodo/cátodo del backlight.

4. Entrada de Usuario

Pin PC13: Entrada digital conectada al pulsador K1. Configurado con pull-down físico para asegurar un estado lógico LOW estable cuando no está presionado.

🏗 Especificaciones de Fabricación (PCBWay)
El diseño ha sido validado y enviado a fabricación con los siguientes parámetros técnicos, asegurando compatibilidad con procesos industriales de alta precisión:

Parámetro	Especificación
Dimensiones	100 mm x 100 mm
Capas	2 Capas (Top/Bottom)
Material	FR-4 : TG150 (Alta resistencia térmica)
Espesor de Placa	1.6 mm
Ancho de Pista Mínimo	6 mil (0.15 mm)
Espaciado Mínimo	6 mil (0.15 mm)
Tamaño de Vía Mínimo	0.3 mm
Máscara de Soldadura	Blanca (White)
Serigrafía	Negra (Black)
Acabado de Superficie	HASL con plomo (HASL with lead)
Cobre Final	1 oz Cu
Vía Process	Tenting vias (Vías cubiertas)
📐 Notas de Layout y Ruteado
Planos de Masa: Se recomienda el uso de planos de masa sólidos en ambas capas para reducir la EMI (Interferencia Electromagnética) generada por el clock del sistema de 400+ MHz.

Diferencial de SPI: Las líneas SCL y SDA deben mantenerse con longitudes similares para evitar desfasajes en frecuencias de SPI superiores a 25MHz.