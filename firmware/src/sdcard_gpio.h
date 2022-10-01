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
void sdcard_do(uint8_t i);
void sdcard_cs(uint8_t i);
void sdcard_clk_toggle();
void sdcard_clk(uint8_t i);
uint8_t sdcard_di();

uint8_t read_byte_spi(void);
void write_byte_spi(uint8_t c);
uint8_t read_byte_slow(void);
void write_byte_slow(uint8_t c);
uint8_t read_byte_fast(void);
void write_byte_fast(uint8_t c);

#endif // SRC_SDCARD_GPIO_H_