#include "headers/initProc.h"

// Initialize a process's page table
void initPageTableEntry(pteEntry_t *entry, int asid, int i) {
  if (i < 31)
    entry->pte_entryHI = KUSEG + (i << VPNSHIFT) + (asid << ASIDSHIFT);
  else
    entry->pte_entryHI = 0xBFFFF000 + (asid << ASIDSHIFT); // stack page
  entry->pte_entryLO = DIRTYON;
}

// Initial Processor State for a U-proc
void initProcessState(state_t *processState, int index){
    processState->pc_epc = UPROCSTARTADDR;
    processState->reg_sp = USERSTACKTOP;
    processState->mie = MIE_ALL;
    processState->status = MSTATUS_MPIE_MASK | MSTATUS_MPP_U;
    processState->entry_hi = index << ASIDSHIFT; // lo sposti dove l'hardware si aspetta di trovarlo
}

// Initialization of a Support Structure for a U-proc
void initSupportStructures(support_t *supportStructure, int index){
    /* Set the process's ASID */
    supportStructure->sup_asid = index;
    /* Set the two PC fields. One of them (0 - PGFAULTEXCEPT) should be set to the address of the
    Support Level’s TLB handler, while the other one (1 - GENERALEXCEPT) should be set to the
    address of the Support Level’s general exception handler.*/
    supportStructure->sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr )uTLB_RefillHandler;
    supportStructure->sup_exceptContext[GENERALEXCEPT].pc = (memaddr )support_exception_handler;
    /* Set the two Status registers to kernel-mode.*/
    supportStructure->sup_exceptContext[PGFAULTEXCEPT].status |= MSTATUS_MPP_M;
    supportStructure->sup_exceptContext[GENERALEXCEPT].status |= MSTATUS_MPP_M;
    /* Set the two SP fields to utilize the two stack spaces allocated in the Support Structure*/
    supportStructure->sup_exceptContext[PGFAULTEXCEPT].stackPtr = &(supportStructure->sup_stackTLB[499]);
    supportStructure->sup_exceptContext[GENERALEXCEPT].stackPtr = &(supportStructure->sup_stackGen[499]);
}

void InstantiatorProcess (){
    // Init swap pool
    for(int i = 0; i < POOLSIZE; i++){
        SWAP_POOL[i].sw_asid = NOPROC;
    }

    // Init swap pool semaphore
    SWAP_POOL_SEMAPHORE = 1;
    // Init masterSemaphore
    MASTER_SEMAPHORE = 0;

    for(int i = 0; i < 3*UPROCMAX; i++){
        U_DEVICE_SEMAPHORES[i] = 1;
    }

    // array di stati per gli 8 u procs
    state_t UprocState[UPROCMAX];
    // array di support struct per gli 8 u procs
    support_t supportStructures[UPROCMAX];

    for(int i = 1; i <= UPROCMAX; i++){
        initProcessState(&UprocState[i], i);
        initSupportStructures(&supportStructures[i], i);
        for(int j = 0; j < USERPGTBLSIZE; j++){
            initPageTableEntry(supportStructures[i].sup_privatePgTbl, i, j);
        }
        /*  Initialize and launch (NSYS1) between 1 and 8 U-procs. */
        SYSCALL(CREATEPROCESS, (int)&UprocState[i], PROCESS_PRIO_LOW, (int)&supportStructures[i]);
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