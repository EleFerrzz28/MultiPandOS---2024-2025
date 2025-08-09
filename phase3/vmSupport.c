#include "vmSupport.h"
#include "sysSupport.h"

/*Declaring the structures - module wide*/
unsigned int SWAP_POOL_SEMAPHORE;
unsigned int MASTER_SEMAPHORE;
swap_t SWAP_POOL[POOLSIZE];
volatile unsigned int U_DEVICE_SEMAPHORES[3*UPROCMAX];

/* Function that given a page table entry updates - if needed! - the TLB. This is done
by probing the TLB (TLBP) to see if the newly updated TLB entry is indeed cached in the TLB. If
so (Index.P is 0), rewrite (update) that entry (TLBWI) to match the entry in the Page Table*/
void update_tlb(pteEntry_t page_table_entry){
    // Set EntryHi register with the given page
    setENTRYHI(page_table_entry.pte_entryHI);
    // Execute TLB-Probe
    TLBP();
    if(getINDEX() != PRESENTFLAG){// Check if the page is cached in the TLB
        // if so, set the EntryLo register and execute TLBWI() instruction
        setENTRYLO(page_table_entry.pte_entryLO);
        TLBWI();
    }
}

/* Function that implements MultiPandOS page replacement algorithm, 
the suggested one is First in Firs Out*/
int get_victim_frame(){
    // First of all, check if there is a frame which is currently free
    for (int i = 0; i < POOLSIZE; i++){
        if (SWAP_POOL[i].sw_asid == NOPROC) // -1 = free page
        return i;
    }

    // If all frames are occupied, use FIFO algorithm to chose the victim
    static int i = -1;
    return i = (i + 1) % POOLSIZE;
}

/* Function that cleans the page associated to the page table entry given */
void clean_page(pteEntry_t* page_table_entry){
    // Mark Page Table entry k as not valid
    page_table_entry->pte_entryLO = ~VALIDON;
    // Update TLB
    update_tlb(*page_table_entry);
}

/* Function that handles reading (or writing) on the correct backing store, given the ASID,
the number of the page to be read (or written) and its address, and a 4th parameter which
indicates wether the page to distinguish reading or writing operations*/
void rw_backing_store(int asid, int page_no, memaddr frame_addr, int rw){
    // Find ASID's Flash Device
    dtpreg_t *flash_device_reg = (dtpreg_t *)DEV_REG_ADDR(IL_FLASH, asid - 1);
    // Compute the block number --> possiamo omettere questa istruzione??
    int block_number = page_no;
    // Write the flash device’s DATA0 field with the appropriate starting physical address of the 4k
    // block to be read (or written); the particular frame’s starting address.
    flash_device_reg->data0 = frame_addr;
   
    // Write the correct command in the flash device's COMMAND field
    if (rw == 1){
        flash_device_reg->command = (block_number << 8) | FLASHWRITE;
    } else {
        flash_device_reg->command = (block_number << 8) | FLASHREAD;
    }
    // Use the NSYS5 system call to write the flash device’s COMMAND field with the device block
    // number (high order three bytes) and the command to read (or write) in the lower order byte.
    SYSCALL(DOIO, (int)flash_device_reg, (int)flash_device_reg->command, 0);

    // Controlla esito
    // if(flash_device_reg->status != 1){
        // Program trap
        // PROGRAM_TRAP_EXCEPTION_HANDLER_SUPPORT();
    // }
}

/* TLB-refill event handler to which the control is passed after a logical address-translation's
search of the TLB for a matching entry fails. This function will: 
    • Locate the correct Page Table entry in the Current Process’s Page Table; a component of
    p_supportStruct.
    • Write the entry into the TLB using the TLBWR instruction.
    • Return control (LDST) to the Current Process to restart the address translation process.*/
void uTLB_RefillHandler(){
    // Determine the page number (denoted as p) of the missing TLB entry, which is done by 
    // inspecting EntryHi in the saved exception state of the CPU
    state_t *saved_exception_state = GET_EXCEPTION_STATE_PTR(getPRID());
    int p = ENTRYHI_GET_VPN(saved_exception_state->entry_hi);

    // Get the Page Table entry for page number p for the Current Process
    pteEntry_t page_table_entry = CURRENT_PROC[getPRID()]->p_supportStruct->sup_privatePgTbl[p];

    // Write the Page Table entry into the TLB
    setENTRYHI(page_table_entry.pte_entryHI);
    setENTRYLO(page_table_entry.pte_entryLO);
    TLBWR();

    // Return control to the Current Process to retry the instruction that caused the TLB-Refill event
    // riguardare: saved_exception_state->pc_epc += WORDLEN;
    LDST(&saved_exception_state);
}

/* Function that handles page faults that are passed up by the Nucleus to the Support Level’s TLB exception
handler – the Pager. The function below, pager(), handles three different TLB exceptions:
• Page fault on a load operation: TLB-Invalid exception – TLBL
• Page fault on a store operation: TLB-Invalid exception – TLBS
• An attempted write to a read-only page: TLB-Modification exception – Mod*/
void pager(){
    // Obtain the pointer to the Current Process’s Support Structure
    support_t* curr_proc_sup_structure = SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    
    // Determine the cause of the TLB exception. The saved exception state responsible for this
    // TLB exception should be found in the Current Process’s Support Structure for TLB exceptions
    state_t *saved_exception_state = &curr_proc_sup_structure->sup_exceptState[0];

    unsigned int CAUSE = saved_exception_state->cause;
    if(CAUSE == EXC_MOD){// if the cause is a TLB-Modification exception...
        PROGRAM_TRAP_EXCEPTION_HANDLER_SUPPORT(saved_exception_state); // ...program trap
    }

    // Gain mutual exclusion over the Swap Pool table
    SYSCALL(PASSEREN, (int)&SWAP_POOL_SEMAPHORE, 0, 0);

    // Determine the missing page number (denoted as p)
    int p = ENTRYHI_GET_VPN(saved_exception_state->entry_hi);

    // Get the Page Table entry for page number p for the Current Process in its support structure
    pteEntry_t page_table_entry = curr_proc_sup_structure->sup_privatePgTbl[p];

    if(SWAP_POOL[p].sw_asid!=NOPROC){// Check if the page is loaded in the swap pool
        update_tlb(page_table_entry); // Update the TLB
        if(page_table_entry.pte_entryLO & VALIDON){ // if the page is valid...
            SYSCALL(VERHOGEN, (int)&SWAP_POOL_SEMAPHORE, 0, 0); // ...release mutual exclusion
            LDST(&saved_exception_state); // and return control to the Current Process 
        }
    }

    // Pick a frame, i, from the Swap Pool.
    int i = get_victim_frame(); 
    // Compute the frame's address by using the SWAP_POOL_AREA offset and the areas
    // occupied by the other pages untile the i
    memaddr victim_addr = (memaddr)SWAP_POOL_START + (i * PAGESIZE);

    // if frame i is curretly occupied...
    if (SWAP_POOL[i].sw_asid != NOPROC){
        clean_page(SWAP_POOL[i].sw_pte); // ...clean the page
        // and update process x's backing store, the previous owner of frame i
        rw_backing_store(SWAP_POOL[i].sw_asid, SWAP_POOL[i].sw_pageNo, victim_addr, 1); // 4th parameter set to 1 => write
    }

    // Read the contents of the Current Process’s backing store/flash device logical page p into frame i 
    rw_backing_store(CURRENT_PROC[getPRID()]->p_pid, p, victim_addr, 0); // 4th parameter set to 1 => read

    //Update the Swap Pool table’s entry i to reflect frame i’s new contents
    SWAP_POOL[i].sw_asid = CURRENT_PROC[getPRID()]->p_pid;
    SWAP_POOL[i].sw_pte = CURRENT_PROC[getPRID()]->p_supportStruct->sup_privatePgTbl[p].pte_entryHI;
    
    //Update the Current Process’s Page Table entry for page p to indicate it is now present (V bit)
    CURRENT_PROC[getPRID()]->p_supportStruct->sup_privatePgTbl[p].pte_entryLO |= VALIDON;
    //DA RIGUARDARE SOTTO
    
    // Update the Current Process’s Page Table entry for page p to indicate it is occupying frame i (PFN field).
    CURRENT_PROC[getPRID()]->p_supportStruct->sup_privatePgTbl[p].pte_entryLO |= (victim_addr << ENTRYLO_PFN_BIT);

    // Update the TLB
    update_tlb(CURRENT_PROC[getPRID()]->p_supportStruct->sup_privatePgTbl[p]);

    // Release mutual exclusion over the Swap Pool table
    SYSCALL(VERHOGEN, (int)&SWAP_POOL_SEMAPHORE, 0, 0);
    // Return control to the Current Process to retry the instruction that caused the page fault
    LDST(&saved_exception_state);
}
