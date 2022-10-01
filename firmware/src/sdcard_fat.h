/**
 * @file sdcard_fat.h
 * @author giuliof (giulio@glgprograms.it)
 * @brief Structures and methods to access FAT file system on SD Card.
 * @version 0.1
 * @date 2022-08-28
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef SDCARD_FAT_H_
#define SDCARD_FAT_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// A valid 8+3 FAT filename is 11 characters long...
const size_t FAT_FILENAME_SIZEOF = 8 + 3;
// Reserve space for dot separator and \0 terminator
const size_t FAT_FILENAME_STRLEN = FAT_FILENAME_SIZEOF + 2;

enum fat_type_t
{
  FAT_FAT_UNKNOWN = 0,
  FAT_FAT16 = 16,
  FAT_FAT32 = 32
};

// This is needed for tests run on x86_64 platform
#pragma pack(push, 1)

struct fat_partition_t
{
  uint8_t state;
  uint8_t start_head;
  uint16_t start_cylinder_sector;
  uint8_t type;
  uint8_t end_head;
  uint16_t end_cylinder_sector;
  uint32_t sector_offset;
  uint32_t num_sectors;
};

struct fat_mbr_t
{
  uint8_t code[446];
  fat_partition_t partition[4];
  uint8_t signature;
};

struct fat_bootsector_t
{
  uint8_t jmp_boot[3];
  char oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sec_per_cluster;
  uint16_t reserved_sec_count;

  uint8_t num_fats;
  uint16_t root_entry_count;
  uint16_t total_sec;
  uint8_t media;
  uint16_t fat_size;
  uint16_t sector_per_track;
  uint16_t num_heads;
  uint32_t hidden_sec;
  uint32_t total_sector;

  union {
    struct
    {
      uint16_t drive_number;
      uint8_t ext_signature;
      uint32_t volume_id;
      char label[11];
      char fs_type[8];

      uint8_t padding[28];
    } fat16;

    struct
    {
      uint32_t fat_size;
      uint16_t flags;
      uint16_t version;
      uint32_t root_sector;
      uint16_t info_sector;
      uint16_t backup_sector;
      uint8_t reserved[12];

      uint16_t drive_number;
      uint8_t ext_signature;
      uint32_t volume_id;
      char label[11];
      char fs_type[8];
    } fat32;
  };

  uint8_t bootstrap_code[420];
  uint16_t signature;
};

enum fat_dir_attribute_t
{
  FILE_READ_ONLY = 1,
  FILE_HIDDEN = 2,
  FILE_SYSTEM = 4,
  FILE_VOLUME = 8,
  FILE_LFN = 15,
  FILE_DIRECTORY = 16,
  FILE_ARCHIVE = 32,
  FILE_ATTRIBUTES = 0x3f,
};

struct fat_dir_entry_t
{
  char name[11];
  uint8_t attribute;
  uint8_t reserved;
  uint8_t creation_time_tenth;
  uint32_t creation_time;
  uint16_t last_access_time;
  uint16_t first_cluster_high;
  uint32_t last_write_time;
  uint16_t first_cluster;
  uint32_t file_size;

  uint8_t is_volume() { return attribute & FILE_VOLUME; }
  uint8_t is_file() { return !(attribute & (FILE_VOLUME | FILE_DIRECTORY)); }
};

union fat_sector_t {
  uint8_t bytes[512];
  uint16_t words[256];
  uint32_t dwords[128];
  fat_dir_entry_t entries[16];
  fat_mbr_t mbr;
  fat_bootsector_t boot;
};

// This is needed for tests run on x86_64 platform
#pragma pack(pop)

bool sdcard_fat_init();
bool fat_cwd(uint16_t fat_entry);
bool fat_next(uint16_t* fat_entry);
bool fat_prev(uint16_t* fat_entry);
bool fat_get_entry_name(char* filename, uint16_t fat_entry);
bool fat_is_directory(uint16_t fat_entry);
bool fat_is_ro(uint16_t fat_entry);
bool fat_find_file(uint16_t* fat_entry, const char* filename);

#endif // SRC_SDCARD_FAT_H_