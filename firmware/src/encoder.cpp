/* * * * * * * * * * * * * * * * *  INCLUDES  * * * * * * * * * * * * * * * * */

#include "encoder.h"

#include <avr/io.h>

#include "util/port.h"
#include "debug.h"

/* * * * * * * * * * * * * PRIVATE MACROS AND DEFINES * * * * * * * * * * * * */

#define ENC_A B, 1
#define ENC_B B, 2
#define BTN_PIN B, 3

/* * * * * * * * * * * * * * *  STATIC VARIABLES  * * * * * * * * * * * * * * */

static uint8_t old_encoder_pos = 0;
static bool old_btn_status;

/* * * * * * * * * * * * * * *  GLOBAL FUNCTIONS  * * * * * * * * * * * * * * */

void encoder_init()
{
  // Configure encoder pushbutton as input with pull-up
  Port(BTN_PIN).DIRCLR |= PinMsk(BTN_PIN);
  PinCtrl(BTN_PIN) = PORT_OPC_PULLUP_gc;
  old_btn_status = Port(BTN_PIN).IN & PinMsk(BTN_PIN);

  // Configure encoder phases pin
  Port(ENC_A).DIRCLR |= (PinMsk(ENC_A) | PinMsk(ENC_B));
  // Set ENC_A and ENC_B as low level sense
  PinCtrl(ENC_A) = PORT_ISC_LEVEL_gc | PORT_OPC_PULLUP_gc;
  PinCtrl(ENC_B) = PORT_ISC_LEVEL_gc | PORT_OPC_PULLUP_gc;
  // Set ENC_A as mux input for event channel 0
  // TODO pin port and number is hardcoded
  EVSYS.CH0MUX = EVSYS_CHMUX_PORTB_PIN1_gc;
  // Enable quadrature decoding and digital filter
  EVSYS.CH0CTRL = EVSYS_QDEN_bm | EVSYS_DIGFILT_8SAMPLES_gc;
  // Set quadrature decoder as action for TCC (timer/counter C), source from Event ch0
  TCC0.CTRLD = TC_EVACT_QDEC_gc | TC_EVSEL_CH0_gc;
  // Set period and counter register (use counter as 8 bit)
  TCC0.PER = 0xFF;
  TCC0.CNT = 0x00;
  // Enable TCC without prescaling
  TCC0.CTRLA = TC_CLKSEL_DIV1_gc;
}

bool encoder_clicked()
{
  bool btn_status;
  bool ret;

  btn_status = Port(BTN_PIN).IN & PinMsk(BTN_PIN);

  ret = btn_status && !old_btn_status;

  old_btn_status = btn_status;

  return ret;
}

int8_t encoder_update()
{
  uint8_t pos = TCC0.CNT;

  // One encoder tick adds 2 counter steps
  int8_t delta = (pos - old_encoder_pos) >> 1;

  // Update last position only if a tick is detected
  if (delta != 0)
    old_encoder_pos = pos;

  return delta;
}
