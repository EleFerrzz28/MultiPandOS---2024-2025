#ifndef VM_SUPPORT_H_INCLUDED
#define VM_SUPPORT_H_INCLUDED

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <stddef.h>
#include <stdint.h>
#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase2/headers/exceptions.h"

/*Swap Mutex Process*/
extern unsigned int SWAP_POOL_SEMAPHORE; // when accessing the swap pool -> MUTEX

/*Page Table*/
extern pteEntry_t PAGE_TABLE[32];

/*Swap Pool Structure*/
extern swap_t SWAP_POOL[POOLSIZE];

/*Master Semaphore*/
extern unsigned int MASTER_SEMAPHORE;

/*Array of semaphores for the u-proc's devices*/
extern volatile unsigned int U_DEVICE_SEMAPHORES[3*UPROCMAX];

#endif