/**
 * @file board/board.h
 * @brief Elige la placa: DS11 o DS12. Es el único header de placa que se incluye
 *        (a través de pin_definitions.h).
 *
 * La placa la decide el Makefile con -DBOARD_DS11 o -DBOARD_DS12
 * (`make BOARD=ds11`, que es el valor por defecto, o `make BOARD=ds12`).
 * Si no llega ninguna, NO compila: un binario que "adivina" la placa es
 * exactamente el error que esto existe para evitar.
 *
 * El código no pregunta "¿es DS12?": pregunta por CAPACIDADES
 * (#if BOARD_HAS_5VA_SENSE, …). En la DS11 esos pines están al aire, y una
 * capacidad de la DS12 prendida en el binario de la DS11 leería basura. Ver
 * docs/ds12_firmware_plan.md.
 */

#ifndef __BOARD_H
#define __BOARD_H

#if defined(BOARD_DS11) && defined(BOARD_DS12)
  #error "Definidas BOARD_DS11 y BOARD_DS12 a la vez: elegir una (make BOARD=ds11|ds12)"
#elif !defined(BOARD_DS11) && !defined(BOARD_DS12)
  #error "Falta la placa: compilar con make BOARD=ds11 o make BOARD=ds12"
#endif

#include "board_common.h"

#if defined(BOARD_DS11)
  #include "board_ds11.h"
#else
  #include "board_ds12.h"
#endif

/* Cada archivo de placa tiene que declarar TODAS las capacidades, en 0 o en 1.
 * Una que falte no puede quedar en un "0 por omisión" silencioso. */
#if !defined(BOARD_ID) || !defined(BOARD_NAME) || !defined(SPK_LAYOUT_2X16) \
 || !defined(BOARD_HAS_5VA_SENSE) || !defined(BOARD_HAS_PG) \
 || !defined(BOARD_HAS_VBUS_SENSE) || !defined(BOARD_HAS_USB_FS) \
 || !defined(BOARD_HAS_TIMING_LINE) || !defined(BOARD_HAS_ID_JUMPER) \
 || !defined(ID_JUMPER_ESPERADO)
  #error "El archivo de placa no declara todas las capacidades (ver board_ds11.h)"
#endif

#endif /* __BOARD_H */
