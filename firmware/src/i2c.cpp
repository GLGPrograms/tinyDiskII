/**
 * @file i2c.c
 * @author giuliof (giulio@glgprograms.it)
 * @brief A simple I2C interface for XMEGA
 * Based on Michael Köhler's library for atmega
 * @version 0.1
 * @date 2022-07-21
 *
 * @copyright Copyright (c) 2022
 *
 */

/* * * * * * * * * * * * * * * * *  INCLUDES  * * * * * * * * * * * * * * * * */

#include "i2c.h"

#include <avr/io.h>

#include "i2c.h"
#include "util/port.h"

/* * * * * * * * * * * * * PRIVATE MACROS AND DEFINES * * * * * * * * * * * * */

// Port for I2C interface
#define TWI_PORT E
// I2C clock in Hz
#define F_I2C 100000UL
// Baudrate calculation
#define BAUDSETTING ((F_CPU / (2 * F_I2C)) - 5)

/* * * * * * * * * * * * * * *  GLOBAL FUNCTIONS  * * * * * * * * * * * * * * */

void i2c_init(void)
{
  // Enable pullup
  // FIXME make ad-hoc macros
  PORTE.PIN0CTRL = PORT_OPC_WIREDANDPULL_gc;
  PORTE.PIN1CTRL = PORT_OPC_WIREDANDPULL_gc;

  Twi(TWI_PORT).MASTER.BAUD = BAUDSETTING;

  // SDA hold time off, normal TWI operation
  Twi(TWI_PORT).CTRL = 0x00;

  Twi(TWI_PORT).MASTER.CTRLA |= TWI_MASTER_ENABLE_bm;

  // No timeout for idle bus
  Twi(TWI_PORT).MASTER.CTRLB = TWI_MASTER_TIMEOUT_DISABLED_gc;

  // initially send ACK and no CMD selected
  Twi(TWI_PORT).MASTER.CTRLC = 0x00;

  // clear all flags initially and select bus state IDLE
  Twi(TWI_PORT).MASTER.STATUS |=
      TWI_MASTER_RIF_bm | TWI_MASTER_WIF_bm | TWI_MASTER_ARBLOST_bm |
      TWI_MASTER_BUSERR_bm |
      TWI_MASTER_BUSSTATE_IDLE_gc;
}

// Start TWI/I2C interface
bool i2c_start(uint8_t i2c_addr)
{
  Twi(TWI_PORT).MASTER.ADDR = i2c_addr;

  while (!(Twi(TWI_PORT).MASTER.STATUS & (TWI_MASTER_WIF_bm | TWI_MASTER_RIF_bm)))
    ;

  // if NACK received from slave (RXACK is 0 when ACK received)
  if (Twi(TWI_PORT).MASTER.STATUS & TWI_MASTER_RXACK_bm)
  {
    Twi(TWI_PORT).MASTER.STATUS |= TWI_MASTER_WIF_bm;
    Twi(TWI_PORT).MASTER.STATUS |= TWI_MASTER_RIF_bm;
    Twi(TWI_PORT).MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
    i2c_stop();
    return false;
  }
  return true;
}

// Stop TWI/I2C interface
void i2c_stop(void)
{
  Twi(TWI_PORT).MASTER.CTRLC = TWI_MASTER_ACKACT_bm | TWI_MASTER_CMD_STOP_gc;
}

// Send byte at TWI/I2C interface
void i2c_byte(uint8_t byte)
{
  // Write data and move buffer pointer
  Twi(TWI_PORT).MASTER.DATA = byte;

  // Wait for transmission completed and check for errors (even nack)
  while (!(Twi(TWI_PORT).MASTER.STATUS & TWI_MASTER_WIF_bm))
    ;

  // If NACK received...
  if (Twi(TWI_PORT).MASTER.STATUS & TWI_MASTER_RXACK_bm)
  {
    // Release bus
    Twi(TWI_PORT).MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
  }
}