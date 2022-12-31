/* * * * * * * * * * * * * * * * *  INCLUDES  * * * * * * * * * * * * * * * * */
#include "sdcard_gpio.h"

#include <avr/io.h>
#include <util/delay.h>
#include <ctype.h>

#include "sdcard.h"
#include "util/macro.h"
#include "util/port.h"

#include "debug.h"

/* * * * * * * * * * * * * * * * PUBLIC MEMBERS * * * * * * * * * * * * * * * */
// IO defines
#define SDCARD_CS  C, 4
#define SDCARD_CLK C, 5
#define SDCARD_DI  C, 6
#define SDCARD_DO  C, 7
#define SDCARD_PRES D, 3
#define SDCARD_LOCK D, 2

// Lo-speed baudrate (50kHz)
#define MSPI_LO_BAUDRATE  (50000)
// See AVR 8077 21.3.1 Internal Clock Generation
#define DEBUG_UART_BSEL   ((F_CPU)/(2*MSPI_LO_BAUDRATE) - 1)

static void sdcard_gpio_hispeed(bool hispeed);
static uint8_t transceive_spi(uint8_t c);

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

  // Enable syncronous UART
  sdcard_gpio_bitbang_init();
  // All interrupts disabled
  USARTC1.CTRLA = USART_RXCINTLVL_OFF_gc | USART_TXCINTLVL_OFF_gc | USART_DREINTLVL_OFF_gc;
  // Master SPI mode
  USARTC1.CTRLC = USART_CMODE_MSPI_gc;
  // Enable both transmitter and receiver
  USARTC1.CTRLB = (USART_RXEN_bm | USART_TXEN_bm);
}

// Switch from lo-speed and hi-speed SPI
static void sdcard_gpio_hispeed(bool hispeed)
{
  // FIXME should disable RX/TX while changing baudrate
  // This causes issues...
  // USARTC1.CTRLB = 0x00;

  // Note: BSCALE is ignored in MSPI mode
  USARTC1.BAUDCTRLA = hispeed ?  0x00 : LSB16(DEBUG_UART_BSEL);
  USARTC1.BAUDCTRLB = hispeed ?  0x00 : MSB16(DEBUG_UART_BSEL);

  // USARTC1.CTRLB = (USART_RXEN_bm | USART_TXEN_bm);
}

// Put SPI communication in lo-speed mode (sd card initialization)
void sdcard_gpio_bitbang_init()
{
  sdcard_gpio_hispeed(false);
}

// Put SPI communication in hi-speed mode
void sdcard_gpio_spi_init()
{
  sdcard_gpio_hispeed(true);
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

void sdcard_cs(uint8_t i)
{
  if (i)
    Port(SDCARD_CS).OUTSET = PinMsk(SDCARD_CS);
  else
    Port(SDCARD_CS).OUTCLR = PinMsk(SDCARD_CS);
}

/* ------------------ byte-level sdcard interface routines ------------------ */

static uint8_t transceive_spi(uint8_t c)
{
  USARTC1.DATA = c;
  while (!(USARTC1.STATUS & USART_TXCIF_bm))
    ;
  USARTC1.STATUS |= USART_TXCIF_bm;

  while (!(USARTC1.STATUS & USART_RXCIF_bm))
    ;
  USARTC1.STATUS |= USART_RXCIF_bm;

  return USARTC1.DATA;
}

uint8_t read_byte_spi(void)
{
  // Transmit dummy byte, then read data register
  return transceive_spi(0xFF);
}

void write_byte_spi(uint8_t c)
{
  // Transmit desired byte, then do a dummy read on data register
  transceive_spi(c);
}
