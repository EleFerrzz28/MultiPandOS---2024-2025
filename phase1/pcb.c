#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

/*
Initialize the pcbFree list to contain all
the elements of the static array of MAXPROC PCBs. This
method will be called only once during data structure
initialization.
*/
void initPcbs()
{
    INIT_LIST_HEAD(&pcbFree_h);
    for (int i = 0; i < MAXPROC; i++)
    {
        list_add_tail(&pcbFree_table[i].p_list, &pcbFree_h);
    }
}

/*Insert the element pointed to by
p onto the pcbFree list.*/
void freePcb(pcb_t *p)
{
    p->p_pid = -1; // se l'ho ucciso e un altro vuole ucciderlo non lo trova
    list_del(&p->p_child);
    list_del(&p->p_sib);
    list_del(&p->p_list);
    list_add_tail(&p->p_list, &pcbFree_h);
}

/* return NULL if the pcbFree list is empty. Otherwise, remove an element from the pcbFree list,
provide initial values for ALL of the PCBs fields and then return a pointer to the removed element.
PCBs get reused, so it is important that no previous value persist in a PCB when it
gets reallocated.*/
pcb_t *allocPcb()
{
    if (list_empty(&pcbFree_h))
    {
        return NULL;
    }
    else
    {
        pcb_t *ptr;
        ptr = container_of(pcbFree_h.next, pcb_t, p_list);
        list_del(pcbFree_h.next);
        // inizializzazione campi
        mkEmptyProcQ(&ptr->p_child);
        ptr->p_parent = NULL;
        ptr->p_pid = next_pid;
        next_pid++;
        ptr->p_semAdd = 0;
        mkEmptyProcQ(&ptr->p_sib);
        ptr->p_supportStruct = NULL;
        ptr->p_time = 0;
        return ptr;
    }
}

/*this method is used to initialize a variable to be head pointer to a
process queue.*/
void mkEmptyProcQ(struct list_head *head)
{
    INIT_LIST_HEAD(head);
}

/*return TRUE if the queue whose head is pointed to by head is empty.
Return FALSE otherwise.*/
int emptyProcQ(struct list_head *head)
{
    return list_empty(head);
}

/*Insert the PCB pointed by p into the process queue
whose head pointer is pointed to by head.*/
void insertProcQ(struct list_head *head, pcb_t *p)
{
    list_add_tail(&p->p_list, head); // aggiunta in coda alla processqueue puntata da head il processo p in questione
}

/*return a pointer to the first PCB from the process queue whose head is
pointed to by head. Do not remove this PCB from the
process queue. Return NULL if the process queue is empty.*/
pcb_t *headProcQ(struct list_head *head)
{
    return container_of(head->next, pcb_t, p_list);
}

/*remove the first (i.e. head) element from the process queue whose
head pointer is pointed to by head. Return NULL if the
process queue was initially empty; otherwise return the pointer
to the removed element.*/
pcb_t *removeProcQ(struct list_head *head)
{
    if (list_next(head) == NULL)
    {
        return NULL;
    }
    pcb_t *tmp = container_of(list_next(head), pcb_t, p_list);
    list_del(&tmp->p_list);
    return tmp;
}

/*remove the PCB pointed to by p from the process queue
whose head pointer is pointed to by head. If the desired entry
is not in the indicated queue (an error condition), return
NULL; otherwise, return p. Note that p can point to any
element of the process queue.*/
pcb_t *outProcQ(struct list_head *head, pcb_t *p)
{
    struct list_head *iter;
    list_for_each(iter, head)
    {
        pcb_t *item = container_of(iter, pcb_t, p_list);
        if (&item->p_list == &p->p_list)
        {
            list_del(&p->p_list);
            return p;
        }
    }
    return NULL;
}

/*return TRUE if the PCB pointed to by p has no children. Return FALSE otherwise*/
int emptyChild(pcb_t *p)
{
    return list_empty(&p->p_child);
}

/*make the PCB pointed to by p a child of the PCB pointed to by prnt.*/
void insertChild(pcb_t *prnt, pcb_t *p)
{
    p->p_parent = prnt;
    list_add_tail(&p->p_sib, &prnt->p_child);
}

/*make the first child of the PCB pointed to by p no longer a child of p. Return NULL if
initially there were no children of p. Otherwise, return a pointer to this removed first child PCB.*/
pcb_t *removeChild(pcb_t *p)
{
    if (emptyChild(p))
        return NULL;
    pcb_t *tmp = container_of(list_next(&p->p_child), pcb_t, p_sib);
    list_del(&tmp->p_sib);
    return tmp;
}

/*make the PCB pointed to by p no longer the child of its parent. If the PCB pointed to by p
has no parent, return NULL; otherwise, return p. Note that the element pointed to by p could
be in an arbitrary position (i.e. not be the first child of its parent).*/
pcb_t *outChild(pcb_t *p)
{
    // se non ha un padre ritorno NULL
    if (p->p_parent == NULL)
    {
        return NULL;
    }
    else
    {
        p->p_parent = NULL;
        // rimuove p modificando già i suoi valori p_sib.next e p_sib.prev
        list_del(&p->p_sib);
        return p;
    }
}

pcb_t *GetPCBbyPid(int pid)
{
    for (int i = 0; i < MAXPROC; i++)
    {
        if (pcbFree_table[i].p_pid == pid)
        {
            return &pcbFree_table[i];
        }
    }
    return NULL;
}