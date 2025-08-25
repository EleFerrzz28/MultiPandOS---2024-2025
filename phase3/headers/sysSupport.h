#ifndef SYS_SUPPORT_H_INCLUDED
#define SYS_SUPPORT_H_INCLUDED

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include "vmSupport.h"

void support_exception_handler();
void PROGRAM_TRAP_EXCEPTION_HANDLER_SUPPORT();
void SYSCALL_EXCEPTION_HANDLER_SUPPORT(support_t *process_support_struct);

#endif