#include "floppy_main.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include <util/delay.h>

#include "sdcard.h"
#include "sdcard_nic.h"
#include "util/macro.h"
#include "util/port.h"

#include "debug.h"

/* * * * * * * * * * * * * * * * PUBLIC MEMBERS * * * * * * * * * * * * * * * */

// Writing buffer size
#define WRITING_BUFFER_SIZE (350)
// Volume number is always the same
#define volume (0xFE)

// IO defines
#define FLOPPY_DISK_WRITE      D, 0
#define FLOPPY_PH              A, 0
#define FLOPPY_PH_MASK         0xF
#define FLOPPY_DISK_WRITE_EN   A, 4
#define FLOPPY_DISK_EN         A, 5
#define FLOPPY_DISK_READ_EX    A, 6
#define FLOPPY_DISK_READ       D, 1
#define FLOPPY_DISK_WRITE_PROT A, 7

#define WRITE_CAPABLE (true)

// DISK II status
// Formatting operation has been detected
static bool formatting = false;
// Physical track
static uint8_t ph_track; // 0 - 69
// Disk sector
static uint8_t sector; // 0 - 15
// Number of read bits from NIC file sector (maximum of 512 bytes)
static uint16_t bitbyte; // 0 - (8*512-1)
static uint8_t write_buffer[WRITING_BUFFER_SIZE];
static uint8_t write_data = 0x00;
static uint8_t write_bitcount = 0;
static bool synced = false;
// Update sector request
static volatile bool prepare = true;

// Reading routines
static void abort_reading();
static void irq_reading();

// Writing routines
static uint8_t floppy_read_data();
static void init_writing();
static void end_writing();
static void check_data();
static void write_pinchange();
static void write_idle();
static void write_back(void);

/* * * * * * * * * * * * * * PUBLIC IMPLEMENTATIONS * * * * * * * * * * * * * */

/* - - - - - - - - - - - - - - Low-level methods  - - - - - - - - - - - - - - */

static uint8_t floppy_write_in()
{
  return (Port(FLOPPY_DISK_WRITE).IN & PinMsk(FLOPPY_DISK_WRITE)) ? 1 : 0;
}

static uint8_t floppy_write_enable()
{
  return (Port(FLOPPY_DISK_WRITE_EN).IN & PinMsk(FLOPPY_DISK_WRITE_EN)) ? 0 : 1;
}

/* - - - - - - - - - - - - - floppy reading methods - - - - - - - - - - - - - */

static void abort_reading()
{
  if (bitbyte < (402 * 8))
  {
    nic_abort_read(bitbyte);
    bitbyte = 402 * 8;
  }
}

static void irq_reading()
{
  uint8_t readpulse;

  // disable match
  TCD0.CCB = 0xFFFF;

  // Still fetching data
  if (prepare)
    return;

  if (bitbyte < (402 * 8))
  {
    // Fetch bit from sdcard
    readpulse = nic_get_bit();

    // 3us low, then 1us high pulse
    if (readpulse)
      TCD0.CCB = 3e-6 * (F_CPU);

    bitbyte++;

    return;
  }

  prepare = true;
}

/* - - - - - - - - - - - - - floppy writing methods - - - - - - - - - - - - - */

static uint8_t floppy_read_data()
{
  static uint8_t magstate = 0;
  uint8_t old_magstate = magstate;
  magstate = floppy_write_in();
  return magstate ^ old_magstate;
}

// this function should be called when a write request is asserted
static void init_writing()
{
  cli();
  // Disable match
  TCD0.CCB = 0xFFFF;
  synced = false;
  write_bitcount = 0;
  floppy_read_data();
  TCD0.CNT = 0x0000;
  TCD0.INTFLAGS = TC0_OVFIF_bm;
}

// this function should be called when write request is de-asserted
static void end_writing()
{
  write_back();
  TCD0.CNT = 0x0000;
  TCD0.INTFLAGS = TC0_OVFIF_bm;
  sei();
}

static void check_data()
{
  static uint16_t bufsize;
  if (!synced && write_data == 0xD5)
  {
    synced = true;
    bufsize = 0;
    goto push;
  }

  if (synced && bufsize < WRITING_BUFFER_SIZE)
  {
    write_bitcount++;
    if (write_bitcount == 8)
    {
    push:
      write_bitcount = 0;
      write_buffer[bufsize++] = write_data;
    }
  }
}

static void write_pinchange()
{
  // disable match
  // TCD0.CCB = 0xFFFF;
  write_data <<= 1;
  write_data |= 1;
  TCD0.CNT = 0x0000;
  TCD0.INTFLAGS = TC0_OVFIF_bm;
  check_data();
}

static void write_idle()
{
  // disable match
  // TCD0.CCB = 0xFFFF;
  write_data <<= 1;
  TCD0.INTFLAGS = TC0_OVFIF_bm;
  check_data();
}

// write back writeData into the SD card
static void write_back(void)
{
  static unsigned char sec;

  // if (bit_is_set(PIND,3)) return;
  if (write_buffer[2] == 0xAD)
  {
    if (!formatting)
    {
      abort_reading();
      nic_write_sector(write_buffer, volume, ph_track >> 1, sector);
      write_buffer[2] = 0;
      prepare = true;
    }
    else
    {
      sector = sec;
      formatting = false;
      if (sec == 0xf)
      {
        // cancel reading
        abort_reading();
        prepare = true;
      }
    }
  }
  if (write_buffer[2] == 0x96)
  {
    sec = (((write_buffer[7] & 0x55) << 1) | (write_buffer[8] & 0x55));
    formatting = true;
  }
}

/* * * * * * * * * * * * * * SHARED IMPLEMENTATIONS * * * * * * * * * * * * * */

void floppy_init()
{
  // Input stepper
  Port(FLOPPY_PH).DIRCLR = FLOPPY_PH_MASK;

  bitbyte = 0;

  /*
   *
   *      /|     /|     /|     /|
   *     / |    / |    / |    / |
   *    /  |   /  |   /  |   /  |
   *   /   |  /   |  /   |  /   |
   *  /    | /    | /    | /    |
   * /     |/     |/     |/     |
   *      _      _             _
   * ____| |____| |___________| |
   *      1      1      0      1
   */

  // Timer. Set period to 4us,
  TCD0.PER = 4e-6 * (F_CPU);
  // PWM mode, output compare enabled on CCB
  TCD0.CTRLB = (TC_WGMODE_SINGLESLOPE_gc << TC0_WGMODE0_bp) | TC0_CCBEN_bm;
  // interrupt on overflow,
  TCD0.INTCTRLA = (TC_OVFINTLVL_HI_gc) << TC0_OVFIF_bp;

  // Inputs
  Port(FLOPPY_DISK_WRITE).DIRCLR = PinMsk(FLOPPY_DISK_WRITE);
  Port(FLOPPY_DISK_WRITE_EN).DIRCLR = PinMsk(FLOPPY_DISK_WRITE_EN);
  Port(FLOPPY_DISK_EN).DIRCLR = PinMsk(FLOPPY_DISK_EN);
  Port(FLOPPY_DISK_READ_EX).DIRCLR = PinMsk(FLOPPY_DISK_READ_EX);
  // Outputs
  Port(FLOPPY_DISK_READ).DIRSET = PinMsk(FLOPPY_DISK_READ);
  Port(FLOPPY_DISK_WRITE_PROT).DIRSET = PinMsk(FLOPPY_DISK_WRITE_PROT);

  // output compare enabled (inverted)
  Port(FLOPPY_DISK_READ).PIN1CTRL = PORT_INVEN_bm;
  // output compare register to MAX?
  TCD0.CCB = 0xFFFF;

  // Enable clock to the TCD0
  TCD0.CTRLA = TC_CLKSEL_DIV1_gc;
}

/* - - - - - - - - - - - - - - Low-level methods  - - - - - - - - - - - - - - */

void floppy_write_protect(bool protect)
{
  if (protect)
    Port(FLOPPY_DISK_WRITE_PROT).OUTSET = PinMsk(FLOPPY_DISK_WRITE_PROT);
  else
    Port(FLOPPY_DISK_WRITE_PROT).OUTCLR = PinMsk(FLOPPY_DISK_WRITE_PROT);
}

bool floppy_drive_enabled()
{
  if (Port(FLOPPY_DISK_EN).IN & PinMsk(FLOPPY_DISK_EN))
    return false;
  return true;
}

// Timer IRQ: called every 4us to output data signal (see comment in floppy_init)
ISR(TCD0_OVF_vect)
{
  irq_reading();
}

/* - - - - - - - - - - - - - - High-level methods - - - - - - - - - - - - - - */

void floppy_main()
{
  // Current stepper offset (0 to 3)
  static uint8_t ofs = 0;
  // Latest stepper offset
  static uint8_t old_ofs = 0;

  // Prepare variables
  cli();
  bitbyte = 0;
  prepare = true;
  // Leave track at last position...
  // ph_track = 0;
  sector = 0;
  sei();

  while (floppy_drive_enabled())
  {
    // Prepare for writing?
    if (WRITE_CAPABLE && floppy_write_enable())
    {
      init_writing();

      uint8_t magstate = floppy_write_in();

      do
      {
        uint8_t new_magstate = floppy_write_in();
        if (magstate != new_magstate)
        {
          write_pinchange();
          magstate = new_magstate;
        }
        if (TCD0.INTFLAGS & TC0_OVFIF_bm)
        {
          write_idle();
        }
      } while (floppy_write_enable());

      end_writing();
    }


    /* * * Update physical track position * * */
    uint8_t stp_pos = (Port(FLOPPY_PH).IN & FLOPPY_PH_MASK);

    ofs = ((stp_pos == 0b00001000)
               ? 3
               : ((stp_pos == 0b00000100)
                      ? 2
                      : ((stp_pos == 0b00000010) ? 1
                                                 : ((stp_pos == 0b00000001) ? 0 : ofs))));
    if (ofs != old_ofs)
    {
      if (ofs == ((old_ofs + 1) & 0x3))
        ph_track++;
      else if (ofs == ((old_ofs - 1) & 0x3))
        ph_track--;

      old_ofs = ofs;

      // < 0
      if (ph_track > 70)
        ph_track = 0;
      // > fine tracce
      else if (ph_track > 69)
        ph_track = 69;

      // DEBUG good old printf debug
      // debug_printP(PSTR("%x\n\r"), ph_track);
      // ... do not interrupt a sector reading when changing track
    }

    if (prepare)
    {
      // mute interrupts during update. 0 will be sent
      cli();

      if (bitbyte == (402 * 8))
      {
        // Sector is ended,
        // discard 112 byte (including CRC 2 byte)
        nic_abort_read(bitbyte);
      }

      sdcard_set_mode(SPI, 1);

      // Move to the next sector circularly
      // Sectors does not have a particular alignemnt between track and track.
      // Then, sector is always incremented
      sector = ((sector + 1) & 0xf);
      nic_update_sector(ph_track >> 1, sector);
      bitbyte = 0;
      prepare = false;

      sdcard_set_mode(BITBANG_FAST, 0);

      sei();
    }
  }

  debug_printP(PSTR("Reading done\n\r"));

  // wait till last irq
  while (!prepare)
    ;
  if (bitbyte == (402 * 8))
  {
    // Sector is ended,
    // discard 112 byte (including CRC 2 byte)
    nic_abort_read(bitbyte);
  }

  sdcard_set_mode(SPI, 1);

  bitbyte = 0;
  ph_track = 0;
  ofs = 0;
  old_ofs = 0;
}
