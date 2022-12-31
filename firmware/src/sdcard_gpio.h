/**
 * @file sdcard_gpio.h
 * @author giuliof (giulio@glgprograms.it)
 * @brief AVR platform dependent code for SPI SD card interface.
 * @version 0.1
 * @date 2022-08-28
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef SDCARD_GPIO_H_
#define SDCARD_GPIO_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void sdcard_gpio_init();
void sdcard_gpio_bitbang_init();
void sdcard_gpio_spi_init();

bool sdcard_present();
bool sdcard_locked();
void sdcard_cs(uint8_t i);

uint8_t read_byte_spi(void);
void write_byte_spi(uint8_t c);

#endif // SRC_SDCARD_GPIO_H_