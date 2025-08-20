#include "./headers/scheduler.h"

/*Important: READY_Q is a shared data structure between the CPUs. So before access or
change it, you must acquire the Global Lock with ACQUIRE_LOCK and then release it with
RELEASE_LOCK when it is no more necessary.*/

/*
Every ready process should have an opportunity to be executed. 
indeed, the task of the scheduler is to deals with that.
The scheduler makes sure that everything is perfectly synchronized
to allow all processes to be executed.
*/
void scheduler()
{
    ACQUIRE_LOCK(GLOBAL_LOCK);
    //Remove the PCB from the head of the Ready Queue, READY_Q...
    pcb_t *current_process = removeProcQ(&READY_Q);

    //If the Ready Queue is not empty...
    if (current_process != NULL)
    {
        //Load 5 milliseconds on the PLT
        setTIMER(TIMESLICE * (*(cpu_t *)(TIMESCALEADDR)));

        // TIME HANDLING:
        cpu_t current_time; 
        STCK(current_time); // save the current cpu time in current_time
        /* update the current_process time field by removing the cpu time
           in which the process wasn't running */
        current_process->p_time -= current_time;

        /*store the pointer to the PCB in the Current Process field of the current CPU
        (Dispatching a process transitions it from a “ready” process to a “running” process)*/
        CURRENT_PROC[getPRID()] = current_process;
        /*set TPR register to 0.*/
        *((memaddr *)TPR) = 0;
        RELEASE_LOCK(GLOBAL_LOCK);
        /*Perform a Load Processor State (LDST) on the processor state stored in PCB of the
        Current Process (p_s) of the current CPU*/
        LDST(&(current_process->p_s));
    }
    else // Ready Queue is empty...
    {
        if (PROC_COUNT == 0) // ...and there are no active processes
        {
            RELEASE_LOCK(GLOBAL_LOCK);
            /* Important: Before calling HALT in the scheduler, one needs to add this snippet of code because
            the BIOS expects that the interrupts arrives at the processor that calls HALT.*/

            unsigned int *irt_entry = (unsigned int*) IRT_START;
            for (int i = 0; i < IRT_NUM_ENTRY; i++) {
                *irt_entry = getPRID();
                irt_entry++;
            }
            HALT();
        }
        else if (PROC_COUNT > 0) // ...and there are active processes
        {
            /*set TPR register to 1.
            (This register holds the priority of tasks running on each processor:
            There are only two possible priorities, 0 and 1, 
            representing the highest and lowest priorities respectively) */
            RELEASE_LOCK(GLOBAL_LOCK);
            *((memaddr *)TPR) = 1;
            /*set the mie register t*/
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            setSTATUS(status);

            /*Wait state: the processor is not executing instructions, 
            but it is waiting for a device interrupt to occur*/
            WAIT();
        }
    }
}
