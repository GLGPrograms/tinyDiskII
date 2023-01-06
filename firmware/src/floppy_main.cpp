/* * * * * * * * * * * * * * * * *  INCLUDES  * * * * * * * * * * * * * * * * */

#include "floppy_main.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include <util/delay.h>

#include "sdcard.h"
#include "sdcard_nic.h"
#include "util/macro.h"
#include "util/port.h"

#include "debug.h"

/* * * * * * * * * * * * * PRIVATE MACROS AND DEFINES * * * * * * * * * * * * */

// Writing buffer size, full sector plus 2 byte CRC
#define WRITING_BUFFER_SIZE (512 + 2 + 1)
// Size in byte of valid data in .NIC file
#define NIC_SIZE (402)
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

enum write_status_t
{
  SYNC_D5,
  SYNC_AA,
  ADDR_FIELD,
  DATA_FIELD
};

/* * * * * * * * * * * * * STATIC FUNCTION PROTOTYPES * * * * * * * * * * * * */

// Reading routines
static void irq_reading();

// Writing routines
static uint8_t floppy_read_data();
static void init_writing();
static void end_writing();
static void check_data();
static void write_pinchange();
static void write_idle();
static void write_back();

/* * * * * * * * * * * * * * *  STATIC VARIABLES  * * * * * * * * * * * * * * */

// DISK II status
// Physical track (0 - 69)
static uint8_t ph_track;
// Disk sector (0 - 15)
static uint8_t sector;

// DISK II read variables
// Number of read bytes from NIC file sector (maximum of 512 bytes)
static uint16_t byte_cnt;
// Masks current bit
static uint8_t byte_mask;
// Update sector request
static volatile bool prepare = true;

// DISK II write variables
// FIXME temporary not static for tests
// SD card sector buffer for writing operations
uint8_t write_buffer[WRITING_BUFFER_SIZE];
// Formatting operation has been detected
static bool formatting = false;
// Writing state machine has detected 0xD5 leading pattern
static bool synced = false;
// Status of the writing state machine
static write_status_t write_status;
// Internal shift register for readed bits
static uint8_t write_data = 0x00;
// Number of non-validated bits shifted inside write_data
static uint8_t write_bitcount = 0;

/* * * * * * * * * * * * * * *  STATIC FUNCTIONS  * * * * * * * * * * * * * * */

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

// This routine is called by timer IRQ every 4us
static void irq_reading()
{
  uint8_t readpulse;

  // disable match (default = output 0)
  TCD0.CCB = 0xFFFF;

  // Still fetching data
  // While in formatting status, always output 0
  if (prepare || formatting)
    return;

  if (byte_cnt < NIC_SIZE)
  {
    // Fetch byte from sdcard sector cache
    readpulse = nic_get_byte(byte_cnt);

    // 3us low, then 1us high pulse
    if (readpulse & byte_mask)
      TCD0.CCB = 3e-6 * (F_CPU);

    // Memento: data must be sent MSb first to Apple][
    byte_mask >>= 1;
    if (byte_mask == 0)
    {
      byte_mask = 0x80;
      byte_cnt++;
    }

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
  write_status = SYNC_D5;
  floppy_read_data();
  TCD0.CNT = 0x0000;
  TCD0.INTFLAGS = TC0_OVFIF_bm;
}

// this function should be called when write request is de-asserted
static void end_writing()
{
  // Write data only if a data field has been populated, with or without its address field
  if (write_status == DATA_FIELD)
    write_back();
  TCD0.CNT = 0x0000;
  TCD0.INTFLAGS = TC0_OVFIF_bm;
  sei();
}

static void check_data()
{
  static uint8_t* write_ptr;
  static uint16_t write_len = 0;

  if (!synced)
  {
    // Wait for sync byte
    if (write_data != 0xD5)
      return;

    synced = true;
    // TODO in write init, remember to initialize:
    // write_bitcount = 0
    // write_status = SYNC_D5
  }
  else
    write_bitcount--;

  if (write_bitcount == 0)
  {
    write_bitcount = 8;
    switch (write_status)
    {
      case SYNC_D5:
        if (write_data == 0xAA)
          write_status = SYNC_AA;
        // TODO else raise an error (debug)
        break;

      case SYNC_AA:
        // Address field
        if (write_data == 0x96)
        {
          // initialize write pointer after 0xD5 0xAA 0x96
          // Address field is 14 bytes long, header must be skipped
          write_ptr = write_buffer + 0x25;
          write_len = 14 - 3;
          write_status = ADDR_FIELD;
          formatting = true;
        }
        // Data field
        else if (write_data == 0xAD)
        {
          // initialize write pointer after 0xD5 0xAA 0xAD
          // Data field is 349 bytes long, header must be skipped
          write_ptr = write_buffer + 0x38;
          write_len = 349 - 3;
          write_status = DATA_FIELD;
        }
        // TODO else raise an error (debug)
        break;

      // Write the actual data
      case ADDR_FIELD:
        if (write_len)
        {
          *(write_ptr++) = write_data;
          write_len--;
        }
        else
        {
          synced = false;
          write_status = SYNC_D5;
          write_bitcount = 0;
        }
        break;

      case DATA_FIELD:
        if (write_len)
        {
          *(write_ptr++) = write_data;
          write_len--;
        }
        else
        {
          write_back();
          synced = false;
          write_status = SYNC_D5;
          write_bitcount = 0;
        }
        break;
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
static void write_back()
{
  uint8_t wr_track = ph_track >> 1;
  uint8_t wr_sector = sector;

  // Formatting: ignore physical track and current sector, fetch them
  // from address field inside writing buffer.
  if (formatting)
  {
    wr_track = ((write_buffer[0x27] & 0x55) << 1) | (write_buffer[0x28] & 0x55);
    wr_sector = ((write_buffer[0x29] & 0x55) << 1) | (write_buffer[0x2A] & 0x55);
    // Globally update sector position for reading
    sector = wr_sector;
    // debug_printP(PSTR("f%d\n\r"), sector);
    formatting = false;
  }
  // Only data field was written, manually populate address field
  // inside buffer with physical track and current sector
  else
  {
    uint8_t crc = volume ^ wr_track ^ wr_sector;
    write_buffer[0x25] = (volume >> 1) | 0xAA;
    write_buffer[0x26] = volume | 0xAA;
    write_buffer[0x27] = (wr_track >> 1) | 0xAA;
    write_buffer[0x28] = wr_track | 0xAA;
    write_buffer[0x29] = (wr_sector >> 1) | 0xAA;
    write_buffer[0x2A] = wr_sector | 0xAA;
    write_buffer[0x2B] = (crc >> 1) | 0xAA;
    write_buffer[0x2C] = crc | 0xAA;
  }

  // Actually write the sector inside SD Card
  nic_write_sector(write_buffer, wr_track, wr_sector);
}

/* * * * * * * * * * * * * * *  GLOBAL FUNCTIONS  * * * * * * * * * * * * * * */

void floppy_init()
{
  // Input stepper
  Port(FLOPPY_PH).DIRCLR = FLOPPY_PH_MASK;

  byte_cnt = 0;
  byte_mask = 0x80;

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

  // Prepare write buffer content
  nic_prepare_wrbuf(write_buffer);
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

static void floppy_update_sector()
{
  // mute interrupts during update. 0 will be sent
  cli();

  // Move to the next sector circularly
  // Sectors does not have a particular alignemnt between track and track.
  // Then, sector is always incremented
  sector = ((sector + 1) & 0xf);
  nic_update_sector(ph_track >> 1, sector);
  byte_cnt = 0;
  byte_mask = 0x80;
  prepare = false;

  sei();
}

static void floppy_handle_writing()
{
  init_writing();

  // TODO explain these lines
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

  // Wait here until DMA has finished.
  // While waiting, writing may be reasserted, for this reason
  // floppy_handle_writing() may be called immediately after return.
  while(!sdcard_nic_writeback_ended())
    ;

  // After writing, always update sector cache from SD card
  prepare = true;
}

/* * * Update physical track position * * */
static void floppy_update_stepper()
{
  // Current stepper offset (0 to 3)
  static uint8_t ofs = 0;
  // Latest stepper offset
  static uint8_t old_ofs = 0;

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
}

void floppy_main()
{
  cli();
  formatting = false;
  // Leave track at last position...
  // ph_track = 0;
  sector = 0;
  byte_cnt = 0;
  byte_mask = 0x80;
  prepare = true;
  sei();

  while (floppy_drive_enabled())
  {
    // Handle writing to floppy operations.
    // Writing routine is blocking, no read operation nor stepper update is done during write
    // This assumption is quite reasonable, who would change track while writing?
    while (WRITE_CAPABLE && floppy_write_enable())
      floppy_handle_writing();

    floppy_update_stepper();

    // Read new sector from SD card
    // Avoid SD card access during formatting operations, it would mess up timings
    if (prepare && !formatting)
      floppy_update_sector();
  }

  debug_printP(PSTR("Reading done\n\r"));

  // TODO wait till last DMA operation

  // CS can be restored to 1
  sdcard_set_mode(FAST, 1);

  // Always invalidate sector reading cache after each floppy operation
  sdcard_cache_invalidate();
}
