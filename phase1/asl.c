#include "./headers/asl.h"
#include "./headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

/*initialize the semdFree list to contain all the elements of the array
static semd_t semdTable[MAXPROC]. This method will be
only called once during data structure initialization.*/
void initASL()
{
    INIT_LIST_HEAD(&semdFree_h);
    INIT_LIST_HEAD(&semd_h);
    for (int i = 0; i < MAXPROC; i++)
    {
        INIT_LIST_HEAD(&semd_table[i].s_procq);
        list_add_tail(&semd_table[i].s_link, &semdFree_h);
    }
}

/*
   Funzione ausiliaria che, dato un id numerico, restituisce un puntatore
   al semaforo corrispondente:

   Parameters:
   semAdd: intero numerico dato in input
   head: puntatore alla lista contenente il semaforo cercato (una tra semdFree_h e semd_h)
*/
static semd_t *searchSemaphore(int *semAdd, struct list_head *head)
{
    struct semd_t *iter;
    list_for_each_entry(iter, head, s_link)
    {
        if (iter->s_key == semAdd)
            return iter;
    }
    return NULL;
}

/*
   Funzione ausiliaria che alloca un nuovo descrittore di semafori
   dalla semdFree_h list; nel caso in cui la lista sia vuota,
   ritorna NULL; altrimenti, ritorna il nuovo semaforo
*/
static semd_t *allocSemd()
{
    if (list_empty(&semdFree_h))
        return NULL;
    else
    {
        semd_t *temp;
        temp = container_of(semdFree_h.next, semd_t, s_link);
        list_del(semdFree_h.next);
        return temp;
    }
}

/*Insert the PCB pointed to by p at the tail of the process queue
associated with the semaphore whose key is semAdd and set
the semaphore address of p to semaphore with semAdd. If the
semaphore is currently not active, allocate a new descriptor
from the semdFree list, insert it in the ASL (at the
appropriate position), initialize all of the fields, and proceed as
above. If a new semaphore descriptor needs to be allocated
and the semdFree list is empty, return TRUE. In all other
cases return FALSE.*/
int insertBlocked(int *semAdd, pcb_t *p)
{
    semd_t *sem = searchSemaphore(semAdd, &semd_h);
    if (sem == NULL)
    { // il semaforo non esiste nella ASL
        semd_t *target = allocSemd();
        if (target == NULL)
            return TRUE;

        target->s_key = semAdd; // chiave del nuovo semaforo
        p->p_semAdd = semAdd;   // puntatore di p al nuovo semaforo
        mkEmptyProcQ(&(target->s_procq));
        insertProcQ(&(target->s_procq), p);
        // inserire target in ASL nella giusta posizione
        list_add_tail(&target->s_link, &semd_h);
    }

    else
    {                         // il semaforo esisteva già
        sem->s_key = semAdd;  // chiave del nuovo semaforo
        p->p_semAdd = semAdd; // puntatore di p al nuovo semaforo
        insertProcQ(&(sem->s_procq), p);
    }
    return FALSE;
}

/*search the ASL for a descriptor of this semaphore. If none is found, return NULL;
otherwise, remove the first PCB from the process queue of the
found semaphore descriptor and return a pointer to it. If the
process queue for this semaphore becomes empty, remove the
semaphore descriptor from the ASL and return it to the semdFree list*/
pcb_t *removeBlocked(int *semAdd)
{
    semd_t *sem = searchSemaphore(semAdd, &semd_h);
    if (sem == NULL)
        return NULL;
    else
    {
        pcb_t *pcb = removeProcQ(&sem->s_procq);
        // removeProcQ, oltre a rimuovere l'elemento, ritorna anche il suo puntatore
        if (emptyProcQ(&sem->s_procq))
        { // verifichiamo se in seguito alla rimozione la procQ diventa vuota
            list_del(&sem->s_link);
            list_add_tail(&sem->s_link, &semdFree_h);
        }
        return pcb;
    }
}

/*
pcb_t* outBlockedPid(int pid) {
}
*/

/*remove the PCB pointed to by p from the process queue associated with p’s semaphore
on the ASL. If PCB pointed to by p does not appear in the
process queue associated with p’s semaphore, which is an
error condition, return NULL; otherwise, return p.
*/
pcb_t *outBlocked(pcb_t *p)
{
    semd_t *sem = searchSemaphore(p->p_semAdd, &semd_h);
    if (sem == NULL)
        return NULL;
    else
        return outProcQ(&sem->s_procq, p); // ritorna il puntatore all'elemento
}

/*return a pointer to the PCB that is at the head of the process queue associated
with the semaphore semAdd. Return NULL if semAdd is not
found on the ASL or if the process queue associated with semAdd is empty*/
pcb_t *headBlocked(int *semAdd)
{
    semd_t *sem = searchSemaphore(semAdd, &semd_h);
    // se non è stato trovato il semaforo o la process queue associata è vuota ritorno NULL:
    if (sem == NULL || list_empty(&sem->s_procq))
        return NULL;
    else
        return container_of(list_next(&sem->s_procq), pcb_t, p_list);
}
