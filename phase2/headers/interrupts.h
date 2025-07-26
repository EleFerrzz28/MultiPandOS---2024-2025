#ifndef INTERRUPTS_H_INCLUDED
#define INTERRUPTS_H_INCLUDED

#include "initial.h"

void local_timer_interrupt_handler(state_t* previous_state);
void pseudoclock_interrupt_handler(state_t* previous_state);
int getIntLineNo (int cause);
void device_interrupt_handler(state_t* previous_state, int cause);
void INTERRUPT_HANDLER(state_t* previous_state, unsigned int exception_code);





#endif