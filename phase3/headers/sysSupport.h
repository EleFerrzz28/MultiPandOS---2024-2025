#ifndef SYS_SUPPORT_H_INCLUDED
#define SYS_SUPPORT_H_INCLUDED

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <stddef.h>

void support_exception_handler();
void SYSCALL_EXCEPTION_HANDLER(state_t *saved_exception_state);
void SYSCALL_EXCEPTION_HANDLER(state_t *saved_exception_state);

#endif