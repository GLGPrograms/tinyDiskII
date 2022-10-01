/**
 * @file clock.hpp
 * @author giuliof (giulio@glgprograms.it)
 * @brief Core clock configurations for AVR XMEGA
 * @version 0.1
 * @date 2022-07-21
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef CLOCK_H_
#define CLOCK_H_

// Initialize AVR clock system. Call this before main loop.
void clk_init();

#endif // CLOCK_H_