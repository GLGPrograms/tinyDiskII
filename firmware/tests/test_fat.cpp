#include "catch.hpp"
#include <string>
#include <fstream>
#include <iostream>

#include "../src/sdcard_fat.h"

static const char* SDCARD_FILENAME = "SAMPLEDISK.bin";
static uint32_t SDCARD_SECTOR_SIZE = 512;

struct sdcard_ctx_t
{
  std::ifstream sdcard;
};

static sdcard_ctx_t sdcard_ctx;

// Fake sdcard handler
static void sdcard_setup()
{
  sdcard_ctx.sdcard = std::ifstream(SDCARD_FILENAME);

  REQUIRE(sdcard_fat_init() == true);
}

static void sdcard_teardown()
{
  sdcard_ctx.sdcard.close();
}

bool sdcard_read_offset(void *data, uint32_t offset, size_t len)
{
  if (!sdcard_ctx.sdcard.is_open())
    throw std::runtime_error("SDCard is not opened");

  sdcard_ctx.sdcard.seekg(offset, std::ios::beg);
  sdcard_ctx.sdcard.read((char*)data, len);
  return true;
}

bool sdcard_read_sector(uint8_t *data, uint32_t start)
{
  return sdcard_read_offset(data, start * SDCARD_SECTOR_SIZE, SDCARD_SECTOR_SIZE);
}

TEST_CASE("", "[fat_root_ls]")
{
  sdcard_setup();

  SECTION("root directory listing")
  {
    uint16_t fat_entry = 0xFFFF;

    int file_no = 0;
    int dir_no = 0;
    
    while(fat_next(&fat_entry))
    {
      fat_is_directory(fat_entry) ? dir_no++ : file_no++;
    }

    REQUIRE(file_no == 3);
    REQUIRE(dir_no == 3);
  }

  SECTION("subdirectory directory listing")
  {
    uint16_t subfolder;
    REQUIRE(fat_find_file(&subfolder, "GAMES") == true);
    REQUIRE(fat_cwd(subfolder) == true);

    int file_no = 0;
    int dir_no = 0;
    
    uint16_t fat_entry = 0xFFFF;
    while (fat_next(&fat_entry))
    {
      fat_is_directory(fat_entry) ? dir_no++ : file_no++;
    }

    REQUIRE(file_no == 2);
    REQUIRE(dir_no == 1);
  }

  SECTION("forward and backward listing")
  {
    int file_no = 0;
    int dir_no = 0;
    
    uint16_t fat_entry = 0xFFFF;

    while (fat_next(&fat_entry))
    {
      fat_is_directory(fat_entry) ? dir_no++ : file_no++;
    }
    
    do
    {
      fat_is_directory(fat_entry) ? dir_no-- : file_no--;
    }
    while (fat_prev(&fat_entry));

    REQUIRE(file_no == 0);
    REQUIRE(dir_no == 0);
  }

  SECTION("Moving to parent folder from subfolder")
  {
    // Move in subfolder
    uint16_t subfolder;
    REQUIRE(fat_find_file(&subfolder, "GAMES") == true);
    REQUIRE(fat_cwd(subfolder) == true);

    // Search for ".." entry
    uint16_t dotdot;
    REQUIRE(fat_find_file(&dotdot, "..") == true);
    REQUIRE(fat_cwd(dotdot) == true);
    
    uint16_t subfolder_check;
    REQUIRE(fat_find_file(&subfolder_check, "GAMES") == true);

    REQUIRE(subfolder_check == subfolder);
  }

  SECTION("Check listing of empty folder")
  {
    uint16_t subfolder;
    REQUIRE(fat_find_file(&subfolder, "EMPTY") == true);
    REQUIRE(fat_cwd(subfolder) == true);

    int file_no = 0;
    int dir_no = 0;
    
    uint16_t fat_entry = 0xFFFF;
    while (fat_next(&fat_entry))
    {
      fat_is_directory(fat_entry) ? dir_no++ : file_no++;
    }

    REQUIRE(file_no == 0);
    REQUIRE(dir_no == 1);
  }

  sdcard_teardown();
}
