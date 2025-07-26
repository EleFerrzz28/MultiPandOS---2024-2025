#ifndef EXCEPTION_H_INCLUDED
#define EXCEPTION_H_INCLUDED

#include "initial.h"
#include "interrupts.h"
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <stddef.h>


void *memcpy (void *dest, const void *src, unsigned int len);
void KILL_PROCESS(pcb_t* process);
int getSemaphoreDeviceIndex(memaddr* commandAddr);
void PASS_UP_OR_DIE(int except_type, state_t* previous_state);
void CREATE_PROCESS (state_t* saved_exception_state);
void TERMINATE_PROCESS(state_t* saved_exception_state);
void SEM_PASSEREN(state_t* saved_exception_state);
void SEM_VERHOGEN(state_t* saved_exception_state);
void DO_IO (state_t* saved_exception_state);
void GetCPUTime (state_t* saved_exception_state);
void WaitForClock(state_t* saved_exception_state);
void GetSupportData(state_t* saved_exception_state);
void GetProcessID(state_t* saved_exception_state);
void PROGRAM_TRAP_EXCEPTION_HANDLER(state_t* saved_exception_state);
void SYSCALL_EXCEPTION_HANDLER(state_t* saved_exception_state);
void exceptionHandler();


#endif