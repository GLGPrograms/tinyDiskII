#include "sdcard.h"

#include <avr/io.h>
#include <util/delay.h>
#include <ctype.h>

#include "util/macro.h"
#include "util/port.h"

#include "debug.h"

/* * * * * * * * * * * * * * * * PUBLIC MEMBERS * * * * * * * * * * * * * * * */
// IO defines
#define SDCARD_DO C, 5
#define SDCARD_CS C, 4
#define SDCARD_CLK C, 7
#define SDCARD_DI C, 6
#define SDCARD_PRES D, 3
#define SDCARD_LOCK D, 2

/* * * * * * * * * * * * *  SHARED OR EXTERN MEMBERS  * * * * * * * * * * * * */

void sdcard_gpio_init()
{
  // Even if SPI Master peripheral is enabled, pin modes must be manually specified
  Port(SDCARD_DO).OUTCLR = PinMsk(SDCARD_CLK) | PinMsk(SDCARD_DO) | PinMsk(SDCARD_CS);
  Port(SDCARD_DO).DIRSET = PinMsk(SDCARD_CLK) | PinMsk(SDCARD_DO) | PinMsk(SDCARD_CS);

  // SD Card sense pins as input with pullup
  Port(SDCARD_PRES).DIRCLR = PinMsk(SDCARD_PRES);
  Port(SDCARD_LOCK).DIRCLR = PinMsk(SDCARD_LOCK);
  PinCtrl(SDCARD_PRES) = PORT_OPC_PULLUP_gc;
  PinCtrl(SDCARD_LOCK) = PORT_OPC_PULLUP_gc;

  // Configure SPI, but keep it disabled
  // MSbit first, master mode, SPI1 mode (ckrising = setup, ckrising = sample), /64 prescaler
  SPIC.CTRL = SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV64_gc;
}

void sdcard_gpio_bitbang_init()
{
  SPIC.CTRL &= ~SPI_ENABLE_bm;
}

void sdcard_gpio_spi_init()
{
  SPIC.CTRL |= SPI_ENABLE_bm;
}

/* ----------------------- sdcard low level utilities ----------------------- */

bool sdcard_present()
{
  return !(Port(SDCARD_PRES).IN & PinMsk(SDCARD_PRES));
}

bool sdcard_locked()
{
  return Port(SDCARD_LOCK).IN & PinMsk(SDCARD_LOCK);
}

void sdcard_do(uint8_t i)
{
  if (i)
    Port(SDCARD_DO).OUTSET = PinMsk(SDCARD_DO);
  else
    Port(SDCARD_DO).OUTCLR = PinMsk(SDCARD_DO);
}

void sdcard_cs(uint8_t i)
{
  if (i)
    Port(SDCARD_CS).OUTSET = PinMsk(SDCARD_CS);
  else
    Port(SDCARD_CS).OUTCLR = PinMsk(SDCARD_CS);
}

void sdcard_clk_toggle()
{
  Port(SDCARD_CLK).OUTTGL = PinMsk(SDCARD_CLK);
}

void sdcard_clk(uint8_t i)
{
  if (i)
    Port(SDCARD_CLK).OUTSET = PinMsk(SDCARD_CLK);
  else
    Port(SDCARD_CLK).OUTCLR = PinMsk(SDCARD_CLK);
}

uint8_t sdcard_di()
{
  if (Port(SDCARD_DI).IN & PinMsk(SDCARD_DI))
    return 1;
  return 0;
}

/* ------------------ byte-level sdcard interface routines ------------------ */

uint8_t read_byte_spi(void)
{
  SPIC.DATA = 0xFF;
  while (!(SPIC.STATUS & SPI_IF_bm))
    ;
  SPIC.STATUS |= SPI_IF_bm;
  return SPIC.DATA;
}

void write_byte_spi(uint8_t c)
{
  SPIC.DATA = c;
  while (!(SPIC.STATUS & SPI_IF_bm))
    ;
  SPIC.STATUS |= SPI_IF_bm;
}

uint8_t read_byte_slow(void)
{
  uint8_t c = 0;
  volatile uint8_t i;

  sdcard_do(1);
  _delay_us(4);
  for (i = 0; i != 8; i++)
  {
    sdcard_clk(1);
    c = ((c << 1) | (sdcard_di()));
    _delay_us(4);
    sdcard_clk(0);
    _delay_us(4);
  }
  return c;
}

void write_byte_slow(uint8_t c)
{
  uint8_t d;
  for (d = 0b10000000; d; d >>= 1)
  {
    if (c & d)
      sdcard_do(1);
    else
      sdcard_do(0);

    _delay_us(4);
    sdcard_clk(1);
    _delay_us(4);
    sdcard_clk(0);
  }
}

uint8_t read_byte_fast(void)
{
  uint8_t c = 0;
  volatile uint8_t i;

  sdcard_do(1);

  for (i = 0; i != 8; i++)
  {
    sdcard_clk(1);
    c = ((c << 1) | (sdcard_di()));
    sdcard_clk(0);
  }
  return c;
}

void write_byte_fast(uint8_t c)
{
  uint8_t d;
  for (d = 0b10000000; d; d >>= 1)
  {
    if (c & d)
      sdcard_do(1);
    else
      sdcard_do(0);
    sdcard_clk(1);
    sdcard_clk(0);
  }
  sdcard_do(0);
}
