/**
 * @file board/board_ds11.h
 * @brief DS11: la placa fabricada (tag ds11-release). Array 8×4.
 *
 * Todas las capacidades nuevas en 0: en la DS11 PC1, PA5, PA10, PD2, PE15,
 * PA11 y PA12 están al aire, y el firmware las deja en modo analógico. PE15 se
 * lee igual al arrancar: al aire es justamente lo que identifica a la DS11.
 */

#ifndef __BOARD_DS11_H
#define __BOARD_DS11_H

#define BOARD_ID                11
#define BOARD_NAME              "DS11"

#define SPK_LAYOUT_2X16         0   /* array 8×4 */

#define BOARD_HAS_5VA_SENSE     0
#define BOARD_HAS_PG            0
#define BOARD_HAS_VBUS_SENSE    0
#define BOARD_HAS_USB_FS        0
#define BOARD_HAS_TIMING_LINE   0
#define BOARD_HAS_ID_JUMPER     0
#define ID_JUMPER_ESPERADO      1   /* PE15 al aire: lee el pull-up (board_common.h) */

#endif /* __BOARD_DS11_H */
