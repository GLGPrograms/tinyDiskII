/* * * * * * * * * * * * * * * * *  INCLUDES  * * * * * * * * * * * * * * * * */

#include "sdcard.h"

#include "sdcard_fat.h"

#include <string.h>
#include <ctype.h>

#include "debug.h"

/* * * * * * * * * * * * * * *  PRIVATE TYPEDEFS  * * * * * * * * * * * * * * */

enum fat_error_t
{
  FAT_OK = 0,
  FAT_ERROR_INIT,
  FAT_ERROR_READ,
  FAT_ERROR_DISK_FORMAT_ERROR,
  FAT_ERROR_NO_FAT,
  FAT_ERROR_NO_MORE_FILES,
  FAT_ERROR_BAD_FILE,
  FAT_ERROR_FILE_NOT_FOUND,
};

/* * * * * * * * * * * * * STATIC FUNCTION PROTOTYPES * * * * * * * * * * * * */

#if 0
static void to_fat_filename(char *fat_filename, const char *filename);
#endif
static void from_fat_filename(char *filename, const char *fat_filename);
static fat_error_t sdcard_fat_find_boot_sector(uint32_t sector);

/* * * * * * * * * * * * * * *  STATIC VARIABLES  * * * * * * * * * * * * * * */

static fat_type_t fat_type = FAT_FAT_UNKNOWN;

/* * * * * * * * * * * * * * *  GLOBAL VARIABLES  * * * * * * * * * * * * * * */

// WIP data shared with sdcard_nic for faster access to floppy disk files.
// Address of the first FAT table
uint32_t fat_addr = 0;
// Number of sectors per cluster
uint32_t sec_per_cluster = 0;
// log_2(sec_per_cluster)
uint8_t sectors_per_cluster2 = 0;
// Address of root directory entry
uint32_t root_addr = 0;
// Address of current directory entry
uint32_t dir_addr = 0;
// Address of data entry
uint32_t data_addr = 0;

// Data shared with sdcard.c for sector caching
extern uint8_t sector_cache[SDCARD_BLOCK_SIZE + 2];

/* * * * * * * * * * * * * * *  STATIC FUNCTIONS  * * * * * * * * * * * * * * */

// Currently unused
#if 0
static void to_fat_filename(char *fat_filename, const char *filename)
{
  memset(fat_filename, ' ', FAT_FILENAME_SIZEOF);

  for (uint8_t i = 0, j = 0; j < FAT_FILENAME_SIZEOF; i++)
  {
    if (filename[i] == '\0')
      return;

    if (filename[i] == '.')
    {
      j = 8;
      continue;
    }

    fat_filename[j++] = toupper(filename[i]);
  }
}
#endif

static void from_fat_filename(char *filename, const char *fat_filename)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (fat_filename[i] == ' ')
      break;

    *(filename++) = fat_filename[i];
  }

  if (fat_filename[8] == ' ')
    goto nodot;

  *(filename++) = '.';

  for (uint8_t i = 8; i < FAT_FILENAME_SIZEOF; i++)
  {
    if (fat_filename[i] == ' ')
      break;

    *(filename++) = fat_filename[i];
  }

nodot:

  *filename = '\0';
}

// Look for a FAT FS at a given sector.
static fat_error_t sdcard_fat_find_boot_sector(uint32_t sector_addr)
{
  fat_sector_t* sector = (fat_sector_t*)sector_cache;
  if (!sdcard_read_sector(sector->bytes, sector_addr))
    return FAT_ERROR_READ;
  if (sector->boot.signature != 0xaa55)
    return FAT_ERROR_DISK_FORMAT_ERROR;
  if (sector->boot.fat16.fs_type[0] == 'F' &&
      sector->boot.fat16.fs_type[1] == 'A')
  {
    fat_type = FAT_FAT16;
    return FAT_OK;
  }
  if (sector->boot.fat32.fs_type[0] == 'F' &&
      sector->boot.fat32.fs_type[1] == 'A')
  {
    fat_type = FAT_FAT32;
    return FAT_OK;
  }
  return FAT_ERROR_NO_FAT;
}

/* * * * * * * * * * * * * * *  GLOBAL FUNCTIONS  * * * * * * * * * * * * * * */

bool sdcard_fat_init()
{
  fat_sector_t* sector = (fat_sector_t*)sector_cache;
  // Is the VBR on sector 0?
  uint32_t boot_sector = 0;
  debug_printP(PSTR("FindBootSect\n\r"));
  fat_error_t status = sdcard_fat_find_boot_sector(boot_sector);
  if (status == FAT_ERROR_READ || status == FAT_ERROR_DISK_FORMAT_ERROR)
    return false;

  if (status == FAT_ERROR_NO_FAT)
  {
    // There is a partition table. Read first partition.
    boot_sector = sector->mbr.partition[0].sector_offset;
    status = sdcard_fat_find_boot_sector(boot_sector);
    if (status != FAT_OK)
      return false;
  }

  debug_printP(PSTR("FS FAT\n\r"));

  if (sector->boot.bytes_per_sector != SDCARD_BLOCK_SIZE)
  {
    debug_printP(PSTR("Wrong bytes per sectors %d != %d\n\r"),
      sector->boot.bytes_per_sector,
      SDCARD_BLOCK_SIZE);
    return false;
  }

  // Read FS layout.
  uint32_t sectors_per_fat = sector->boot.fat_size;
  if (fat_type == FAT_FAT32)
    sectors_per_fat = sector->boot.fat32.fat_size;
  if (sector->boot.num_fats == 2)
      sectors_per_fat += sectors_per_fat;
  sectors_per_fat *= SDCARD_BLOCK_SIZE;

  fat_addr = (boot_sector + sector->boot.reserved_sec_count) * SDCARD_BLOCK_SIZE;
  sec_per_cluster = sector->boot.sec_per_cluster;

  sectors_per_cluster2 = 0;
  for (uint8_t i = sec_per_cluster; i != 1; i >>= 1)
    sectors_per_cluster2++;

  uint32_t start = fat_addr + sectors_per_fat;

  root_addr = dir_addr = fat_type == FAT_FAT32 ? sector->boot.fat32.root_sector : start;
  data_addr = start + ((uint32_t)sector->boot.root_entry_count * sizeof(fat_dir_entry_t));

  debug_printP(PSTR("fat_addr:%lx\n\r"), fat_addr);
  debug_printP(PSTR("sec_per_cluster:%lx\n\r"), sec_per_cluster);
  debug_printP(PSTR("root_addr:%lx\n\r"), root_addr);
  debug_printP(PSTR("data_addr:%lx\n\r"), data_addr);

  return true;
}

bool fat_cwd(uint16_t fat_entry)
{
  fat_dir_entry_t dir_entry;
  sdcard_read_offset(&dir_entry, dir_addr + fat_entry * sizeof(fat_dir_entry_t), sizeof(dir_entry));
  if (dir_entry.first_cluster == 0)
  {
    dir_addr = root_addr;
  }
  else
  {
    dir_addr = dir_entry.first_cluster - 2;
    dir_addr <<= sectors_per_cluster2;
    // Convert in bytes.
    dir_addr *= SDCARD_BLOCK_SIZE;
    dir_addr += data_addr;
  }

  return true;
}

bool fat_move(uint16_t *fat_entry, uint16_t stop, int8_t inc)
{
  for (uint16_t i = *fat_entry + inc; i != stop; i += inc)
  {
    fat_dir_entry_t dir_entry;
    sdcard_read_offset(&dir_entry, dir_addr + i * sizeof(dir_entry), sizeof(dir_entry));

    // Stop if end of table is reached.
    if (dir_entry.name[0] == 0)
      break;

    // Skip volumes.
    if (dir_entry.is_volume())
      continue;

    // Skip current directory, or deleted files
    if ((dir_entry.name[0] == '.' && dir_entry.name[1] == ' ')
      || (uint8_t)dir_entry.name[0] == 0xe5)
      continue;

    *fat_entry = i;

    return true;
  }
  return false;
}

bool fat_next(uint16_t *fat_entry)
{
  return fat_move(fat_entry, 512, 1);
}

bool fat_prev(uint16_t *fat_entry)
{
  return fat_move(fat_entry, 0xFFFF, -1);
}

bool fat_get_entry_name(char *filename, uint16_t fat_entry)
{
  fat_dir_entry_t dir_entry;
  sdcard_read_offset(&dir_entry, dir_addr + fat_entry * sizeof(fat_dir_entry_t), sizeof(dir_entry));
  from_fat_filename(filename, dir_entry.name);
  return true;
}

bool fat_is_directory(uint16_t fat_entry)
{
  fat_dir_entry_t dir_entry;
  sdcard_read_offset(&dir_entry, dir_addr + fat_entry * sizeof(fat_dir_entry_t), sizeof(dir_entry));
  if (dir_entry.attribute & FILE_DIRECTORY)
    return true;
  return false;
}

bool fat_is_ro(uint16_t fat_entry)
{
  fat_dir_entry_t dir_entry;
  sdcard_read_offset(&dir_entry, dir_addr + fat_entry * sizeof(fat_dir_entry_t), sizeof(dir_entry));
  if (dir_entry.attribute & FILE_READ_ONLY)
    return true;
  return false;
}

bool fat_find_file(uint16_t* fat_entry, const char* filename)
{
  for (uint16_t i = 0; i < 512; i++)
  {
    char current_filename[FAT_FILENAME_STRLEN];

    fat_get_entry_name(current_filename, i);

    if (0 == strcmp(current_filename, filename))
    {
      *fat_entry = i;
      return true;
    }
  }

  return false;
}
