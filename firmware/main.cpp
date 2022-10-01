/**
 * @file main.c
 * @author Giulio (giulio@glgprograms.it)
 * @brief Kickstart and setup code
 * @version 0.1
 * @date 2022-03-03
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

#include "src/clock.h"
#include "src/floppy_main.h"
#include "src/prompt.h"
#include "util/port.h"

#include "src/sdcard.h"
#include "src/sdcard_gpio.h"
#include "src/sdcard_nic.h"
#include "src/sdcard_fat.h"

#include "src/oled_gui.h"
#include "src/oled_lcd.h"
#include "src/encoder.h"

#include "debug.h"

// Enable GUI
#define DISPLAY_ENABLED (true)

static void cli_mainloop() __attribute__((noreturn));
static void gui_mainloop() __attribute__((noreturn));
static void display_update_file_list(uint16_t fat_idx);

int main()
{
  // If display is not connected or disabled, use CLI
  bool cli_mode = true;

  // Set up PLL to reach desired clock speed
  clk_init();

  // Configure SDcard I/O
  sdcard_init();

  // Configure prompt uart
  prompt_init();

  // Configure floppy
  floppy_init();

  // Initialize OLED display
  if (DISPLAY_ENABLED)
    cli_mode = !oled_init();

  PMIC.CTRL |= PMIC_HILVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm;
  sei();

  if (cli_mode)
    cli_mainloop();
  else
    gui_mainloop();
}

static void cli_mainloop()
{
  while (1)
  {
    prompt_main();

    // Do nothing if drive is disabled
    if (floppy_drive_enabled() && nic_file_selected())
    {
      debug_printP(PSTR("Reading enabled\n\r"));
      floppy_main();
    }
  }
}

static void gui_mainloop()
{
  encoder_init();

  // Currently "hovered" fat entry
  uint16_t fat_entry;
  // Currently selected folder and file names
  char file_name[FAT_FILENAME_STRLEN] = "";
  char cwd_name[FAT_FILENAME_STRLEN] = "";

  oled_gui_volume_name(NULL, false);

  while (1)
  {
    if (!sdcard_present())
    {
      oled_gui_error("Please insert an SD card");

      while (!sdcard_present())
        ;
      // "Debouncing", assure that card is correctly inserted
      _delay_ms(500);
    }

    oled_gui_error("Initializing SD card, please wait");

    // Initialize SD card
    if (!sdcard_card_init() || !sdcard_fat_init())
    {
      oled_gui_error("Error: SDcard failed in initialization");
      // Must remove SDcard to proceed
      while (sdcard_present())
        ;

      continue;
    }

    oled_gui_volume_name(NULL, sdcard_locked());
    oled_gui_cwd_currentfile(NULL, NULL);

    // Populate file list on home screen
    fat_entry = 0xFFFF;
    // TODO if fail, error: no file in this SDCARD
    fat_next(&fat_entry);
    display_update_file_list(fat_entry);

    while (sdcard_present())
    {
      int8_t delta = encoder_update();
      // Update file pointer and eventually update file list
      if (delta != 0)
      {
        if (delta > 0) fat_next(&fat_entry);
        if (delta < 0) fat_prev(&fat_entry);
        display_update_file_list(fat_entry);
      }

      if (encoder_clicked())
      {
        if (fat_is_directory(fat_entry))
        {
          // Get directory name
          fat_get_entry_name(cwd_name, fat_entry);
          // Enter in directory
          fat_cwd(fat_entry);
          // Reset pointer in directory entry table, then validate for first file
          fat_entry = 0xFFFF;
          fat_next(&fat_entry);
          // Update display with folder content
          display_update_file_list(fat_entry);
        }
        else
        {
          bool ro = sdcard_locked() || fat_is_ro(fat_entry);
          nic_build_fat(fat_entry);
          floppy_write_protect(ro);
          fat_get_entry_name(file_name, fat_entry);
          // FIXME strip extension
          for(uint8_t i = 0; i <= 8; i++)
            if (file_name[i] == '.')
            {
              file_name[i] = '\0';
              break;
            }
        }
        oled_gui_cwd_currentfile(cwd_name, file_name);
      }

      // Do nothing if drive is disabled
      if (floppy_drive_enabled() && nic_file_selected())
      {
        debug_printP(PSTR("Reading enabled\n\r"));

        oled_gui_set_busyflag(true);

        // While floppy is selected, floppy_main will never return
        floppy_main();

        oled_gui_set_busyflag(false);
      }
    }
  }
}

static void display_update_file_list(uint16_t fat_idx)
{
  // Invert for first file (hovered)
  oled_invert(true);

  int i;

  for (i = 0; i < 6; i++)
  {
    char filename[FAT_FILENAME_STRLEN];

    fat_get_entry_name(filename, fat_idx);

    oled_gui_fileline(filename, fat_is_directory(fat_idx), fat_is_ro(fat_idx), i);

    oled_invert(false);

    // If no more files, clean lines then stop
    if (!fat_next(&fat_idx)) break;
  }

  for (i++; i < 6; i++)
    oled_gui_fileline(NULL, false, false, i);
}
