#include "headers/sysSupport.h";

/* sysSupport.c: This module implements the Support Level’s:
• General exception handler 
• SYSCALL exception handler 
• Program Trap exception handler */


/* Function that, given a pointer to the values in a1 and a2 registers, checks wether they are correct.
Infact, it is an error to write to a printer device from an address outside of the requesting U-proc’s logical
address space, request a SYS3 with a length less than 0, or a length greater than 128. Any of these
errors should result in the U-proc being terminated (SYS2).*/
void checkError(int *a2, char *a1){
    if(*a2 < 0 || *a2 > 128 || *a1 < 0x80000000 || *a1 > 0x8001E000 || *a1 < 0xBFFFF000 || *a1 > USERSTACKTOP ) 
        SYSCALL(TERMINATE, 0, 0, 0);
        
    // Compute the last address occupied by the string
    char *check_address = *a1 + 0x8*(*a2);

    if((*check_address > 0x8001E000 && *check_address < 0xBFFFF000) || *check_address > USERSTACKTOP) 
        SYSCALL(TERMINATE, 0, 0, 0);
}

/* Function that, given a U-proc's pid and the device type, calculates the correct device
associated to the given process and its corrisponding index to be used in the U_DEVICE_SEMAPHORES.
As for the mapping of a U-proc's devices, the first U-proc will operate with U_DEVICE_SEMAPHORES[0...3],
following the order Flash Device, Printer Device, Terminal Device, and so one for the other seven U-procs.
N.B.: in order to correctly handle terminal devices, the value in the 4th parameter is checked:
• rw = -1 => Flash Devices/Printer Devices
• rw = 0 => Terminal device - receiver
• rw = 1 => Terminal device - transmitter
Eventually, the first parameter is a pointer to dev_reg_addr*/
int getDevice(dtpreg_t *dev_reg_addr, int pid, int devType, int rw){
    // Device address
    dev_reg_addr = (dtpreg_t*)DEV_REG_ADDR(devType, pid);

    int dev_index;
    // device index
    switch(devType){
        case IL_FLASH:
            dev_index = (pid-1)*4;
            break;
        case IL_PRINTER:
            dev_index = (pid-1)*4+1;
            break;
        case IL_TERMINAL:
            dev_index = (pid-1)*4+2;
            if(rw == 1){
                dev_index += 1;
                dev_reg_addr += 0x8;
            }
            break;
        default:
            PROGRAM_TRAP_EXCEPTION_HANDLER_SUPPORT();
            break;
    }
    return dev_index;
}

/* Program Trap exception handler: for all Program Trap exceptions, the nucleus either treats 
the exception as a process termination or “passes up” the handling of the exception if the 
offending process was provided a non-NULL value for its Support Structure pointer when it was created. 
The Support Level’s Program Trap exception handler is to terminate the process in an orderly
fashion. */
void PROGRAM_TRAP_EXCEPTION_HANDLER_SUPPORT(){
    // Check if the process to be terminated is currently holding mutual exclusion 
    // on the Swap Pool Table
    if(CURRENT_PROC[getPRID()] == swap_holder){
        // if so, mutual exclusion is released 
        SYSCALL(VERHOGEN, (int)&SWAP_POOL_SEMAPHORE, 0, 0);
        // There's now no process holding mutual exclusion on the Swap Pool Table
        swap_holder = NULL;
    }
    SYSCALL(TERMPROCESS, 0, 0, 0);
}


/* Syscall Terminate: this service causes the executing U-proc to cease to exist. The SYS2 service is essentially a user-mode
“wrapper” for the kernel-mode restricted NSYS2 service. */
void Terminate (state_t *saved_exception_state){
    // V operation on the Master Semaphore
    SYSCALL(VERHOGEN, (int)&MASTER_SEMAPHORE, 0, 0);
    SYSCALL(TERMPROCESS, saved_exception_state->reg_a1, 0, 0);
}

/* Syscall WritePrinter: this service causes the requesting U-proc to be suspended until a line of output
(string of characters) has been transmitted to the printer device associated with the U-proc.*/
void WritePrinter(state_t *saved_exception_state){
    char *a1 = &saved_exception_state->reg_a1;
    int *a2 = &saved_exception_state->reg_a2;

    // Check if the parameters a1 and a2 are "legal"
    checkError(*a2,*a1);

    // Save off the length of the string
    int i = *a2;
 
    dtpreg_t *dev_reg_address;
    // Save off the current process' id
    int pid = CURRENT_PROC[getPRID()]->p_pid;

    // Compute the correct index for that U-proc and its printer's register address
    int dev_index = getDevice(dev_reg_address, pid, IL_PRINTER, -1); //-1: writePrinter, 0: readTerminal, 1: writeTerminal
    
    // Acquire mutual exclusion on the correct device's semaphore
    SYSCALL(PASSEREN, (int)&U_DEVICE_SEMAPHORES[dev_index], 0, 0);
    // The following while loop, containing the operations that are needed in order to
    // print characters, will either terminate when i == 0 or when a value different from
    // "Device Ready" is found in the status field
    while(i > 0){
        if(dev_reg_address->status != READY) break;
        // Write the address of the character to be printed in data0
        dev_reg_address->data0 = *a1;
        // Write the correct command in the command field
        dev_reg_address->command = PRINTCHR;
        // Execute DOIO to activte the command
        SYSCALL(DOIO, (int)dev_reg_address, (int)dev_reg_address->command, 0);
        // Eventually, go to the next character...
        *a1 += 0x8;
        // ...and decement i
        i--;
    }
    // Release mutual exclusion on the correct device's sempahore
    SYSCALL(VERHOGEN, (int)&U_DEVICE_SEMAPHORES[dev_index], 0, 0);
    if(i == 0) saved_exception_state->reg_a0 = *a2; // the number of characters actually printed is returned in a0...
    else saved_exception_state->reg_a0 = -dev_reg_address->status; // ...or the negative of the device’s status value.
    // the Support Level’s SYSCALL exception handler must also increment the PC by 4 in order to return control to
    // the instruction after the SYSCALL instruction 
    saved_exception_state->pc_epc += WORDLEN;
    LDST(saved_exception_state);
}

/* Syscall WriteTerminal: this service causes the requesting U-proc to be suspended until a line of output
(string of characters) has been transmitted to the terminal device associated with the U-proc.*/
void WriteTerminal(state_t *saved_exception_state){
    char *a1 = &saved_exception_state->reg_a1;
    int *a2 = &saved_exception_state->reg_a2;

    // Check if the parameters a1 and a2 are "legal"
    checkError(*a2,*a1);

    // Save off the length of the string
    int i = *a2;
 
    dtpreg_t *dev_reg_address;
    // Save off the current process' id
    int pid = CURRENT_PROC[getPRID()]->p_pid;

    // Compute the correct index for that U-proc and its terminal's register address
    int dev_index = getDevice(dev_reg_address, pid, IL_TERMINAL, 1); //-1: writePrinter, 0: readTerminal, 1: writeTerminal
    
    // Acquire mutual exclusion on the correct device's semaphore
    SYSCALL(PASSEREN, (int)&U_DEVICE_SEMAPHORES[dev_index], 0, 0);
    // The following while loop, containing the operations that are needed in order to
    // transmit characters, will either terminate when i == 0 or when a value different from
    // "Character Transmitted" is found in the status field
    while(i > 0){
        if(dev_reg_address->status != OKCHARTRANS) break;
        // Write the address of the character to be printed in data0
        dev_reg_address->data0 = *a1;
        // Write the correct command in the command field
        dev_reg_address->command = TRANSMITCHAR;
        // Execute DOIO to activte the command
        SYSCALL(DOIO, (int)dev_reg_address, (int)dev_reg_address->command, 0);
        // Eventually, go to the next character...
        *a1 += 0x8;
        // ...and decement i
        i--;
    }
    // Release mutual exclusion on the correct device's semaphore
    SYSCALL(VERHOGEN, (int)&U_DEVICE_SEMAPHORES[dev_index], 0, 0);
    if(i == 0) saved_exception_state->reg_a0 = *a2; // the number of characters actually transmitted is returned in a0...
    else saved_exception_state->reg_a0 = -dev_reg_address->status; //...or the negative of the device’s status value.
    // the Support Level’s SYSCALL exception handler must also increment the PC by 4 in order to return control to
    // the instruction after the SYSCALL instruction 
    saved_exception_state->pc_epc += WORDLEN;
    LDST(saved_exception_state);
}

/* Syscall ReadTerminal: this service causes the requesting U-proc to be suspended until a line of input (string
of characters) has been transmitted from the terminal device associated with the U-proc.*/
void ReadTerminal(state_t *saved_exception_state){
    char *a1 = &saved_exception_state->reg_a1;
    int *a2 = &saved_exception_state->reg_a2;

    // Check if the parameters a1 and a2 are "legal"
    checkError(*a2,*a1);

    // Save off the length of the string
    int i = *a2;
 
    dtpreg_t *dev_reg_address;
    // Save off the current process' id
    int pid = CURRENT_PROC[getPRID()]->p_pid;

    // Compute the correct index for that U-proc and its terminal's register address
    int dev_index = getDevice(dev_reg_address, pid, IL_TERMINAL, 0); //-1: writePrinter, 0: readTerminal, 1: writeTerminal
    
    // Acquire mutual exclusion on the correct device's semaphore
    SYSCALL(PASSEREN, (int)&U_DEVICE_SEMAPHORES[dev_index], 0, 0);
    // The following while loop, containing the operations that are needed in order to
    // transmit characters, will either terminate when i == 0 or when a value different from
    // "Character Received" is found in the status field
    while(i > 0){
        if(dev_reg_address->status != CHARRECV) break;
        // Write the address of the character to be printed in data0
        dev_reg_address->data0 = *a1;
        // Write the correct command in the command field
        dev_reg_address->command = RECEIVECHAR;
        // Execute DOIO to activte the command
        SYSCALL(DOIO, (int)dev_reg_address, (int)dev_reg_address->command, 0);
        // Eventually, go to the next character...
        *a1 += 0x8;
        // ...and decement i
        i--;
    }
    // Release mutual exclusion on the correct device's semaphore
    SYSCALL(VERHOGEN, (int)&U_DEVICE_SEMAPHORES[dev_index], 0, 0);
    if(i == 0) saved_exception_state->reg_a0 = *a2; // the number of characters actually received is returned in a0...
    else saved_exception_state->reg_a0 = -dev_reg_address->status; //...or the negative of the device’s status value.
    // the Support Level’s SYSCALL exception handler must also increment the PC by 4 in order to return control to
    // the instruction after the SYSCALL instruction 
    saved_exception_state->pc_epc += WORDLEN;
    LDST(saved_exception_state);
}

/* Syscall exception handler: The nucleus directly handles all NSYS SYSCALL exceptions 
(those having negative identifiers). For all other SYSCALL exceptions the nucleus either 
treats the exception as a NSYS2 (terminate) or “passes up” the handling of the exception 
if the offending process was provided a non-NULL value for its Support Structure pointer when it was created */
void SYSCALL_EXCEPTION_HANDLER(state_t *saved_exception_state){
    /* save off a pointer to the a0 register: it will be used to
    perform the correct SYSCALL depending on the relative constant
    contained in a0 */
    unsigned int *a0 = &saved_exception_state->reg_a0;
    switch(*a0){
        case TERMINATE:
            Terminate(*a0);
            break;
        case WRITEPRINTER:
            WritePrinter(*a0);
            break;
        case WRITETERMINAL:
            WriteTerminal(*a0);
            break;
        // NOTA: mancava la costante READTERMINAL, aggiunta in const.h
        case READTERMINAL:
            ReadTerminal(*a0);
            break;
        default:
            PROGRAM_TRAP_EXCEPTION_HANDLER_SUPPORT(saved_exception_state);
            break;
    }
}

/* General exception handler: The Support Level general exception handler will process all passed up non-TLB exceptions:
• All SYSCALL (SYSCALL) exceptions numbered 1 and above (positive number).
• All Program Trap exceptions namely all exception causes except of those for SYSCALL excep-tions and those related to TLB exceptions.*/
void support_exception_handler(){
    // The processor state at the time of the exception will be in the Support Structure’s corresponding sup_exceptState field
    state_t *saved_exception_state = CURRENT_PROC[getPRID()]->p_supportStruct->sup_exceptState;
    // After examining the sup_exceptState’s Cause register, the Support Level general exception handler
    // will pass control to either the Support Level’s SYSCALL exception handler or to the
    // Support's Level's Program Trap exception handler
    int CAUSE = CURRENT_PROC[getPRID()]->p_supportStruct->sup_exceptState->cause;
    if(CAUSE >= 0 && CAUSE < 24 && CAUSE != 8 && CAUSE != 11){        
        PROGRAM_TRAP_EXCEPTION_HANDLER_SUPPORT(saved_exception_state);
    } else {
        SYSCALL_EXCEPTION_HANDLER(saved_exception_state);
    }
}