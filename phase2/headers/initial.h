#ifndef INITIAL_H_INCLUDED
#define INITIAL_H_INCLUDED

#include <uriscv/liburiscv.h>

#include "../../headers/listx.h"
#include "../../headers/types.h"
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"
#include "exceptions.h"
#include "scheduler.h"

/*Declare the Level 3 global variables*/

/*Process Count*/
extern int PROC_COUNT;
/*Ready Queue*/
extern struct list_head READY_Q;
/*Current Process*/
extern pcb_t *CURRENT_PROC[8];
/*Global Lock: values belong to: [0,1]*/
extern volatile unsigned int *GLOBAL_LOCK;
/*Device Semaphores*/
extern volatile unsigned int DEVICE_SEMAPHORES[SEMDEVLEN];

extern void uTLB_RefillHandler();
extern void test();

void initFirstProcess();
void configureIRTEntries(__UINT32_TYPE__ *irtBase);

#endif