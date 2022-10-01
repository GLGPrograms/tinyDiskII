#include "oled_gui.h"

#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>

#include "oled_lcd.h"
#include "std_font.h"

/* * * * * * * * * * * * * * * * PUBLIC MEMBERS * * * * * * * * * * * * * * * */

// Number of lines in yellow area
#define YELLOW_H (2)
// Number of lines in blue area
#define BLUE_H (6)
// Starting line of blue area
#define BLUE_Y (2)
// Number of characters per line
#define MAX_X (21)
// Available area in error messages (exclude borders)
#define ERROR_X (1)
#define ERROR_Y (1)
#define ERROR_W (MAX_X - 2)
#define ERROR_H (BLUE_H - 2)

const uint8_t borders[][6] PROGMEM = {
    {0x00, 0xC0, 0x60, 0x30, 0x18, 0x08}, // Top Left
    {0x08, 0x08, 0x08, 0x08, 0x08, 0x08}, // Top
    {0x08, 0x18, 0x30, 0x60, 0xC0, 0x80}, // Top Right
    {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF}, // Right
    {0x30, 0x38, 0x1C, 0x0E, 0x07, 0x03}, // Bottom Right
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30}, // Bottom
    {0x00, 0x03, 0x06, 0x0C, 0x18, 0x30}, // Bottom Left
    {0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}, // Left
};

static void draw_borders();

/* * * * * * * * * * * * * * PUBLIC IMPLEMENTATIONS * * * * * * * * * * * * * */

static void draw_borders()
{
  // Top left
  oled_goto_char(0, BLUE_Y);
  oled_putcstmP(borders[0]);

  // Top
  for (uint8_t i = 0; i < ERROR_W; i++)
    oled_putcstmP(borders[1]);

  // Top Right
  oled_putcstmP(borders[2]);

  // Right
  for (uint8_t i = 0; i < ERROR_H; i++)
  {
    oled_goto_char(20, BLUE_Y + i + 1);
    oled_putcstmP(borders[3]);
  }

  // Bottom left
  oled_goto_char(0, BLUE_Y + BLUE_H - 1);
  oled_putcstmP(borders[6]);

  // Bottom
  for (uint8_t i = 0; i < ERROR_W; i++)
    oled_putcstmP(borders[5]);

  // Bottom Right
  oled_putcstmP(borders[4]);

  // Left
  for (uint8_t i = 0; i < ERROR_H; i++)
  {
    oled_goto_char(0, BLUE_Y + i + 1);
    oled_putcstmP(borders[7]);
    for (uint8_t j = 0; j < ERROR_W; j++)
      oled_putch(' ');
  }
}

/* * * * * * * * * * * * * * SHARED IMPLEMENTATIONS * * * * * * * * * * * * * */

/* - - - - - - - - - - - - - - Generic methods  - - - - - - - - - - - - - - - */

void oled_gui_error_P(const char *msg)
{
  // Fetch string message, split and display
  char buf[MAX_X * (ERROR_H) + 1];
  strncpy_P(buf, msg, sizeof(msg));

  oled_gui_error(buf);
}

void oled_gui_error(const char *msg)
{

  // Draw the contour
  draw_borders();

  // Put inside frame
  oled_goto_char(ERROR_X, BLUE_Y + 1);

  uint8_t space = ERROR_W;
  uint8_t y = 0;

  while (msg != NULL)
  {
    char *p = strsep((char **)&msg, " ");

    // Word length + trailing space
    uint8_t word_len = strlen(p) + 1;

    // If word is longer than display width, skip it
    if (word_len >= ERROR_W)
      continue;

    // If word is longer than available x space, put newline first
    if (word_len >= space)
    {
      oled_goto_char(ERROR_X, BLUE_Y + ERROR_Y + ++y);
      space = ERROR_W;
      if (y >= ERROR_H)
        break;
    }

    oled_putstr(p);
    oled_putch(' ');
    space -= (word_len);
  }
}

/* - - - - - - - - - - - - - - - - tinyDisk GUI - - - - - - - - - - - - - - - */

/*
|---------------------|
|VOLUMENAM11         L|
|D:CWDxxxx8 FILENAM8 B|
|---------------------|
*/

void oled_gui_volume_name(const char* volume_name, bool sd_locked)
{
  oled_clrline(0);

  // If no volume name provided, print "tinyDisk][""
  if (volume_name == NULL || volume_name[0] == '\0')
    oled_putstr("tinyDisk][");
  else
    oled_putstr(volume_name);

  oled_goto_char(20, 0);

  if (sd_locked)
    oled_putcstmP(cstm_font[PADLOCK]);
}

void oled_gui_cwd_currentfile(const char* cwd, const char* cfile)
{
  oled_clrline(1);

  oled_putstr("D:");
  if (cwd == NULL || cwd[0] == '\0')
    oled_putch('/');
  else
    oled_putstr(cwd);

  oled_goto_char(11, 1);

  if (cfile != NULL)
    oled_putstr(cfile);
}

void oled_gui_set_busyflag(bool busy)
{
  oled_goto_char(20, 1);

  // Custom char clessidra
  if (busy)
    oled_putcstmP(cstm_font[BUSY]);
  else
    oled_putch(' ');
}

void oled_gui_fileline(const char* filename, bool is_dir, bool is_locked, uint8_t line_no)
{
  oled_clrline(BLUE_Y + line_no);

  if (!filename) return;

  if (is_dir)
    oled_putcstmP(cstm_font[FOLDER]);
#if 0
  else if (is_nic)
    oled_putcstmP(cstm_font[FLOPPY]);
#endif
  else
    oled_putcstmP(cstm_font[FILE]);

  oled_goto_char(2, BLUE_Y + line_no);
  if (is_locked)
    oled_putcstmP(cstm_font[PADLOCK]);

  oled_goto_char(4, BLUE_Y + line_no);
  oled_putstr(filename);
}