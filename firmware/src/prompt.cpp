#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include <util/delay.h>

#include "prompt.h"

#include "util/port.h"
#include "util/utils.h"

#include "floppy_main.h"
#include "sdcard.h"
#include "sdcard_fat.h"
#include "sdcard_nic.h"

#include "debug.h"

/* * * * * * * * * * * * * * * * INTERNAL TYPES * * * * * * * * * * * * * * * */

struct prompt_commands_t {
  const char* cmd;
  void (*do_cmd)(const char *arg, size_t len);
};

/* * * * * * * * * * * * * * * * PUBLIC MEMBERS * * * * * * * * * * * * * * * */

#define DEBUG_UART C, 0
#define UART_BAUDRATE  (115200)
// Clock is always between 2MHz and 30MHz, this BSCALE is always good
#define DEBUG_UART_BSCALE (-6)
#if DEBUG_UART_BSCALE < 0
#define DEBUG_UART_BSEL (_BV(-DEBUG_UART_BSCALE) * ((F_CPU)/(16*UART_BAUDRATE) - 1))
#else
#define DEBUG_UART_BSEL ((F_CPU)/(_BV(DEBUG_UART_BSCALE)*16*UART_BAUDRATE) - 1)
#endif

#define BUFFER_MAXSIZE 32

// Send echoes while writing in prompt
#define ECHO 1

// Buffers for received data
static char buffer[BUFFER_MAXSIZE];
static volatile uint8_t size = 0;
static volatile uint8_t ready = 0;
// Buffer for printf routines
static char printf_buf[BUFFER_MAXSIZE];

/* * * * * * * * * * * * * * PUBLIC IMPLEMENTATIONS * * * * * * * * * * * * * */

/* - - - - - - - - - - - - - - - print routines - - - - - - - - - - - - - - - */

static void putChar(char c)
{
  // wait the data register to be empty
  while (!(Uart(DEBUG_UART).STATUS & USART_DREIF_bm))
    ;
  Uart(DEBUG_UART).DATA = c;
}

/* - - - - - - - - - - - -  prompt commands routines - - - - - - - - - - - -  */

static void do_cwd(const char *arg, size_t len __attribute__((unused)))
{
  uint16_t fat_entry = 0;

  if (!fat_find_file(&fat_entry, arg))
  {
    printf_P(PSTR("Failed finding directory\n\r"));
    return;
  }

  fat_cwd(fat_entry);
}

static void do_up(const char *arg __attribute__((unused)), size_t len __attribute__((unused)))
{
  uint16_t fat_entry = 0;

  if (!fat_find_file(&fat_entry, ".."))
  {
    printf_P(PSTR("Failed finding parent\n\r"));
    return;
  }

  fat_cwd(fat_entry);
}

static void do_lst(const char *arg __attribute__((unused)), size_t len __attribute__((unused)))
{
  uint16_t fat_handle = 0;

  do {
    char filename[FAT_FILENAME_STRLEN];
    fat_get_entry_name(filename, fat_handle);
    printf_P(PSTR("%s\n\r"), filename);
  } while (fat_next(&fat_handle));
}

static void do_set(const char *arg, size_t len __attribute__((unused)))
{
  uint16_t fat_entry;

  printf_P(PSTR("\n\r"));

  if (!fat_find_file(&fat_entry, arg))
  {
    printf_P(PSTR("Failed finding file\n\r"));
    return;
  }

  if (!nic_build_fat(fat_entry))
  {
    printf_P(PSTR("Failed building FAT\n\r"));
    return;
  }


  printf_P(PSTR("Selected image\n\r"));
}

static void do_rem(const char *arg __attribute__((unused)), size_t len __attribute__((unused)))
{
  nic_unselect_file();
}

static void do_init(const char *arg __attribute__((unused)), size_t len __attribute__((unused)))
{
#if 0
  sdcard_insert(true);
#endif
  if (!sdcard_card_init() || !sdcard_fat_init())
  {
#if 0
    sdcard_insert(false);
#endif
    debug_printP(PSTR("Failed initialization\n\r"));
  }
}

static const prompt_commands_t prompt_commands[] = {
  {"CWD", do_cwd},
  {"UP", do_up},
  {"LST", do_lst},
  {"SET", do_set},
  {"REM", do_rem},
  {"INIT", do_init},
  {NULL, NULL}
};

/* * * * * * * * * * * * * * SHARED IMPLEMENTATIONS * * * * * * * * * * * * * */

void prompt_init()
{
  // Set-up uart TX pin
  Port(DEBUG_UART).OUTSET = _BV(PinTX(DEBUG_UART)) | _BV(PinRX(DEBUG_UART));
  Port(DEBUG_UART).DIRSET = _BV(PinTX(DEBUG_UART));

  // Set-up a 115200 uart
  Uart(DEBUG_UART).BAUDCTRLA = low(DEBUG_UART_BSEL);
  Uart(DEBUG_UART).BAUDCTRLB = high(DEBUG_UART_BSEL) | DEBUG_UART_BSCALE << 4;

  // USART interrupt (MID priority)
  Uart(DEBUG_UART).CTRLA = USART_RXCINTLVL_MED_gc;

  // Async, 8 bit, no parity, one stop bit
  Uart(DEBUG_UART).CTRLC = USART_CMODE_ASYNCHRONOUS_gc |
                           USART_CHSIZE_8BIT_gc | USART_PMODE_DISABLED_gc;
  Uart(DEBUG_UART).CTRLB = USART_TXEN_bm | USART_RXEN_bm;
}

void prompt_main()
{
  if (ready)
  {
    for (size_t i = 0; prompt_commands[i].cmd != NULL; i++)
    {
      const prompt_commands_t* cmd = &prompt_commands[i];
      size_t cmd_len = strlen(cmd->cmd);
      if (0 == strncmp(buffer, cmd->cmd, cmd_len))
        // Remember to skip cmd and space
        cmd->do_cmd(buffer + cmd_len + 1, size - cmd_len - 1);
    }
    printf_P(PSTR("Ready.\n\r"));
    ready = 0;
    size = 0;
  }
}

/* - - - - - - - - - - - - - - -  tx routines - - - - - - - - - - - - - - - - */

int puts(const char *c)
{
  for (uint8_t i = 0; *(c + i) != '\0'; i++)
    putChar(*(c + i));
  return 0;
}

int printf_P(const char *__fmt, ...)
{
  va_list lst;
  va_start(lst, __fmt);
  vsnprintf_P(printf_buf, sizeof(printf_buf), __fmt, lst);
  puts(printf_buf);
  va_end(lst);
  return 1;
}

/* - - - - - - - - - - - - - - - - rx routines  - - - - - - - - - - - - - - - */

ISR(ISR_RXC(DEBUG_UART))
{
  char ch = Uart(DEBUG_UART).DATA;

  // Avoid messing up with already committed buffer
  if (ready)
    return;

  if (ECHO)
    Uart(DEBUG_UART).DATA = ch;

  // Allow backspace
  if (ch == 0x1F || ch == 0x08)
  {
    size--;
    return;
  }

  // Ignore newlines
  if (ch == '\n')
    return;

  // Commit buffer when enter is pressed or if buffer is filled-up
  if (ch == '\r' || size == BUFFER_MAXSIZE)
  {
    ready = 1;
    buffer[size] = '\0';
    return;
  }

  ch = toupper(ch);

  buffer[size++] = ch;
}
