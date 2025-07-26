#include "vmSupport.h"

/*Declaring the structures - module wide*/
unsigned int SWAP_POOL_SEMAPHORE;
unsigned int MASTER_SEMAPHORE;
pteEntry_t PAGE_TABLE[32];
swap_t SWAP_POOL[POOLSIZE];
volatile unsigned int U_DEVICE_SEMAPHORES[3*UPROCMAX];
