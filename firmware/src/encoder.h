/**
 * @file encoder.h
 * @author giuliof (giulio@glgprograms.it)
 * @brief Collection of method to handle rotary encoders.
 * @version 0.1
 * @date 2022-09-22
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef ENCODER_H_
#define ENCODER_H_

#include <stdbool.h>
#include <stdint.h>

// Initialize encoder interface. Call this before main loop.
void encoder_init();
// Returns true if encoder was clicked.
bool encoder_clicked();
// Returns > 0 if encoded was turned CW, < 0 if CCW, else = 0.
int8_t encoder_update();

#endif // ENCODER_H_