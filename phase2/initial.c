#include "headers/initial.h"
#include "../phase3/headers/initProc.h"

/*Declare the Level 3 global variables*/

/*Process Count*/
int PROC_COUNT;
/*Ready Queue*/
struct list_head READY_Q;
/*Current Process*/
pcb_t *CURRENT_PROC[8];

/*Global Lock: values belong to: [0,1] */
volatile unsigned int GLOBAL_LOCK_value;                 // the value of the Global Lock
volatile unsigned int *GLOBAL_LOCK = &GLOBAL_LOCK_value; // a pointer to GLOBAL_LOCK

/*Device Semaphores: one integer semaphore for each external (sub)device,
plus one additional semaphore to support the Pseudo-clock*/
volatile unsigned int DEVICE_SEMAPHORES[SEMDEVLEN];

/*Function to instantiate a single process - the first one*/
void initFirstProcess()
{
    /* process is instantiated by allocating a PCB*/
    pcb_t *first_process = allocPcb();
    /* Initializing the processor state that is part of the PCB.  */
    first_process->p_s.mie = MIE_ALL;                              // enable interrupt
    first_process->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M; // enable interrupt and kernel mode
    RAMTOP(first_process->p_s.reg_sp);
    first_process->p_s.pc_epc = (memaddr)InstantiatorProcess;

    // there are no other processes, the first process has no parent
    first_process->p_parent = NULL;
    // initializing the list of the process children
    INIT_LIST_HEAD(&first_process->p_child);
    // initializing the list of the process siblings
    INIT_LIST_HEAD(&first_process->p_sib);

    first_process->p_time = 0;
    first_process->p_semAdd = NULL;
    first_process->p_supportStruct = NULL;

    insertProcQ(&READY_Q, first_process);
    PROC_COUNT++;
}

/* For each IRT entry (Table 1) the RP bit must be set to 1 and the NCPU
least significative bit of destination must be set to 1 */
void configureIRTEntries(__UINT32_TYPE__ *irtBase)
{
    for (int i = 0; i < IRT_NUM_ENTRY; i++)
    {
        irtBase[i] = IRT_RP_BIT_ON;
        irtBase[i] |= 0xff;
    }
}

void main()
{
    /*Populate the Pass Up Vector for each processor*/
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;

    for (int i = 0; i < NCPU; i++)
    {
        if (i == 0)
        {
            /*Set the Stack Pointer for the Nucleus TLB-Refill event handler to the top of
            the Nucleus stack page for CPU0*/
            passupvector[0].tlb_refill_stackPtr = (memaddr)KERNELSTACK;
            /*Set the Stack pointer for the Nucleus exception handler*/
            passupvector[0].exception_stackPtr = (memaddr)KERNELSTACK;
        }
        else
        {
            /*Set the Stack Pointer for the Nucleus TLB-Refill event handler to
            RAMSTART + (64 * PAGESIZE) + (cpu_id * PAGESIZE) for other CPUs*/
            passupvector[i].tlb_refill_stackPtr = (memaddr)0x20020000 + (i * PAGESIZE);
            /*Set the Stack pointer for the Nucleus exception handler: */
            passupvector[i].exception_stackPtr = (memaddr)0x20020000 + (i * PAGESIZE);
        }
        /*Set the Nucleus TLB-Refill*/
        passupvector[i].tlb_refill_handler = (memaddr)uTLB_RefillHandler;
        /*Set the Nucleus exception handler address to the address of exceptionHandler
        that is to be the entry point for exception (and interrupt) handling*/
        passupvector[i].exception_handler = (memaddr)exceptionHandler;
    }
    /*Initialize the Level 2 (Phase 1) data structures*/
    initPcbs();
    initASL();

    /*Initialize all the previously declared variables;*/

    /*There are no started processes during the initialization of the Nucleus*/
    PROC_COUNT = 0;
    /*There are no "ready" processes during the initialization of the Nucleus*/
    INIT_LIST_HEAD(&READY_Q);
    /*There are no "running" processes during the initialization of the Nucleus*/
    for (int i = 0; i < NCPU; i++)
    {
        CURRENT_PROC[i] = NULL;
    }
    /*The GLOBAL_LOCK semaphore is initially free*/
    GLOBAL_LOCK_value = 0;
    /*All the device semaphores are initially free*/
    for (int i = 0; i < SEMDEVLEN; i++)
    {
        DEVICE_SEMAPHORES[i] = 0;
    }

    /*Load the system-wide Interval Timer with 100 milliseconds (constant PSECOND)*/
    LDIT(PSECOND * (*(cpu_t *)(TIMESCALEADDR)));
    /*Instantiate a single process, the first one*/
    initFirstProcess();

    configureIRTEntries((__UINT32_TYPE__ *)IRT_START);
    /* TPR register must be set to 0 just before starting to execute a process */
    *((memaddr *)TPR) = 0;

    state_t cpu_state;

    /*Initialize all the other fields to 0 or null.*/
    cpu_state.cause = 0;
    cpu_state.entry_hi = 0;
    cpu_state.status = MSTATUS_MPP_M;
    cpu_state.mie = 0;
    for (int j = 0; j < 32; j++)
    {
        cpu_state.gpr[j] = 0;
    }

    // Set the state for the other NCPU - 1 CPUs:
    for (int i = 1; i < NCPU; i++)
    {
        cpu_state.pc_epc = (memaddr)scheduler;
        cpu_state.reg_sp = 0x20020000 + (i * PAGESIZE);

        INITCPU(i, &cpu_state);
    }

    scheduler();
}