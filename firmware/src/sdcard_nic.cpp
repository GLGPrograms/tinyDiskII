#include "sdcard_nic.h"

#include <string.h>

#include "sdcard_fat.h"
#include "sdcard_gpio.h"
#include "sdcard.h"

#include "debug.h"

/* * * * * * * * * * * * * * * * PUBLIC MEMBERS * * * * * * * * * * * * * * * */

#define FAT_NIC_ELEMS (150)

static uint16_t nic_fat[FAT_NIC_ELEMS];
static bool is_file_selected = false;

/* * * * * * * * * * * * *  SHARED OR EXTERN MEMBERS  * * * * * * * * * * * * */

// WIP data shared with sdcard_nic for faster access to floppy disk files.
extern uint32_t fat_addr;
extern uint32_t sec_per_cluster;
extern uint8_t sectors_per_cluster2;
extern uint32_t dir_addr;
extern uint32_t data_addr;

/* * * * * * * * * * * * * * SHARED IMPLEMENTATIONS * * * * * * * * * * * * * */

bool nic_build_fat(uint16_t fat_entry)
{
  fat_dir_entry_t entry;
  uint16_t ft;
  size_t i;

  // Cleanup FAT before populating it
  memset(nic_fat, 0, sizeof(nic_fat));

  // Read address of first cluster in FAT
  if (!sdcard_read_offset(&entry, dir_addr + fat_entry * 32, sizeof(entry)))
    return false;
  nic_fat[0] = entry.first_cluster;

  for (i = 1; i < sizeof(nic_fat) / sizeof(*nic_fat); i++)
  {
    // Read next fat element in chain
    if (!sdcard_read_offset(&ft, (uint32_t)fat_addr + (uint32_t)nic_fat[i - 1] * 2, 2))
      return false;
    if (ft < 0x0002 || ft > 0xfff6)
      break;

    nic_fat[i] = ft;
  }

  debug_printP(PSTR("fat size: %x\n\r"), i);

  is_file_selected = true;

  return true;
}

bool nic_file_selected()
{
  return is_file_selected;
}

void nic_unselect_file()
{
  is_file_selected = false;
}

bool nic_update_sector(uint8_t dsk_trk, uint8_t dsk_sector)
{
  // Compute current disk index expressed in sectors (0..16*35)
  uint16_t dsk_index = (uint16_t)dsk_trk * 16 + dsk_sector;
  // Get current SD cluster index to find its address in FAT
  uint8_t sd_cluster = dsk_index >> sectors_per_cluster2;
  // Bytes per sector is assumed to be 512
  uint16_t sd_sector_offset = dsk_index % 4;

  // Check if FAT entry is valid
  if (nic_fat[sd_cluster] < 2)
    return false;

  // Get cluster from reordered FAT table and convert in sectors
  uint32_t sd_address = nic_fat[sd_cluster] - 2;
  sd_address <<= sectors_per_cluster2;
  // Add sector offset
  sd_address += sd_sector_offset;
  // Convert in bytes
  sd_address *= SDCARD_BLOCK_SIZE;

  sdcard_cs(0);
  uint8_t ret = 0;
  ret |= sdcard_command(SD_CMD_SET_BLOCKLEN, SDCARD_BLOCK_SIZE);
  ret |= sdcard_command(SD_CMD_READ_SINGLE_BLOCK, data_addr + sd_address);
  ret |= (sdcard_wait_for_data(0) != SD_STATE_START_DATA_BLOCK);
  sdcard_cs(1);

  if (ret)
    debug_printP(PSTR("Fail updt t:%d s:%d\n\r"), dsk_trk, dsk_sector);

  return true;
}

uint8_t nic_get_bit()
{
  sdcard_clk(1);
  uint8_t bit = sdcard_di();
  sdcard_clk(0);
  return bit;
}

void nic_abort_read(uint16_t bits)
{
  for (; bits < (514 * 8); bits++)
  {
    sdcard_clk(1);
    sdcard_clk(0);
  }
}

bool nic_write_sector(uint8_t *buffer, uint8_t volume, uint8_t track, uint8_t sector)
{
  uint8_t c;

  // Compute current sector index (0..16*35) FIXME
  uint16_t dsk_sector = (uint16_t)track * 16 + sector;
  // Get current SD cluster index to find its address in FAT
  uint8_t sd_cluster = dsk_sector >> sectors_per_cluster2;
  // Bytes per sector is assumed to be 512
  uint16_t sd_sector_offset = dsk_sector % 4;

  // Get cluster from reordered FAT table and convert in sectors
  uint32_t sd_address = nic_fat[sd_cluster] - 2;
  sd_address <<= sectors_per_cluster2;
  // Add sector offset
  sd_address += sd_sector_offset;
  // Convert in bytes
  sd_address *= SDCARD_BLOCK_SIZE;

  sdcard_cs(0);
  sdcard_command(SD_CMD_WRITE_BLOCK, (data_addr + sd_address));

  write_byte(0xff);
  write_byte(0xfe);
  // 22 ffs
  sdcard_do(1);
  for (uint8_t i = 0; i < 22 * 8; i++)
  {
    sdcard_clk(1);
    sdcard_clk(0);
  }

  // sync header
  write_byte(0x03);
  write_byte(0xfc);
  write_byte(0xff);
  write_byte(0x3f);
  write_byte(0xcf);
  write_byte(0xf3);
  write_byte(0xfc);
  write_byte(0xff);
  write_byte(0x3f);
  write_byte(0xcf);
  write_byte(0xf3);
  write_byte(0xfc);

  // address header
  write_byte(0xD5);
  write_byte(0xAA);
  write_byte(0x96);
  write_byte((volume >> 1) | 0xAA);
  write_byte(volume | 0xAA);
  write_byte((track >> 1) | 0xAA);
  write_byte(track | 0xAA);
  write_byte((sector >> 1) | 0xAA);
  write_byte(sector | 0xAA);
  c = (volume ^ track ^ sector);
  write_byte((c >> 1) | 0xAA);
  write_byte(c | 0xAA);
  write_byte(0xDE);
  write_byte(0xAA);
  write_byte(0xEB);

  // sync header
  write_byte(0xff);
  write_byte(0xff);
  write_byte(0xff);
  write_byte(0xff);
  write_byte(0xff);

  // Put actual data
  for (uint16_t i = 0; i < 349; i++)
    write_byte(buffer[i]);

  // 14 ffs
  for (uint8_t i = 0; i < 14; i++)
    write_byte(0xff);

  // 96 00s
  for (uint16_t i = 0; i < 96; i++)
    write_byte(0x00);

  write_byte(0xff);
  write_byte(0xff);

  read_byte();

  // wait until data is written to the SD card
  sdcard_wait_for_data(0);

  sdcard_cs(1);

  return true;
}
