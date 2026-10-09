/**
 * @file board/board_ds12.h
 * @brief DS12: la tirada de 10. Array 2×16. Agrega pines que en la DS11 estaban
 *        al aire; no mueve ninguno.
 *
 * Pines verificados contra el netlist exportado el 2026-10-08 20:58
 * (commit c37e9f9). Las capacidades están declaradas pero todavía no las usa
 * nadie: el código que las aprovecha llega en las fases 2 a 5 de
 * docs/ds12_firmware_plan.md.
 */

#ifndef __BOARD_DS12_H
#define __BOARD_DS12_H

#define BOARD_ID                12
#define BOARD_NAME              "DS12"

#define SPK_LAYOUT_2X16         1   /* 2 filas × 16 columnas entrelazadas */

#define BOARD_HAS_5VA_SENSE     1
#define BOARD_HAS_PG            1
#define BOARD_HAS_VBUS_SENSE    1
#define BOARD_HAS_USB_FS        1
#define BOARD_HAS_TIMING_LINE   1
#define BOARD_HAS_ID_JUMPER     1
#define ID_JUMPER_ESPERADO      0   /* PE15 a masa por JP3 + R36 (board_common.h) */

/* Lectura de +5VA: R163 100k (de +5VA) / R162 100k (a GND) / C312 4,7 nF.
 * PC1 = +5VA / 2. ADC123_INP11, FT_ha (DS12110). Umbral de SD: 2,25 V en el
 * pin = 4,5 V en +5VA (DS12_REVISION_ALIMENTACION.md §9.2). */
#define V5A_SENSE_PIN           GPIO_PIN_1      // PC1  pata 16
#define V5A_SENSE_PORT          GPIOC
#define V5A_SENSE_ADC_CHANNEL   ADC_CHANNEL_11  // ADC123_INP11

/* PG del buck de los amplificadores (IC4, AP63356), open-drain con pull-up
 * R159 100k a +3V3 y serie R152 100k. OJO: sin VIN el chip no puede bajarlo,
 * así que en modo sólo-USB dice "bien" con +5VA en 0. Manda PC1, no esto. */
#define PG_5VA_PIN              GPIO_PIN_10     // PA10 pata 69
#define PG_5VA_PORT             GPIOA

/* VBUS del USB del STM32 (J8): R161 62k / R160 100k / C50 100 nF.
 * PA5 = VBUS × 100/162. TT_ha: entrada SIN pull (un pull de 40 kΩ rompe el
 * divisor) y nunca con el DAC ni el ADC. */
#define VBUS_SENSE_PIN          GPIO_PIN_5      // PA5  pata 29
#define VBUS_SENSE_PORT         GPIOA

/* Línea de timing desde el ESP32: GPIO4 → R132 200 Ω → PD2. Entrada (EXTI). */
#define TIMING_LINE_PIN         GPIO_PIN_2      // PD2  pata 83
#define TIMING_LINE_PORT        GPIOD

/* USB Full-Speed del STM32 (OTG_FS, AF10), a través de U9 (USBLC6) y FL1. */
#define USB_FS_DM_PIN           GPIO_PIN_11     // PA11
#define USB_FS_DP_PIN           GPIO_PIN_12     // PA12
#define USB_FS_PORT             GPIOA
#define USB_FS_AF               GPIO_AF10_OTG1_FS

#endif /* __BOARD_DS12_H */
