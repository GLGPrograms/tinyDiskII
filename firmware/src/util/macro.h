#ifndef MACRO_H_
#define MACRO_H_

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define MSB16(x) ((x) >> 8)
#define LSB16(x) ((x) & 0xFF)

#endif