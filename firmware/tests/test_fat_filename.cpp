#include "catch.hpp"
#include <inttypes.h>
#include <string.h>
#include <string>

#include "../src/sdcard_fat.h"

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

TEST_CASE("To Fat Filename", "[to_fat_filename]")
{
  std::vector<uint8_t> to(FAT_FILENAME_SIZEOF);
  std::string from;

  from = std::string("test");
  to_fat_filename((char *)to.data(), from.data());
  REQUIRE(memcmp("TEST       ", to.data(), FAT_FILENAME_SIZEOF) == 0);

  from = std::string("helo.bin");
  to_fat_filename((char *)to.data(), from.data());
  REQUIRE(memcmp("HELO    BIN", to.data(), FAT_FILENAME_SIZEOF) == 0);

  from = std::string("12345678901");
  to_fat_filename((char *)to.data(), from.data());
  REQUIRE(memcmp("12345678901", to.data(), FAT_FILENAME_SIZEOF) == 0);

  from = std::string("12345678.exe");
  to_fat_filename((char *)to.data(), from.data());
  REQUIRE(memcmp("12345678EXE", to.data(), FAT_FILENAME_SIZEOF) == 0);

  from = std::string(".exe");
  to_fat_filename((char *)to.data(), from.data());
  REQUIRE(memcmp("        EXE", to.data(), FAT_FILENAME_SIZEOF) == 0);

  from = std::string(".");
  to_fat_filename((char *)to.data(), from.data());
  REQUIRE(memcmp("           ", to.data(), FAT_FILENAME_SIZEOF) == 0);

  from = std::string("..");
  to_fat_filename((char *)to.data(), from.data());
  REQUIRE(memcmp("           ", to.data(), FAT_FILENAME_SIZEOF) == 0);
}

TEST_CASE("String", "[from_fat_filename]")
{
  uint8_t to[FAT_FILENAME_STRLEN + 1];

  // Add a canary byte to check for buffer overflows
  to[FAT_FILENAME_STRLEN] = 0x42;

  SECTION("Regular filename")
  {
    from_fat_filename((char *)to, "TEST    TXT");
    REQUIRE(strcmp((char *)to, "TEST.TXT") == 0);
  }

  SECTION("Full length filename")
  {
    from_fat_filename((char *)to, "TESTTESTTXT");
    REQUIRE(strcmp((char *)to, "TESTTEST.TXT") == 0);
  }

  SECTION("Directory (no extension)")
  {
    from_fat_filename((char *)to, "TEST       ");
    REQUIRE(strcmp((char *)to, "TEST") == 0);
  }

  SECTION("Only extension (is that even possible?)")
  {
    from_fat_filename((char *)to, "        TXT");
    REQUIRE(strcmp((char *)to, ".TXT") == 0);
  }

  SECTION("Current directory")
  {
    from_fat_filename((char *)to, ".          ");
    REQUIRE(strcmp((char *)to, ".") == 0);
  }

  SECTION("Parent directory")
  {
    from_fat_filename((char *)to, "..         ");
    REQUIRE(strcmp((char *)to, "..") == 0);
  }

  REQUIRE(to[FAT_FILENAME_STRLEN] == 0x42);
}