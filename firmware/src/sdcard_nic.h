/**
 * @file sdcard_nic.h
 * @author giuliof (giulio@glgprograms.it)
 * @brief Low level FAT/SD Card routines for NIC file management.
 * @version 0.1
 * @date 2022-08-28
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef SRC_SDCARD_NIC_H_
#define SRC_SDCARD_NIC_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool nic_build_fat(uint16_t fat_entry);
bool nic_file_selected();
void nic_unselect_file();
bool nic_update_sector(uint8_t dsk_trk, uint8_t dsk_sector);
uint8_t nic_get_bit();
void nic_abort_read(uint16_t bits);
bool nic_write_sector(uint8_t *buffer, uint8_t volume, uint8_t track, uint8_t sector);

#endif // SRC_SDCARD_NIC_H_
