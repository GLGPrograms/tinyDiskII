/**
 * @file i2c.h
 * @author giuliof (giulio@glgprograms.it)
 * @brief A simple I2C interface for XMEGA
 * Based on Michael Köhler's library for atmega
 * @version 0.1
 * @date 2022-07-21
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef I2C_H_
#define I2C_H_

#include <stdint.h>
#include <stdbool.h>

// Initialize I2C peripheral.
void i2c_init();
// Send start byte to address addr. If issued twice without stop in between,
// issues a repeated start. Addr is in 8 bit format with R/W bit.
// Returns true if peripheral ACKs.
bool i2c_start(uint8_t addr);
// Send data byte over I2C.
void i2c_byte(uint8_t data);
// Send stop bit and close communication.
void i2c_stop();

#endif // SRC_I2C_H_