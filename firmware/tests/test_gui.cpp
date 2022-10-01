#include "catch.hpp"
#include <string.h>

// 2
#define BLUE_Y 0
// 1
#define ERROR_X 0
// 1
#define ERROR_Y 0
#define ERROR_H 4
#define ERROR_W 19

#define FONT_SIZE 1

#define DISPLAY_W ERROR_W * FONT_SIZE


// trim from left
inline std::string& ltrim(std::string& s, const char* t = " \t\n\r\f\v")
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from right
inline std::string& rtrim(std::string& s, const char* t = " \t\n\r\f\v")
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from left & right
inline std::string& trim(std::string& s, const char* t = " \t\n\r\f\v")
{
    return ltrim(rtrim(s, t), t);
}

using display_matrix_t = std::vector<std::vector<char>>;

typedef struct cursor_pos_str
{
  int x;
  int y;
} cursor_pos_str;

static display_matrix_t oled_display(ERROR_H, std::vector<char>(ERROR_W));
static cursor_pos_str cursor_pos = {0, 0};

static void oled_goto_char(int x, int y)
{
  cursor_pos.x = x * FONT_SIZE;
  cursor_pos.y = y;
}

static void oled_newline()
{
  oled_goto_char(0, cursor_pos.y + 1);
}

static void oled_putch(const char ch)
{
  // Ignore fonts not in table
  if (ch < ' ' || ch > '~')
    return;

  if (((cursor_pos.x + FONT_SIZE) >= DISPLAY_W) || (ch == '\n'))
  {
    /* If the cursor has reached to end of line on page1
     OR NewLine command is issued Then Move the cursor to next line */
    oled_newline();
  }

  if (ch != '\n')
  {
    oled_display.at(cursor_pos.y).at(cursor_pos.x) = ch;
    cursor_pos.x++;
  }
}

static void oled_putstr(const char* str)
{
  while (*str != '\0')
    oled_putch(*(str++));
}


void oled_gui_error(char* msg)
{
  oled_goto_char(ERROR_X, BLUE_Y + ERROR_Y);
  uint8_t space = ERROR_W;

  uint8_t y = 0;

  while (msg != NULL)
  {
    char *p = strsep((char **)&msg, " ");

    uint8_t word_len = strlen(p);

    // If word is longer than display width, skip it
    if (word_len > ERROR_W)
      continue;

    // If word is longer than available x space, put newline first
    if (word_len > space)
    {
      oled_goto_char(1, BLUE_Y + ERROR_Y + ++y);
      space = ERROR_W;
      if (y >= ERROR_H)
        break;
    }

    oled_putstr(p);
    oled_putch(' ');
    space -= (word_len);
  }
}

static void oled_home()
{
  oled_goto_char(0,0);
}

static void oled_clrscr()
{
  for (auto& line : oled_display)
    std::fill(line.begin(), line.end(), ' ');
  
  oled_home();
}

static std::string oled_getline(int line)
{
  std::string ret(oled_display.at(line).begin(), oled_display.at(line).end());
  return trim(ret);
}

TEST_CASE("Check the display interaction", "[oled]")
{
  oled_clrscr();

  SECTION("Clear screen")
  {
    REQUIRE(oled_getline(0) == "");
    REQUIRE(oled_getline(1) == "");
    REQUIRE(oled_getline(2) == "");
    REQUIRE(oled_getline(3) == "");
  }

  SECTION("Print a short string")
  {
    oled_putstr("Ciao!");
    REQUIRE(oled_getline(0) == "Ciao!");
    REQUIRE(oled_getline(1) == "");
    REQUIRE(oled_getline(2) == "");
    REQUIRE(oled_getline(3) == "");
  }

  SECTION("Print a longer string")
  {
    oled_putstr("Ciao Mondo stringa lunga");
    REQUIRE(oled_getline(0) == "Ciao Mondo stringa");
    REQUIRE(oled_getline(1) == "lunga");
    REQUIRE(oled_getline(2) == "");
    REQUIRE(oled_getline(3) == "");
  }

  SECTION("Print the error message")
  {
    char msg[] = "Ciao Mondo stringa lunga";
    oled_gui_error(msg);
    REQUIRE(oled_getline(0) == "Ciao Mondo stringa");
    REQUIRE(oled_getline(1) == "lunga");
    REQUIRE(oled_getline(2) == "");
    REQUIRE(oled_getline(3) == "");
  }

  SECTION("Print single word error message")
  {
    char msg[] = "Ciao";
    oled_gui_error(msg);
    REQUIRE(oled_getline(0) == "Ciao");
    REQUIRE(oled_getline(1) == "");
    REQUIRE(oled_getline(2) == "");
    REQUIRE(oled_getline(3) == "");
  }

  SECTION("Print empty")
  {
    char msg[] = "";
    oled_gui_error(msg);
    REQUIRE(oled_getline(0) == "");
    REQUIRE(oled_getline(1) == "");
    REQUIRE(oled_getline(2) == "");
    REQUIRE(oled_getline(3) == "");
  }
}