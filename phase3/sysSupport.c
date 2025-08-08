#include "headers/sysSupport.h";

/* sysSupport.c: This module implements the Support Level’s:
• General exception handler [Section 6].
• SYSCALL exception handler [Section 7].
• Program Trap exception handler [Section 8].*/

/* Program Trap exception handler: for all Program Trap exceptions, the nucleus either treats 
the exception as a process termination or “passes up” the handling of the exception if the 
offending process was provided a non-NULL value for its Support Structure pointer when it was created. 
The Support Level’s Program Trap exception handler is to terminate the process in an orderly
fashion; perform the same operations as a SYS2 request.*/
void PROGRAM_TRAP_EXCEPTION_HANDLER_SUPPORT(state_t *saved_exception_state){
    
    SYSCALL(TERMPROCESS, 0, 0, 0);
}


/* Syscall Terminate: this services causes the executing U-proc to cease to exist. The SYS2 service is essentially a user-mode
“wrapper” for the kernel-mode restricted NSYS2 service. */
void Terminate (state_t *saved_exception_state){
    SYSCALL(TERMPROCESS, saved_exception_state->reg_a1, 0, 0);
}

/* Syscall exception handler: The nucleus directly handles all NSYS SYSCALL exceptions 
(those having negative identifiers). For all other SYSCALL exceptions the nucleus either 
treats the exception as a NSYS2 (terminate) or “passes up” the handling of the exception 
if the offending process was provided a non-NULL value for its Support Structure pointer when it was created*/
void SYSCALL_EXCEPTION_HANDLER(state_t *saved_exception_state){
    /* save off a pointer to the a0 register: it will be used to
    perform the correct SYSCALL depending on the relative constant
    contained in a0 */
    unsigned int *a0 = &saved_exception_state->reg_a0;
    switch(*a0){
        case TERMINATE:
            break;
        case WRITEPRINTER:
            break;
        case WRITETERMINAL:
            break;
        // NOTA: mancava la costante, aggiunta in const.h
        case READTERMINAL:
            break;
        default:
            PROGRAM_TRAP_EXCEPTION_HANDLER_SUPPORT(saved_exception_state);
            break;
        
    }
}

/* General exception handler*/
// IMPORTANT: The processor state at the time of the exception will be in the Support Structure’s corresponding sup_exceptState field
void support_exception_handler(){
    state_t *saved_exception_state = CURRENT_PROC[getPRID()]->p_supportStruct->sup_exceptState;
    int CAUSE = CURRENT_PROC[getPRID()]->p_supportStruct->sup_exceptState->cause;
    if(CAUSE >= 0 && CAUSE < 24 && CAUSE != 8 && CAUSE != 11){        
        PROGRAM_TRAP_EXCEPTION_HANDLER_SUPPORT(saved_exception_state);
    } else {
        SYSCALL_EXCEPTION_HANDLER(saved_exception_state);
    }
}

