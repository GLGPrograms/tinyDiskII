/**
 * @file floppy_main.h
 * @author Giulio (giulio@glgprograms.it)
 * @brief Core software of the floppy emulator
 * @version 0.1
 * @date 2022-03-03
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef FLOPPY_MAIN_H_
#define FLOPPY_MAIN_H_

// Initialize floppy interface. Call this before main loop.
void floppy_init();
// Floppy main event loop. Must periodically call this.
// Blocking routine while Apple][ keeps ENABLE asserted.
void floppy_main();
// Manually control WRITE_PROTECT line.
void floppy_write_protect(bool protect);
// Return true if Apple][ has asserted ENABLE.
bool floppy_drive_enabled();

#endif