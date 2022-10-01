/**
 * @file oled_gui.h
 * @author giuliof (giulio@glgprograms.it)
 * @brief Complementary routines to oled_lcd.cpp to introduce GUI-like interface
 * in tinyDisk][ using an OLED display.
 * @version 0.1
 * @date 2022-04-24
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef OLED_GUI_H_
#define OLED_GUI_H_

#include <inttypes.h>

// Prints an error message in a bubble. msg is an in-RAM string.
void oled_gui_error(const char* msg);
// Shows SD Card volume_name string in upper side of the display.
// volume_name is max 11 char length. Adds a padlock icon if sd_locked is true.
void oled_gui_volume_name(const char* volume_name, bool sd_locked);
// Shows current directory cwd and current file cfile in upper side of the
// display. cwd is max 11 char length, cfile has stripped extension (max 8 char).
void oled_gui_cwd_currentfile(const char* cwd, const char* cfile);
// Shows an hourglass icon in upper side of the display if busy is true.
void oled_gui_set_busyflag(bool busy);
// Prints file name, with custom icon and some properties. Must provide:
// is_dir if file is actually a directory.
// is_locked if file is read only (a padlock icon is added).
// line_no, between 0 and 5, to choose where to print file line.
void oled_gui_fileline(const char* filename, bool is_dir, bool is_locked, uint8_t line_no);

#endif // SRC_OLED_GUI_H_