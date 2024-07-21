/* * * * * * * * * * * * * * * * *  INCLUDES  * * * * * * * * * * * * * * * * */

#include "oled_lcd.h"

#include <avr/pgmspace.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>

#include "i2c.h"
#include "std_font.h"

#include "util/port.h"

/* * * * * * * * * * * * * PRIVATE MACROS AND DEFINES * * * * * * * * * * * * */

// 7 bit slave-adress without r/w-bit
#define LCD_I2C_ADR (0x78 >> 1)
#define LCD_DISP_OFF 0xAE
#define LCD_DISP_ON 0xAF
#define LCD_RES_PIN E, 2

// Display size in pixels
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

/* * * * * * * * * * * * * * *  PRIVATE TYPEDEFS  * * * * * * * * * * * * * * */

struct cursor_pos_str
{
  uint8_t x;
  uint8_t y;
};

enum
{
  INVERT_OFF = 0x00,
  INVERT_ON = 0xFF
};

/* * * * * * * * * * * * * STATIC FUNCTION PROTOTYPES * * * * * * * * * * * * */

static inline void oled_command_byte(const uint8_t &data);
static inline void oled_byte(const uint8_t &data);

/* * * * * * * * * * * * * * *  STATIC VARIABLES  * * * * * * * * * * * * * * */

// Cursor position buffer
static cursor_pos_str cursor_pos = {0, 0};

// Invert currently printed data
static uint8_t invert = INVERT_OFF;

// Initialization Sequence
static const uint8_t init_sequence[] PROGMEM = {
    LCD_DISP_OFF, // Display OFF (sleep mode)
    0x20, 0b00,   // Set Memory Addressing Mode
    0xB0,         // Set Page Start Address for Page Addressing Mode, 0-7
    0xD5, 0x80,   // Clock divide ratio/Oscillator frequency
    0xA8, 0x3F,   // Multiplexer ratio
    0xD3, 0x00,   // Display offfset
    0x40,         // Display start line
    0x8D, 0x10,   // Charge pump
    0xA1,         // Column remap
    0xC8,         // Set inverse scan direction (COMn to COM0)
    0xDA, 0x12,   // COM pin HW Config
    0x81, 0xCF,   // Contrast control
    0xD9, 0x22,   // Precharge period
    0xDB, 0x20,   // VCOMh deselect level
    0x8D, 0x14,   // TODO
    0xA4,         // Display off
    0xA6,         // Normal display
    0xAF          // Turn on display
};

/* * * * * * * * * * * * * * *  STATIC FUNCTIONS  * * * * * * * * * * * * * * */

static inline void oled_command_byte(const uint8_t &data)
{
  oled_command(&data, 1);
}

static inline void oled_byte(const uint8_t &data)
{
  oled_data(&data, 1);
}

/* * * * * * * * * * * * * * *  GLOBAL FUNCTIONS  * * * * * * * * * * * * * * */

bool oled_init()
{
  // Set up I2C peripheral
  i2c_init();

  // Set up RES pin
  Port(LCD_RES_PIN).DIRSET = PinMsk(LCD_RES_PIN);

  // Reset the display
  // FIXME, arbitrary delay
  Port(LCD_RES_PIN).OUTCLR = PinMsk(LCD_RES_PIN);
  _delay_ms(100);
  Port(LCD_RES_PIN).OUTSET = PinMsk(LCD_RES_PIN);
  _delay_ms(100);

  // Try to address the device, checking if present
  if (!i2c_start((LCD_I2C_ADR << 1) | 1))
    return false;
  i2c_stop();

  // Initialize the display
  uint8_t commandSequence[sizeof(init_sequence)];

  memcpy_P(commandSequence, init_sequence, sizeof(init_sequence));

  oled_command(commandSequence, sizeof(commandSequence));

  oled_clrscr();

  return true;
}

/* - - - - - - - - - - - - - - Low-level methods  - - - - - - - - - - - - - - */

void oled_command(const uint8_t cmd[], uint8_t size)
{
  i2c_start((LCD_I2C_ADR << 1) | 0);
  i2c_byte(0x00); // 0x00 for command, 0x40 for data
  for (uint8_t i = 0; i < size; i++)
  {
    i2c_byte(cmd[i]);
  }
  i2c_stop();
}

void oled_data(const uint8_t data[], uint16_t size)
{
  i2c_start((LCD_I2C_ADR << 1) | 0);
  i2c_byte(0x40); // 0x00 for command, 0x40 for data
  for (uint16_t i = 0; i < size; i++)
  {
    i2c_byte(data[i] ^ invert);
  }
  i2c_stop();
}

/* - - - - - - - - - - - - - - High-level methods - - - - - - - - - - - - - - */

/** Cursor positioning **/

void oled_gotoxy(uint8_t x, uint8_t y)
{
  if ((x > (DISPLAY_WIDTH)) || (y > (DISPLAY_HEIGHT / 8 - 1)))
    return; // out of display
  cursor_pos.x = x;
  cursor_pos.y = y;
  uint8_t commandSequence[] = {(uint8_t)(0xb0 + y), 0x21, x, 0x7f};
  oled_command(commandSequence, sizeof(commandSequence));
}

void oled_goto_char(uint8_t x, uint8_t y)
{
  oled_gotoxy(x * sizeof(std_font[0]), y);
}

void oled_home()
{
  oled_gotoxy(0, 0);
}

void oled_newline()
{
  oled_gotoxy(0, cursor_pos.y + 1);
}

/** Cleanup of display **/

void oled_clrline(uint8_t line_no)
{
  uint8_t displayBuffer[DISPLAY_WIDTH];
  memset(displayBuffer, 0x00, sizeof(displayBuffer));
  oled_gotoxy(0, line_no);
  oled_data(displayBuffer, sizeof(displayBuffer));
  oled_gotoxy(0, line_no);
}

void oled_clrscr()
{
  for (uint8_t i = 0; i < DISPLAY_HEIGHT / 8; i++)
    oled_clrline(i);
  oled_home();
}

void oled_putch(char ch)
{
  // Ignore fonts not in table
  if (ch < ' ' || ch > '~')
    return;

  if (((cursor_pos.x + FONT_SIZE) >= 128) || (ch == '\n'))
  {
    /* If the cursor has reached to end of line on page1
     OR NewLine command is issued Then Move the cursor to next line */
    oled_newline();
  }

  if (ch != '\n')
  {
    // As the lookup table starts from Space
    ch = ch - ' ';

    for (uint8_t i = 0; i < FONT_SIZE; i++)
    {

      // Get the data to be displayed for LookUptable
      uint8_t dat = pgm_read_byte(&std_font[(uint8_t)ch][i]);

      oled_byte(dat);
      cursor_pos.x++;
    }
  }
}

void oled_putstr(const char *str)
{
  while (*str != '\0')
    oled_putch(*(str++));
}

void oled_putcstmP(const uint8_t *ch)
{
  uint8_t buf[FONT_SIZE];
  memcpy_P(buf, ch, FONT_SIZE);
  for (uint8_t i = 0; i < FONT_SIZE; i++)
  {
    oled_byte(buf[i]);
    cursor_pos.x++;
  }
}

void oled_invert(bool inv)
{
  invert = inv ? INVERT_ON : INVERT_OFF;
}