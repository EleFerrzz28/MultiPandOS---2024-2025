#ifndef SYS_SUPPORT_H_INCLUDED
#define SYS_SUPPORT_H_INCLUDED

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <stddef.h>
#include "vmSupport.h"

void support_exception_handler();
void PROGRAM_TRAP_EXCEPTION_HANDLER_SUPPORT();
void SYSCALL_EXCEPTION_HANDLER_SUPPORT(state_t *saved_exception_state);

#endif