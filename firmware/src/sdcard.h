/**
 * @file sdcard.h
 * @author giuliof (giulio@glgprograms.it)
 * @brief SD card interfacing methods.
 * @version 0.1
 * @date 2022-04-24
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef SDCARD_H_
#define SDCARD_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum sdcard_mode_t
{
  // Used during data writing and FAT navigation
  SPI = 0,
  // Used for initialization phase only
  BITBANG_SLOW = 1,
  // Used for single-bit reading
  BITBANG_FAST = 3
};

// Low level methods
extern uint8_t (*read_byte)();
extern void (*write_byte)(uint8_t);
void sdcard_set_mode(sdcard_mode_t mode, bool cs_status);

// Commands

enum sd_commands_t
{
  SD_CMD_GO_IDLE_STATE = 0,
  SD_CMD_SEND_IF_COND = 8,
  SD_CMD_SEND_CSD = 9,
  SD_CMD_SEND_CID = 10,
  SD_CMD_STOP_TRANSMISSION = 12,
  SD_CMD_SEND_STATUS = 13,
  SD_CMD_SET_BLOCKLEN = 16,
  SD_CMD_READ_SINGLE_BLOCK = 17,
  SD_CMD_READ_MULTIPLE_BLOCK = 18,
  SD_CMD_WRITE_BLOCK = 24,
  SD_CMD_WRITE_MULTIPLE_BLOCK = 25,
  SD_CMD_ERASE_WR_BLK_START = 32,
  SD_CMD_ERASE_WR_BLK_END = 33,
  SD_CMD_ERASE = 38,
  SD_CMD_APP_CMD = 55,
  SD_CMD_READ_OCR = 58,
};

enum sd_app_commands_t
{
  SD_AMCD_SET_WR_BLK_ERASE_COUNT = 23,
  SD_AMCD_SD_SEND_OP_COND = 41,
};

enum
{
  R1_IDLE = 1 << 0,
  R1_ERASE_RESET = 1 << 1,
  R1_ILLEGAL_CMD = 1 << 2,
  R1_CRC_ERR = 1 << 3,
  R1_ERASE_SEQ_ERR = 1 << 4,
  R1_ADDR_ERR = 1 << 5,
  R1_PARAM_ERR = 1 << 6,
  R1_START_BIT = 1 << 7,
  //
  R1_ERROR_bm = (R1_ILLEGAL_CMD | R1_CRC_ERR | R1_ERASE_SEQ_ERR | R1_ADDR_ERR | R1_PARAM_ERR)
};

#define SD_STATE_START_DATA_BLOCK (0xfe)

const uint16_t SDCARD_BLOCK_SIZE = 512;

bool sdcard_wait_for_status(uint8_t command, uint8_t status, uint16_t timeout);
bool sdcard_wait_for_astatus(uint8_t command, uint32_t arg, uint8_t status, uint16_t timeout);
uint8_t sdcard_wait_for_data(uint16_t timeout);
uint8_t sdcard_command(uint8_t command, uint32_t argument);
uint8_t sdcard_acommand(uint8_t command, uint32_t argument);
uint32_t addressToBlock(uint32_t address);

// Initialization methods

void sdcard_init();
bool sdcard_card_init();
bool sdcard_read_sector(uint8_t *data, uint32_t start);
bool sdcard_read_offset(void *data, uint32_t offset, size_t len);

#endif // SRC_SDCARD_H_