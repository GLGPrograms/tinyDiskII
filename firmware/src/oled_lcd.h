/**
 * @file oled_lcd.h
 * @author giuliof (giulio@glgprograms.it)
 * @brief Small library, that relies on i2c.h, that offers some basic methods to
 * handle an I2C OLED graphical display.
 * @version 0.1
 * @date 2022-04-24
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef OLED_LCD_H_
#define OLED_LCD_H_

#include <stdint.h>

// Initialize OLED interface. Call this before main loop. Includes a call to
// i2c_init().
bool oled_init();

/** Low-level methods **/
void oled_command(const uint8_t cmd[], uint8_t size);
void oled_data(const uint8_t data[], uint16_t size);

/** High-level methods **/
void oled_gotoxy(uint8_t x, uint8_t y);
void oled_goto_char(uint8_t x, uint8_t y);
void oled_home();
void oled_newline();

void oled_clrline(uint8_t line_no);
void oled_clrscr();

void oled_putch(char ch);
void oled_putstr(const char *str);
void oled_putcstmP(const uint8_t *ch);
void oled_invert(bool inv);

#endif // SRC_OLED_LCD_H_