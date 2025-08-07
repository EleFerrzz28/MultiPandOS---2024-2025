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

}

/* Syscall exception handler: The nucleus directly handles all NSYS SYSCALL exceptions 
(those having negative identifiers). For all other SYSCALL exceptions the nucleus either 
treats the exception as a NSYS2 (terminate) or “passes up” the handling of the exception 
if the offending process was provided a non-NULL value for its Support Structure pointer when it was created*/
void SYSCALL_EXCEPTION_HANDLER(state_t *saved_exception_state){
    
}

/* General exception handler*/
// IMPORTANT: The processor state at the time of the exception will be in the Support Structure’s corresponding sup_exceptState field
void support_exception_handler(){
    
}

