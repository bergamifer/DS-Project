# STM32H743 DMA Streams para SAI

## DMA1 (para SAI periféricos)
| Stream | Channel | SAI | Direction | Priority |
|--------|---------|-----|-----------|----------|
| Stream 0 | 0 | SAI1_A | TX | High |
| Stream 5 | 0 | SAI1_B | TX | High |

## DMA2 (alternativas)
| Stream | Channel | SAI | Direction |
|--------|---------|-----|-----------|
| Stream 1 | 0 | SAI1_A | TX |
| Stream 4 | 1 | SAI1_B | TX |
| Stream 2 | 2 | SAI4_B | RX |

## Configuración recomendada
- Mode: Circular
- Data width: Word (32-bit)
- Memory increment: Enabled
- Peripheral increment: Disabled
- FIFO: Enabled, threshold 1/2