#pragma once

#include "utils.h"

// /* Macros for port management */
#define xCAT2(a, b) a##b
#define CAT2(a, b) xCAT2(a, b)

#define xCAT3(a, b, c) a##b##c
#define CAT3(a, b, c) xCAT3(a, b, c)

#define CAT_PORT(a, b, c) xCAT2(a, b)
#define Port(a) (CAT_PORT(PORT, a))

#define CAT_PIN(a, b, c) xCAT2(a, b)
#define Pin(a) (CAT_PIN(PIN, a))

#define CAT_DDR(a, b, c) xCAT2(a, b)
#define Ddr(a) (CAT_DDR(DDR, a))

#define CAT_PINNO(a, b) b
#define PinNo(a) (CAT_PINNO(a))
#define PinMsk(a) (1 << (CAT_PINNO(a)))
#define PinCtrl(x) (CAT_PORT(PORT, x).CAT3(PIN, CAT_PINNO(x), CTRL))

#define Uart(my_uart) CAT2(USART, CAT2(my_uart))

#define ISR_RXC(my_uart) CAT3(USART, CAT2(my_uart), _RXC_vect)

#define RXD0 2
#define TXD0 3
#define RXD1 6
#define TXD1 7

#define GET_RX_PIN(x) CAT2(RXD, x)
#define GET_TX_PIN(x) CAT2(TXD, x)
#define PinCtrlRX(x) CAT3(PIN, GET_RX_PIN(CAT_PINNO(x)), CTRL)
#define PinCtrlTX(x) CAT3(PIN, GET_TX_PIN(CAT_PINNO(x)), CTRL)
#define PinRX(x) GET_RX_PIN(CAT_PINNO(x))
#define PinTX(x) GET_TX_PIN(CAT_PINNO(x))

#define Twi(x) CAT2(TWI, x)
#define TwiISR(x) CAT3(TWI, x, _TWIM_vect)