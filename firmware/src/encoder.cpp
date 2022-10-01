#include "encoder.h"

#include <avr/io.h>

#include "util/port.h"

/* * * * * * * * * * * * * * * * PUBLIC MEMBERS * * * * * * * * * * * * * * * */

#define ENC_A B, 2
#define ENC_B B, 1
#define BTN_PIN B, 3

static uint8_t old_encoder_status;
static bool old_btn_status;

/* * * * * * * * * * * * * * SHARED IMPLEMENTATIONS * * * * * * * * * * * * * */

void encoder_init()
{
  Port(ENC_A).DIRCLR |= (PinMsk(ENC_A) | PinMsk(ENC_B));
  Port(BTN_PIN).DIRCLR |= PinMsk(BTN_PIN);
  PinCtrl(ENC_A) = PORT_OPC_PULLUP_gc;
  PinCtrl(ENC_B) = PORT_OPC_PULLUP_gc;
  PinCtrl(BTN_PIN) = PORT_OPC_PULLUP_gc;

  old_encoder_status = Port(ENC_A).IN & (PinMsk(ENC_A) | PinMsk(ENC_B));
  old_btn_status = Port(BTN_PIN).IN & PinMsk(BTN_PIN);
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
  int8_t inc = 0;
  uint8_t status = Port(ENC_A).IN & (_BV(PinNo(ENC_A)) | _BV(PinNo(ENC_B)));
  if (((status ^ old_encoder_status) == _BV(PinNo(ENC_B))) && !(status & _BV(PinNo(ENC_B))))
  {
    if (status & _BV(PinNo(ENC_A)))
      inc++;
    else
      inc--;
  }

  old_encoder_status = status;

  return inc;
}
