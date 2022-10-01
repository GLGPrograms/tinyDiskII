/**
 * @file debug.h
 * @author giuliof (giulio@glgprograms.it)
 * @brief Debugging defines, helper macros to kick out debugging printf messages
 * @version 0.1
 * @date 2022-06-03
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#ifdef __AVR__
#ifdef _DEBUG
#include <stdio.h>
#include <avr/pgmspace.h>
#define debug_printP(...) printf_P(__VA_ARGS__)
#define debug_print(...) printf(__VA_ARGS__)
#else
#include <avr/pgmspace.h>
#define debug_printP(...)
#define debug_print(...)
#endif
#else
#define PSTR(x)
#define debug_printP(...)
#define debug_print(...)
#endif

#endif // SRC_DEBUG_H_