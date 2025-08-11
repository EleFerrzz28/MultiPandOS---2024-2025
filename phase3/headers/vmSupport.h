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

/* The recommended location in MultiPandOS is to place the Swap Pool after the end of the operating
system code and processors kernel stacks. Though the size of one’s operating system code is unknown2 ,
simply overestimate its size.*/
#define SWAP_POOL_START RAMSTART + (64 * PAGESIZE) + (NCPU * PAGESIZE);

/*Swap Mutex Process*/
extern unsigned int SWAP_POOL_SEMAPHORE; // when accessing the swap pool -> MUTEX

/*Swap Pool Structure*/
extern swap_t SWAP_POOL[POOLSIZE];

/*Master Semaphore*/
extern unsigned int MASTER_SEMAPHORE;

/*Array of semaphores for the u-proc's devices*/
extern volatile unsigned int U_DEVICE_SEMAPHORES[4*UPROCMAX];

/* A pointer to the current holder of mutual exclusion* on the Swap Pool Semaphore */
extern pcb_t *swap_holder;
#endif