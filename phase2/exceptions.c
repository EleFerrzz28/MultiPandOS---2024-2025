#include "headers/exceptions.h"

void *
memcpy(void *dest, const void *src, unsigned int len)
{
    char *d = dest;
    const char *s = src;
    while (len--)
        *d++ = *s++;
    return dest;
}

/*
Function that kills the process given in input (and its subtree)
*/
void KILL_PROCESS(pcb_t *process)
{
    // Check if the process given in input doesn't exist
    if (process == NULL)
        return;
    pcb_t *ptr;
    while ((ptr = removeChild(process)) != NULL) // Check if the process has a child
    {
        KILL_PROCESS(ptr); // Recursive call to navigate through the process tree
    }
    // Check, for each CPU, if the current process is being killed
    for (int i = 0; i < NCPU; i++)
    {
        if (CURRENT_PROC[i] == process)
        {
            CURRENT_PROC[i] = NULL;
        }
    }

    freePcb(process); // free the PCB that was previously allocated for that process

    PROC_COUNT--;
}

/*Function which, given a pointer to the memory cell containing the block of
command of the device to which we are referring, returns the correct index within
DEVICE_SEMAPHORES pointing to its associated traffic light
Parameters: a memaddr* pointer to the command field of the device register
Returns: an integer which is the device index inside the DEVICE_SEMAPHORES[]*/
int getSemaphoreDeviceIndex(memaddr *commandAddr)
{
    // Get the base address of the device's register (commandAddr - 1 word)
    memaddr *devAddrBase = (memaddr *)((char *)commandAddr - WORDLEN);

    // beginning address of the terminal devices
    memaddr *terminalReg = (memaddr *)0x10000254;
    // beginning address of the non terminal devices
    memaddr *nonTerminalBase = (memaddr *)0x10000054;

    // Check if the device is not a terminal
    if (devAddrBase < terminalReg)
    {
        /* The subtraction is used to obtain the distance - in bytes - of the device register
        from the beginning of the correct device register addresses (in this case, non terminal);
        then, that distance is divided by 16 bytes (0x10 in hexadecimal), since each non terminal
        device has a 16 bytes register reserved.
        Note that in order to return the index, which is an integer, a casting is required*/
        return ((unsigned int)devAddrBase - (unsigned int)nonTerminalBase) / 0x10;
    }
    else // ...if the device is a terminal:
    {
        /* The subtraction is used to obtain the distance - in bytes - of the device register
        from the beginning of the correct device register addresses (in this case, terminal);
        then, that distance is divided by 8 bytes (0x8 in hexadecimal), since each terminal device
        is made of two sub devices, which both have a 8 bytes register reserved. Eventually,
        we add 32 in order to consider the first non terminal devices.
        Note that in order to return the index, which is an integer, a casting is required*/
        return (((unsigned int)devAddrBase - (unsigned int)terminalReg) / 0x8) + 32;
    }
}

/* Function to perform a PASS UP OR DIE operation */
void PASS_UP_OR_DIE(int except_type, state_t *previous_state)
{
    ACQUIRE_LOCK(GLOBAL_LOCK);
    /* if the Current Process’s p_supportStruct is NULL, then the exception should be handled as a
    TerminateProcess: the Current Process of the current CPU and all its progeny are terminated.
    This is the “die” portion of Pass Up or Die.*/
    if (CURRENT_PROC[getPRID()]->p_supportStruct == NULL)
    {
        KILL_PROCESS(CURRENT_PROC[getPRID()]);
        RELEASE_LOCK(GLOBAL_LOCK);
        scheduler();
    }
    /*If the Current Process’s p_supportStruct is non-NULL. The handling of the exception is “passed
    up”.*/
    else
    {
        CURRENT_PROC[getPRID()]->p_supportStruct->sup_exceptState[except_type] = *previous_state;
        RELEASE_LOCK(GLOBAL_LOCK);
        LDCXT(CURRENT_PROC[getPRID()]->p_supportStruct->sup_exceptContext[except_type].stackPtr, CURRENT_PROC[getPRID()]->p_supportStruct->sup_exceptContext[except_type].status, CURRENT_PROC[getPRID()]->p_supportStruct->sup_exceptContext[except_type].pc);
    }
}

/*CREATE_PROCESS Syscall*/
void CREATE_PROCESS(state_t *saved_exception_state)
{
    ACQUIRE_LOCK(GLOBAL_LOCK);
    pcb_t *new_process = allocPcb();
    if (new_process == NULL) // if there are no available PCBs
        saved_exception_state->reg_a0 = -1;
    else // initialize all the PCB fields
    {
        new_process->p_s = *(state_t *)saved_exception_state->reg_a1;
        new_process->p_supportStruct = (support_t *)saved_exception_state->reg_a3;
        saved_exception_state->reg_a0 = new_process->p_pid;
        insertProcQ(&READY_Q, new_process);                // the new process is inserted in the Ready Queue
        insertChild(CURRENT_PROC[getPRID()], new_process); // the new process is a child of the current process
        PROC_COUNT++;                                      // increment the process count
        new_process->p_time = 0;
        new_process->p_semAdd = NULL;             // the PCB is in the "ready" state, not blocked
        saved_exception_state->pc_epc += WORDLEN; // incrementare il program counter di 4 byte
    }
    RELEASE_LOCK(GLOBAL_LOCK);
    /* ritorno il controllo al processore che si è interrotto caricandogli lo stato precedente alla syscall*/
    LDST(saved_exception_state);
}

/*TERMINATE_PROCESS Syscall*/
void TERMINATE_PROCESS(state_t *saved_exception_state)
{
    ACQUIRE_LOCK(GLOBAL_LOCK);
    // Check if the current process is the one to be killed (reg_a1 = 0)
    if (saved_exception_state->reg_a1 == 0)
    {
        KILL_PROCESS(CURRENT_PROC[getPRID()]);
    }
    else // ...else, kill the process identified by the process id contained in a1
    {
        KILL_PROCESS(GetPCBbyPid(saved_exception_state->reg_a1));
        saved_exception_state->pc_epc += WORDLEN;
    }
    // Check if, after the termination of the process there is no current process
    if (CURRENT_PROC[getPRID()] == NULL)
    {
        RELEASE_LOCK(GLOBAL_LOCK);
        scheduler(); //...if so, call the scheduler
    }
    RELEASE_LOCK(GLOBAL_LOCK);
    LDST(saved_exception_state);
}

/*PASSEREN Syscall*/
void SEM_PASSEREN(state_t *saved_exception_state)
{
    ACQUIRE_LOCK(GLOBAL_LOCK);

    saved_exception_state->pc_epc += WORDLEN;
    int *sem = (int *)saved_exception_state->reg_a1;

    // Check if value of the semaphore given is already 0
    if (*sem == 0)
    {
        insertBlocked(sem, CURRENT_PROC[getPRID()]); // ...if so, the PASSEREN is blocking
        CURRENT_PROC[getPRID()]->p_s = *saved_exception_state;

        /* Update the accumulated CPU time for the Current Process */
        cpu_t current_time;
        STCK(current_time);
        CURRENT_PROC[getPRID()]->p_time += current_time;

        CURRENT_PROC[getPRID()] = NULL;

        RELEASE_LOCK(GLOBAL_LOCK);
        scheduler();
    }
    else // otherwise, it is possible to perform a P operation (decrement the sempahore by 1)
    {
        *sem = 0;

        // Check if there's a process blocked on that semaphore
        if (headBlocked(sem) != NULL)
        {
            // if that is the case, release the process and place it in the Ready Queue
            pcb_t *released = removeBlocked(sem);
            insertProcQ(&READY_Q, released);
            *sem = 1; // simulazione incremento dal verhogen
        }
        RELEASE_LOCK(GLOBAL_LOCK);
        LDST(saved_exception_state);
    }
}

/* VERHOGEN Syscall */
void SEM_VERHOGEN(state_t *saved_exception_state)
{
    ACQUIRE_LOCK(GLOBAL_LOCK);

    saved_exception_state->pc_epc += WORDLEN;
    int *sem = (int *)saved_exception_state->reg_a1;

    // Check if value of the semaphore given is already 1
    if (*sem == 1)
    {
        insertBlocked(sem, CURRENT_PROC[getPRID()]); // ...if so, the VERHOGEN is blocking
        CURRENT_PROC[getPRID()]->p_s = *saved_exception_state;

        /* Update the accumulated CPU time for the Current Process */
        cpu_t current_time;
        STCK(current_time);
        CURRENT_PROC[getPRID()]->p_time += current_time;

        CURRENT_PROC[getPRID()] = NULL;

        RELEASE_LOCK(GLOBAL_LOCK);
        scheduler();
    }
    else
    {
        // otherwise, it is possible to perform a P operation (decrement the sempahore by 1)
        *sem = 1;

        if (headBlocked(sem) != NULL)
        {
            // if that is the case, release the process and place it in the Ready Queue
            pcb_t *released = removeBlocked(sem);
            insertProcQ(&READY_Q, released);
            *sem = 0; // simulazione incremento dal verhogen
        }
        RELEASE_LOCK(GLOBAL_LOCK);
        LDST(saved_exception_state);
    }
}

/* NOTA: ogni volta in cui si reuperano valori dai registri a1,...,a3 bisogna
tenere a mente che si tratta di valori INTERI: casting?*/
void DO_IO(state_t *saved_exception_state)
{
    ACQUIRE_LOCK(GLOBAL_LOCK);
    // address of the command field
    int *commandAddr = (int *)saved_exception_state->reg_a1;
    int semIndex = getSemaphoreDeviceIndex((memaddr *)commandAddr);
    (DEVICE_SEMAPHORES[semIndex])++;
    insertBlocked((int *)&DEVICE_SEMAPHORES[semIndex], CURRENT_PROC[getPRID()]);

    saved_exception_state->pc_epc += WORDLEN;
    CURRENT_PROC[getPRID()]->p_s = *saved_exception_state;
    /* Update the accumulated CPU time for the Current Process */
    cpu_t current_time;
    STCK(current_time); // salva in current_time il tempo cpu attuale
    CURRENT_PROC[getPRID()]->p_time += current_time;
    CURRENT_PROC[getPRID()] = NULL;
    RELEASE_LOCK(GLOBAL_LOCK);
    // value to be assigned
    int commandValue = saved_exception_state->reg_a2;
    // assign a value to that command field for that device
    *commandAddr = commandValue;
    scheduler();
}

void GetCPUTime(state_t *saved_exception_state)
{
    /*CURRENT_PROC[getPRID()]->p_time --> tempo di tutte le esecuzioni del processo accumulate*/
    cpu_t current_time;
    STCK(current_time); // salva in current_time il tempo cpu attuale

    saved_exception_state->reg_a0 = CURRENT_PROC[getPRID()]->p_time + current_time;

    saved_exception_state->pc_epc += WORDLEN;
    LDST(saved_exception_state);
}

/*in sezione 7 dice di fare p_time + il tempo della cpu durante il quantum time
slice che è gestito dal plt tuttavia la sezione 7 dice di usare il tod per calcolare
la durata di un intervallo*/

void WaitForClock(state_t *saved_exception_state)
{
    ACQUIRE_LOCK(GLOBAL_LOCK);
    DEVICE_SEMAPHORES[SEMDEVLEN - 1] += 1;
    insertBlocked((int *)&DEVICE_SEMAPHORES[SEMDEVLEN - 1], CURRENT_PROC[getPRID()]);
    saved_exception_state->pc_epc += WORDLEN;
    CURRENT_PROC[getPRID()]->p_s = *saved_exception_state;

    /* Update the accumulated CPU time for the Current Process */
    cpu_t current_time;
    STCK(current_time); // salva in current_time il tempo cpu attuale
    CURRENT_PROC[getPRID()]->p_time += current_time;

    CURRENT_PROC[getPRID()] = NULL; // finito finito

    RELEASE_LOCK(GLOBAL_LOCK);
    scheduler();
}

void GetSupportData(state_t *saved_exception_state)
{
    if (CURRENT_PROC[getPRID()]->p_supportStruct != NULL)
    {
        saved_exception_state->reg_a0 = (unsigned int)CURRENT_PROC[getPRID()]->p_supportStruct;
    }
    else
    {
        saved_exception_state->reg_a0 = 0; // NULL
    }
    saved_exception_state->pc_epc += WORDLEN;
    LDST(saved_exception_state);
}

void GetProcessID(state_t *saved_exception_state)
{
    if (saved_exception_state->reg_a1 == 0)
        saved_exception_state->reg_a0 = CURRENT_PROC[getPRID()]->p_pid;
    else
    {
        if (CURRENT_PROC[getPRID()]->p_parent != NULL)
            saved_exception_state->reg_a0 = CURRENT_PROC[getPRID()]->p_parent->p_pid;
        /* se stiamo operando sul root process - che nn ha genitore - ...*/
        else
            saved_exception_state->reg_a0 = 0;
    }
    saved_exception_state->pc_epc += WORDLEN;
    LDST(saved_exception_state);
}

/* Function to handle Program Trap exceptions:a Program Trap exception occurs when the
Current Process attempts to perform some illegal or undefined action.
The Nucleus Program Trap exception handler should perform a standard Pass Up or Die operation
using the GENERALEXCEPT index value.*/
void PROGRAM_TRAP_EXCEPTION_HANDLER(state_t *saved_exception_state)
{
    PASS_UP_OR_DIE(GENERALEXCEPT, saved_exception_state);
}

void SYSCALL_EXCEPTION_HANDLER(state_t *saved_exception_state)
{
    /*if isKernel_mode is equal to 1 then we are in kernel mode*/
    int isKernel_mode = saved_exception_state->status & MSTATUS_MPP_MASK;
    /* save off a pointer to the a0 register: it will be used to
    perform the correct SYSCALL depending on the relative constant
    contained in a0 */
    unsigned int *a0 = &saved_exception_state->reg_a0;

    if (isKernel_mode)
    {
        switch (*a0)
        {
        case CREATEPROCESS:
            // if a0 is equal to -1 then it's a Create Process Syscall
            CREATE_PROCESS(saved_exception_state);
            break;
        case TERMPROCESS:
            // if a0 is equal to -2 then it's a Terminate Process Syscall
            TERMINATE_PROCESS(saved_exception_state);
            break;
        case PASSEREN:
            // if a0 is equal to -3 then it's a Passeren Syscall
            SEM_PASSEREN(saved_exception_state);
            break;
        case VERHOGEN:
            // if a0 is equal to -4 then it's a Verhogen Syscall
            SEM_VERHOGEN(saved_exception_state);
            break;
        case DOIO:
            // if a0 is equal to -5 then it's a Do Input Output Syscall
            DO_IO(saved_exception_state);
            break;
        case GETTIME:
            // if a0 is equal to -6 then it's a Get CPU Time Syscall
            GetCPUTime(saved_exception_state);
            break;
        case CLOCKWAIT:
            // if a0 is equal to -7 then it's a Wait For Clock Syscall
            WaitForClock(saved_exception_state);
            break;
        case GETSUPPORTPTR:
            // if a0 is equal to -8 then it's a Get Support Struct Syscall
            GetSupportData(saved_exception_state);
            break;
        case GETPROCESSID:
            // if a0 is equal to -9 then it's a Get Process ID Syscall
            GetProcessID(saved_exception_state);
            break;
        default:
            // in all other cases, the control is passed to the program trap exception handler
            PROGRAM_TRAP_EXCEPTION_HANDLER(saved_exception_state);
            break;
        }
    }
    else // ...if not in kernel mode:
    {
        if (*a0 < 0){
            saved_exception_state->cause = PRIVINSTR;
        }
        PROGRAM_TRAP_EXCEPTION_HANDLER(saved_exception_state);
    }
}

/* Function that handles the exceptions; exceptionHandler will be called
(with a fresh stack) after each and every exception. Depending on the CAUSE
of the exception, it will pass control a certain function. */
void exceptionHandler()
{
    state_t *previous_state = GET_EXCEPTION_STATE_PTR(getPRID());
    unsigned int exception_code = getCAUSE();

    if (CAUSE_IS_INT(exception_code))
        // Processing passed along to the Nucleus’s device interrupt handler
        INTERRUPT_HANDLER(previous_state, exception_code);
    else if (exception_code >= 24 && exception_code <= 28)
        // Processing passed along to the Nucleus’s TLB exception handler
        PASS_UP_OR_DIE(PGFAULTEXCEPT, previous_state);
    else if (exception_code == 8 || exception_code == 11)
        // Processing passed along to the Nucleus’s SYSCALL exception handler
        SYSCALL_EXCEPTION_HANDLER(previous_state);
    else if (exception_code >= 0 && exception_code < 24)
        // Processing passed along to the Nucleus’s Program Trap exception handler
        PROGRAM_TRAP_EXCEPTION_HANDLER(previous_state);
    else
        PANIC();
}