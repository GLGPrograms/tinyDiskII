/**
 * @file std_font.h
 * @author Michael Köhler, giuliof
 * @brief Character map for I2C display, extended with some graphical symbols.
 * @version 0.2
 * @date 2022-09-26
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef STD_FONT_H_
#define STD_FONT_H_

#include <avr/pgmspace.h>

enum special_char_t
{
  FOLDER,
  FLOPPY,
  FILE,
  PADLOCK,
  BUSY,
  NUMOF
};

#define FONT_SIZE 6

extern const uint8_t std_font[][FONT_SIZE] PROGMEM;
extern const uint8_t cstm_font[NUMOF][FONT_SIZE] PROGMEM;


#endif // STD_FONT_H_