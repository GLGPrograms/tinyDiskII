/* * * * * * * * * * * * * * * * *  INCLUDES  * * * * * * * * * * * * * * * * */
#include "sdcard.h"
#include "sdcard_gpio.h"

#include <ctype.h>
#include <string.h>

#include "util/macro.h"

#include "debug.h"

/* * * * * * * * * * * * * * *  EXTERN VARIABLES  * * * * * * * * * * * * * * */

// Shared with sdcard_fat for sector caching
extern uint8_t sector_cache[SDCARD_BLOCK_SIZE];

/* * * * * * * * * * * * * * *  PRIVATE TYPEDEFS  * * * * * * * * * * * * * * */

enum sdcard_type_t
{
  SD_SD1,
  SD_SD2,
  SD_SDHC
};

/* * * * * * * * * * * * * STATIC FUNCTION PROTOTYPES * * * * * * * * * * * * */

/* * * * * * * * * * * * * * *  STATIC VARIABLES  * * * * * * * * * * * * * * */

static sdcard_type_t type = SD_SD1;

// Index of currently cached sector
static uint32_t sector_cache_idx = 0x00;

/* * * * * * * * * * * * * * *  GLOBAL VARIABLES  * * * * * * * * * * * * * * */

// Function pointers for byte-oriented SD-card operations
uint8_t (*read_byte)() = NULL;
void (*write_byte)(uint8_t) = NULL;

/* * * * * * * * * * * * * * *  GLOBAL FUNCTIONS  * * * * * * * * * * * * * * */

void sdcard_init()
{
  sdcard_gpio_init();
  sdcard_set_mode(BITBANG_SLOW, 1);
}

/* - - - - - - - - - - - - sdcard low level utilities - - - - - - - - - - - - */

void sdcard_set_mode(sdcard_mode_t mode, bool cs_status)
{
  if (mode >= BITBANG_SLOW)
  {
    sdcard_gpio_bitbang_init();
    read_byte = (mode == BITBANG_SLOW ? read_byte_slow : read_byte_fast);
    write_byte = (mode == BITBANG_SLOW ? write_byte_slow : write_byte_fast);
    sdcard_cs(cs_status);
  }
  else
  {
    sdcard_gpio_spi_init();
    read_byte = read_byte_spi;
    write_byte = write_byte_spi;
    sdcard_cs(1);
  }
}
/* - - - - - - - - - - - - - - - sdcard commands  - - - - - - - - - - - - - - */

bool sdcard_wait_for_status(uint8_t command, uint8_t status, uint16_t timeout)
{
  // Arbitrary timeout
  if (timeout == 0)
    timeout = 0xFFFF;

  for (uint16_t i = 0; i < timeout; i++)
    if (sdcard_command(command, 0) == status)
      return true;

  return false;
}

bool sdcard_wait_for_astatus(uint8_t command, uint32_t arg, uint8_t status, uint16_t timeout)
{
  // Arbitrary timeout
  if (timeout == 0)
    timeout = 0xFFFF;

  for (uint16_t i = 0; i < timeout; i++)
    if (sdcard_acommand(command, arg) == status)
      return true;

  return false;
}

uint8_t sdcard_wait_for_data(uint16_t timeout)
{
  uint8_t status;

  // Arbitrary timeout
  if (timeout == 0)
    timeout = 0xFFFF;

  for (uint16_t i = 0; i < timeout; ++i)
  {
    status = read_byte();
    if (status != 0xff)
      break;
  }
  return status;
}

uint8_t sdcard_command(uint8_t command, uint32_t argument)
{
  // Wait for a few clock cycles.
  read_byte();
  write_byte(command | 0x40);
  write_byte(argument >> 24);
  write_byte((argument >> 16) & 0xff);
  write_byte((argument >> 8) & 0xff);
  write_byte(argument & 0xff);
  uint8_t crc = 0xff;

  if (command == SD_CMD_GO_IDLE_STATE)
    crc = 0x95;
  if (command == SD_CMD_SEND_IF_COND)
    crc = 0x87;

  uint8_t status;
  write_byte(crc);
  for (uint8_t i = 0; i < 20; i++)
  {
    status = read_byte();
    if (!(status & 0x80))
    {
      break;
    }
  }
  return status;
}

uint8_t sdcard_acommand(uint8_t command, uint32_t argument)
{
  sdcard_command(SD_CMD_APP_CMD, 0);
  return sdcard_command(command, argument);
}

bool sdcard_read_data(uint8_t *data, uint16_t size)
{
  if (sdcard_wait_for_data(0) != SD_STATE_START_DATA_BLOCK)
    return false;

  while (size--)
  {
    *data++ = read_byte();
  }

  // CRC
  read_byte();
  read_byte();

  return true;
}

/* - - - - - - - - - - - - - High-level methods - - - - - - - - - - - - - - - */

bool sdcard_card_init()
{
  bool ret = false;
  uint16_t i;

  // SDCard initialization must use lower speed
  sdcard_set_mode(BITBANG_SLOW, 1);

  // 200 clock pulses with CS not asserted
  sdcard_cs(1);
  for (i = 200; i; i--)
    read_byte();

  // initialize the SD card
  // 75 clock pulses
  sdcard_cs(0);
  for (i = 75; i; i--)
    read_byte();

  // command 0 - wait for idle
  debug_printP(PSTR("CMD0\n\r"));
  if (!sdcard_wait_for_status(0, R1_IDLE, 200))
    goto failure;

  // command 8 - SD version
  debug_printP(PSTR("CMD8\n\r"));
  if (sdcard_command(8, 0x1AA) & R1_ILLEGAL_CMD)
  {
    debug_printP(PSTR("SDv1\n\r"));
    type = SD_SD1;
  }
  else
  {
    read_byte();
    read_byte();
    // Check that the card operates in the 2.7-3.6V range.
    if (!(read_byte() & 0x01))
      goto failure;
    // Check the test pattern.
    if (read_byte() != 0xaa)
      goto failure;
    debug_printP(PSTR("SDv2 or SDHC\n\r"));
    type = SD_SD2;
  }

  // Initialize card
  debug_printP(PSTR("CMD55&ACMD41\n\r"));
  if (!(sdcard_wait_for_astatus(SD_AMCD_SD_SEND_OP_COND, type == SD_SD2 ? 0x40000000 : 0, 0x00, 200)))
    goto failure;

  // Check for SDHC
  if (type == SD_SD2)
  {
    debug_printP(PSTR("Check SDHC\n\r"));
    if (sdcard_command(SD_CMD_READ_OCR, 0))
      goto failure;
    if ((read_byte() & 0x40))
      type = SD_SDHC;
    read_byte();
    read_byte();
    read_byte();
  }
  else
  {
    debug_printP(PSTR("Set blocklen\n\r"));
    if (sdcard_command(SD_CMD_SET_BLOCKLEN, SDCARD_BLOCK_SIZE))
      goto failure;
  }

  debug_printP(PSTR("SDCard type: "));
  switch(type) {
    case SD_SD1:
      debug_printP(PSTR("SD1\n\r"));
      break;
    case SD_SD2:
      debug_printP(PSTR("SD2\n\r"));
      break;
    case SD_SDHC:
      debug_printP(PSTR("SDHC\n\r"));
      break;
    default:
      debug_printP(PSTR("unknown"));
  }
  debug_printP(PSTR("\n\r"));

  // Now, higher speed may be used
  sdcard_set_mode(SPI, 1);

  ret = true;

failure:
  // Initialization is completed, with success or failure
  return ret;
}

bool sdcard_read_sector(uint8_t *data, uint32_t start)
{
  bool ret = false;

  sdcard_cs(0);

  if (sdcard_command(SD_CMD_SET_BLOCKLEN, SDCARD_BLOCK_SIZE) != 0)
    goto fail;

  if (type != SD_SDHC)
    start *= SDCARD_BLOCK_SIZE;

  if (sdcard_command(SD_CMD_READ_SINGLE_BLOCK, start) != 0)
    goto fail;

  ret = sdcard_read_data(data, SDCARD_BLOCK_SIZE);

  ret = true;

fail:

  sdcard_cs(1);
  return ret;
}

bool sdcard_read_offset(void *data, uint32_t offset, size_t len)
{
  bool ret = false;

  if ((offset / SDCARD_BLOCK_SIZE) != sector_cache_idx)
  {
    sdcard_cs(0);
    if (sdcard_command(SD_CMD_SET_BLOCKLEN, SDCARD_BLOCK_SIZE) != 0)
      goto fail;
    if (sdcard_command(SD_CMD_READ_SINGLE_BLOCK, (offset / SDCARD_BLOCK_SIZE) * SDCARD_BLOCK_SIZE) != 0)
      goto fail;
    ret = sdcard_read_data((uint8_t*)sector_cache, SDCARD_BLOCK_SIZE);

    sector_cache_idx = offset / SDCARD_BLOCK_SIZE;

fail:
    sdcard_cs(1);
  }
  else
    ret = true;

  memcpy(data, sector_cache + (offset % SDCARD_BLOCK_SIZE), len);

  return ret;
}
