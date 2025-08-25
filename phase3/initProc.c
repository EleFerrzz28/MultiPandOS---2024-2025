#include "headers/initProc.h"

/* Function that initializes a U-proc's page table */
void initPageTableEntry(pteEntry_t *entry, int asid, int i) {
  if (i < 31)/*Each U-proc executes in the kuseg address space*/
    entry->pte_entryHI = KUSEG | (i << VPNSHIFT) | (asid << ASIDSHIFT);
  else /*The stack page is limited to one page and is set to the halfway point in kuseg.*/
    entry->pte_entryHI = 0xBFFFF000 | (i << VPNSHIFT) | (asid << ASIDSHIFT);
  entry->pte_entryLO = DIRTYON;
}

/* Function that initializes the Processor State for a U-proc */
void initProcessState(state_t *processState, int index){
    processState->pc_epc = UPROCSTARTADDR; // PC set to 0x8000.00B0; the address of the start of the .text section
    processState->reg_sp = USERSTACKTOP; //  SP set to 0xC000.0000
    // Status set for user-mode with all interrupts and the processor Local Timer enabled.
    processState->mie = MIE_ALL;
    processState->status = MSTATUS_MPIE_MASK | MSTATUS_MPP_U;
    //  EntryHi.ASID set to the process’s unique ID; an integer from [1..8]:
    processState->entry_hi = index << ASIDSHIFT; // moved to where the hw expects to find it
}

/* Function that initializes a Support Structure for a U-proc */
void initSupportStructures(support_t *supportStructure, int index){
    /* Set the process's ASID */
    supportStructure->sup_asid = index;
    /* Set the two PC fields. One of them (0 - PGFAULTEXCEPT) should be set to the address of the
    Support Level’s TLB handler, while the other one (1 - GENERALEXCEPT) should be set to the
    address of the Support Level’s general exception handler.*/
    supportStructure->sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr )pager;
    supportStructure->sup_exceptContext[GENERALEXCEPT].pc = (memaddr )support_exception_handler;
    /* Set the two Status registers to kernel-mode.*/
    supportStructure->sup_exceptContext[PGFAULTEXCEPT].status = MSTATUS_MPP_M;
    supportStructure->sup_exceptContext[GENERALEXCEPT].status = MSTATUS_MPP_M;
    /* Set the two SP fields to utilize the two stack spaces allocated in the Support Structure*/
    supportStructure->sup_exceptContext[PGFAULTEXCEPT].stackPtr = (unsigned int) &(supportStructure->sup_stackTLB[499]);
    supportStructure->sup_exceptContext[GENERALEXCEPT].stackPtr = (unsigned int) &(supportStructure->sup_stackGen[499]);
}

void InstantiatorProcess (){
    // Initialize swap pool table
    for(int i = 0; i < POOLSIZE; i++){
        SWAP_POOL[i].sw_asid = NOPROC;
    }

    // Initialize swap pool table semaphore
    SWAP_POOL_SEMAPHORE = 1;

    // Initialize masterSemaphore
    MASTER_SEMAPHORE = 0;

    for(int i = 0; i < 4*UPROCMAX; i++){
        U_DEVICE_SEMAPHORES[i] = 1;
    }

    /* Initializing U-procs: to launch a U-proc, one simply sets up the parameters for a NSYS1, 
    followed by the actual execution of the NSYS1 Nucleus service. The NSYS1 Nucles service takes
    two parameters: the inital processor state for the U-proc and a pointer to an initalized 
    Support Structure for the U-prc */

    // array of state_t
    state_t UprocState[UPROCMAX];
    // array of support_t 
    support_t supportStructures[UPROCMAX];

    for(int i = 1; i <= UPROCMAX; i++){
        initProcessState(&UprocState[i-1], i);
        initSupportStructures(&supportStructures[i-1], i);
        for(int j = 0; j < USERPGTBLSIZE; j++){
            initPageTableEntry(&supportStructures[i-1].sup_privatePgTbl[j], i, j);
        }
        /*  Initialize and launch (NSYS1) between 1 and 8 U-procs. */
        SYSCALL(CREATEPROCESS, (int)&UprocState[i-1], PROCESS_PRIO_LOW, (int)&supportStructures[i-1]);
    }

    for(int i = 0; i < UPROCMAX; i++){
        /* Introduce a new Support Level-level semaphore; the zero-initialized masterSemaphore. After
        launching all the U-procs, test should repeatedly issue a NSYS3 (P operation) on this semaphore.
        This loop should iterate UPROCMAX times: the number of U-proc’s launched: [1..8].*/
        SYSCALL(PASSEREN, (int)&MASTER_SEMAPHORE, 0, 0);
    }
    /* After this loop concludes, test concludes by issuing a NSYS2 */
    SYSCALL(TERMPROCESS, 0, 0, 0);
}