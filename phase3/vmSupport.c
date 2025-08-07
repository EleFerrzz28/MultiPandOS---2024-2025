#include "vmSupport.h"

/*Declaring the structures - module wide*/
unsigned int SWAP_POOL_SEMAPHORE;
unsigned int MASTER_SEMAPHORE;
pteEntry_t PAGE_TABLE[32];
swap_t SWAP_POOL[POOLSIZE];
volatile unsigned int U_DEVICE_SEMAPHORES[3*UPROCMAX];

void update_tlb(pteEntry_t page_table_entry){
    setENTRYHI(page_table_entry.pte_entryHI);
    TLBP();
    if(getINDEX() != PRESENTFLAG){//check if the page is loaded in the TLB
        setENTRYLO(page_table_entry.pte_entryLO);
        TLBWI();
    }
}

int get_victim_frame(){
    for (int i = 0; i < POOLSIZE; i++){
        if (SWAP_POOL[i].sw_asid == NOPROC) // -1 = free page
        return i;
    }

    // se non c'e' una pagina libera, use un algoritmo FIFO per trovare una pagina
    // da rimpiazzare
    static int i = -1;
    return i = (i + 1) % POOLSIZE;
}

void clean_page(pteEntry_t* page_table_entry){
    page_table_entry->pte_entryLO = ~VALIDON;
    update_tlb(*page_table_entry);
}

void rw_backing_store(int asid, int page_no){
    dtpreg_t *flash_device_reg = (dtpreg_t *)DEV_REG_ADDR(IL_FLASH, asid - 1);
    flash_device_reg->data0 = (memaddr*)page_no;
    //SEZIONE 9.c + 10 del pager
}

void uTLB_RefillHandler(){
    //Determine the page number(p) of the missing TLB entry
    state_t *saved_exception_state = GET_EXCEPTION_STATE_PTR(getPRID());
    int p = ENTRYHI_GET_VPN(saved_exception_state->entry_hi);

    //get the Page Table entry 
    pteEntry_t page_table_entry = CURRENT_PROC[getPRID()]->p_supportStruct->sup_privatePgTbl[p];

    //write the page table entry into the TLB
    setENTRYHI(page_table_entry.pte_entryHI);
    setENTRYLO(page_table_entry.pte_entryLO);
    TLBWR();

    //return control to the Current Process to retry the instruction that caused the TLB-Refill event
    //riguardare: saved_exception_state->pc_epc += WORDLEN;
    LDST(&saved_exception_state);
}

void pager(){
    //Obtain the pointer to the Current Process’s Support Structure
    support_t* curr_proc_sup_structure = SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *saved_exception_state = &curr_proc_sup_structure->sup_exceptState[0];

    unsigned int CAUSE = saved_exception_state->cause;
    if(CAUSE == EXC_MOD){//the cause is a TLB-Modification exception
        SYSCALL(TERMINATE, 0, 0, 0); //DA RIGUARDARE
    }

    //Gain mutual exclusion over the Swap Pool table
    SYSCALL(PASSEREN, (int)&SWAP_POOL_SEMAPHORE, 0, 0);

    //Determine the missing page number
    int p = ENTRYHI_GET_VPN(saved_exception_state->entry_hi);

    pteEntry_t page_table_entry = curr_proc_sup_structure->sup_privatePgTbl[p];

    if(SWAP_POOL[p].sw_asid!=NOPROC){//Check if the page is loaded in the swap pool
        update_tlb(page_table_entry);
        if(page_table_entry.pte_entryLO & VALIDON){
            SYSCALL(VERHOGEN, (int)&SWAP_POOL_SEMAPHORE, 0, 0);
            LDST(&saved_exception_state);
        }
    }

    //Pick a frame, i, from the Swap Pool.
    int i = get_victim_frame();
    if (SWAP_POOL[i].sw_asid != NOPROC){
        clean_page(SWAP_POOL[i].sw_pte);
        rw_backing_store(SWAP_POOL[i].sw_asid, SWAP_POOL[i].sw_pageNo);
    }

    //Update the Swap Pool table’s entry i to reflect frame i’s new contents
    SWAP_POOL[i].sw_asid = CURRENT_PROC[getPRID()]->p_pid;
    SWAP_POOL[i].sw_pte = CURRENT_PROC[getPRID()]->p_supportStruct->sup_privatePgTbl[p].pte_entryHI;
    //Update the Current Process’s Page Table entry for page p to indicate it is now present (V bit)
    CURRENT_PROC[getPRID()]->p_supportStruct->sup_privatePgTbl[p].pte_entryLO |= VALIDON;
    //DA RIGUARDARE SOTTO
    // calcolo l'indirizzo della pagina tenendo conto dell'offset SWAP_POOL_AREA
    // e ogni singola pagina fino alla i-esima
    int SWAP_POOL_START = RAMSTART + (64 * PAGESIZE) + (NCPU * PAGESIZE);
    memaddr victim_addr = (memaddr)SWAP_POOL_START + (i * PAGESIZE);
    CURRENT_PROC[getPRID()]->p_supportStruct->sup_privatePgTbl[p].pte_entryLO |= (victim_addr);
    // Uso il mio algoritmo di rimpiazzamento per trovare la pagina da
    // sostituire
    int i = getPage(); // pagina vittima

    //Update the TLB
    update_tlb(CURRENT_PROC[getPRID()]->p_supportStruct->sup_privatePgTbl[p]);

    SYSCALL(VERHOGEN, (int)&SWAP_POOL_SEMAPHORE, 0, 0);
    LDST(&saved_exception_state);
}
