/**
 * @file utils.h
 * @author Giulio (giulio@glgprograms.it)
 * @brief (almost) library agnostic utility collection (mostly macros)
 * @version 0.1
 * @date 2020-12-29
 *
 * @copyright Copyright (c) 2020
 *
 */

#pragma once

#define CAST(type, p_name, src) type* p_name = (type*)src

// Byte manipulation

#define MASK(bits) ((1 << (bits)) - 1)
#define low(x) ((x)&0xFF)
#define high(x) ((x >> 8) & 0xFF)

#define hi8(x) (((x) >> 4) & 0xf)
#define lo8(x) ((x) & 0xf)

// Concatenation
#define xCAT2(a, b) a##b
#define CAT2(a, b) xCAT2(a, b)

#define xCAT3(a, b, c) a##b##c
#define CAT3(a, b, c) xCAT3(a, b, c)

// Stringification
#define xstr(s) str(s)
#define str(s) #s