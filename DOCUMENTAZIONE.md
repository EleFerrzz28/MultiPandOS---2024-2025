# Fase 2 - Livello 3: il Nucleo

In Fase 2, ovvero, dopo esserci occupati della costruzione delle procedure necessarie per la corretta gestione delle strutture dati relative ai PCB e agli ASL, si entra in contatto con la prima vera e propria gestione del sistema operativo. Ora, prima di scendere nei particolari, occorre sottolineare che MultiPandos è un sistema operativo multiprocessore: ciò significa che gli accessi alle risorse condivise - come ad esempio, la lista dei processi in stato ready - da parte dei processori dovranno essere effettuati in mutua esclusione onde evitare di incorrere in *race condition*. Dopo questa premessa essenziale per il funzionamento dell'intero sistema, è possibile cominciare a parlare del Nucleo. In particolare, le funzionalità di quest'ultimo possono essere divise in cinque categorie:
- Inizializzazione del Nucleo;
- Lo Scheduler;
- Gestione delle eccezioni e delle syscalls;
- Gestione degli interrupts;
- Passare a un livello più alto la gestione di determinati eventi.
In Fase 2 ci occuperemo dei primi 4 punti, i quali sono stati suddivisi e gestiti all'interno dei file:
- initial.c;
- scheduler.c;
- exceptions.c;
- interrupts.c.
Di seguito si trova la descrizione di ciascuno dei file.

# initial.c

initial.c è il file che contiene l’inizializzazione del Nucleo. Si apre con la dichiarazione di alcune variabili globali:
- `PROC_COUNT`, un intero che mantiene in memoria la quantità di processi in attesa di terminare.
- `READY_Q`, il puntatore alla testa della coda di processi che sono nello stato “ready”.
- `CURRENT_PROC`, un array di puntatori ai processi attivi in ogni CPU (l’isemo puntatore è legato all’iesima CPU).
- `GLOBAL_LOCK_value` e `GLOBAL_LOCK`, rispettivamente il semaforo binario usato per le sincronizzazioni globali ed il suo puntatore a GLOBAL_LOCK_value.
- `DEVICE_SEMAPHORES`, un array di semafori, uno per ogni device non terminale, due per ogni device terminale, in quanto composto da due sub-device, più uno per supportare lo pseudo_clock - per un totale di 49 semafori. 


Le funzioni all’interno del file sono:

## `pcb_t* initFirstProcess()`:
Istanzia un singolo processo e lo configura come il primo processo utente nel sistema.
Alloca un nuovo Pcb e ne definisce i valori base.
Alloca un puntatore a test nel program counter (pc_epc).
Imposta i registri di stato e puntatore di stack.
Inserisce il processo nella READY_Q, incrementa  PROC_COUNT e restituisce first process, ovvero il pcb allocato inizialmente.

## `void configureIRTEntries(__UINT32_TYPE__ *irtBase)`:
Prende come input la tabella delle entrate degli interrupt di routing e pone il bit RP e il bit meno significativo della NCPU ad 1.
Quando il valore del bit RP è ad uno, l’interrupt viene mandato ad uno dei processori indicati nel campo Destination, dove il bit i di destinazione corrisponde all’ID del processore i, che sta eseguendo la task con la minore importanza.

## `void main()` : 
Viene innanzitutto popolato il passupvector di ogni processore, assegnando i puntatori agli stack e impostando gli handler per refill TLB e la gestione delle eccezioni - facendo una distinzione, almeno per quanto riguarda `passupvector[i].exception_stackPtr` e `passupvector[i].tlb_refill_stackPtr`, in quanto la CPU ad indice 0 segue un'inizializzazione diversa rispetto alle altre CPU.
Le funzioni `initPCBs` e `initASL` vengono richiamate per inizializzare rispettivamente la lista dei pcb liberi e la lista dei semafori attivi.
`PROC_COUNT`, `READY_Q` e `CURRENT_PROC` vengono posti a 0 o NULL, poichè non ci sono processi durante l’inizializzazione del nucleo, quindi GLOBAL_LOCK e tutti i semafori di CURRENT_PROC vengono posti a 0 poiché liberi.
`LDIT` imposta il timer per generare input periodici ogni 100 ms.
`initFirstProcess()` crea e mette in coda il processo iniziale.
`configureIRTentries()` abilita il routing degli interrupt a tutte le CPU.
impostare TRP a zero permette l’accettazione di tutti gli interrupt.
Inizializza i valori di cpu_state, per poi iniziare l’esecuzione con INITCPU.
Infine viene effettuata una chiamata allo scheduler. 


# scheduler.c

Lo scheduler è il componente che si occupa di garantire una buona sincronizzazione che consenta l'esecuzione di tutti i processi nello stato ready.
Lo implementiamo attraverso una funzione omonima che come prima cosa acquisisce il global lock così da poter accedere alla READY_Q in quanto variabile condivisa.
Successivamente lo scheduler si occupa di verificare che la READY_Q non sia vuota: in tal caso lavoriamo sul processo che aspetta da più tempo in coda.
Quindi incrementiamo il PLT di 5 millisecondi, aggiorniamo il campo p_time del processo, lo mandiamo in esecuzione e impostiamo il registro TPR a 0.

## GESTIONE DEL TEMPO:
Il meccanismo che abbiamo usato per la gestione del campo `p_time` di ciascun processo evita l'uso di variabili globali e viene attuato nel seguente modo:

Ipotizziamo, nel momento in cui lo scheduler viene chiamato, di avere un tempo attuale della CPU(`current_time`) pari a 1000 e un `p_time` di 200.
Ciò significa che, precedentemente, il processo ha accumulato 200 ms di esecuzione sul processore su cui ci troviamo.

All'interno dello scheduler andremo ad eseguire il comando:
`current_process->p_time -= current_time`
e otterremo `current_process->p_time= 200 - 1000 = -800`

Intanto il processo verrà eseguito, ipotizziamo per altri 200 ms, e quando si bloccherà per una qualche eccezione verrà eseguito il comando 
`saved_exception_state->reg_a0 = CURRENT_PROC[getPRID()]->p_time + current_time`
A quel punto avremo:
`current_time = 1200;`
`CURRENT_PROC[getPRID()]->p_time=-800`

`saved_exception_state->reg_a0= -800 + 1200 = 400;`

400 è esattamente il tempo di esecuzione del processo che si è accumulato sul processore.

Nel caso in cui la `READY_Q`, al contrario, fosse vuota, dobbiamo verificare se ci sono processi attivi o meno.
Se ci sono processi attivi invochiamo l'istruzione `HALT()` che stampa sul terminale il messaggio "System Halted" e costringe il processore ad entrare in un loop infinito.
Se non ci sono processi attivi allora impostiamo il TPR a 1 e modifichiamo alcuni registri in grado di abilitare interrupts e disabilitare il PLT.
Il TPR è un registro utilizzato per memorizzare la priorità delle istruzioni in esecuzione su ogni processo. Questo registro può assumere solo i valori 0 e 1, che rappresentano, rispettivamente, il livello più basso e più alto di priorità.
Dopo la modifica di quest'ultimo e altri registri il processore entra in stato Wait(): esso non esegue istruzioni ma aspetta che si verifichi l'interrupt di qualche device.

# exceptions.c

Nel modulo exceptions.c vengono implementate le funzioni che gestiscono le eccezioni e in particolare quelle che gestiscono le SYSCALL.

Le eccezioni possono essere di vario tipo in base alla loro causa, infatti abbiamo: interrupts, eccezioni TLB, SYSCALL e Program Traps.


## SYSCALL
Per convenzione nel nostro progetto, un processo che intende eseguire una syscall come prima operazione memorizza dei valori appropriati nei registri `a0-a3`. Successivamente, in base a quei valori,  i Nuclei eseguono precise istruzioni, in virtù del processo che ha chiamato la syscall. 

Le SYSCALL da noi gestite sono:
`CreateProcess` (NSYS1)
`TerminateProcess` (NSYS2)
`Passeren (P)` (NSYS3)
`Verhogen (V)` (NSYS4)
`DoIO` (NSYS5)
`GetCPUTime` (NSYS6)
`WaitForClock` (NSYS7)
`GetSupportData` (NSYS8)
`GetProcessID` (NSYS9)

Ad ognuna di esse è associato un valore intero negativo ( in ordine da -1 a -9 )  che viene posizionato dal processo chiamante nel registro `a0`. È bene precisare che le syscall  elencate qui sopra sono considerate servizi privilegiati, pertanto possono essere chiamate da un processo solo se quest'ultimo si trova in Kernel Mode; nel caso in cui un processo in User Mode voglia eseguire una syscall,  il controllo viene passato al `PROGRAM_TRAP_EXCEPTION_HANDLER`.


Analizziamo passo passo le funzioni in questo modulo.

## `void exceptionHandler()`
A seguito di un’ eccezione, questa funzione è la prima ad attivarsi; durante l’esecuzione, attraverso la funzione `getCAUSE()` la causa dell’eccezione (espressa come un intero senza segno) viene riconosciuta e salvata all’interno di una variabile `exception_code`, in seguito una serie di rami `IF ELSE` passa il controllo alla funzione corrispondente alla causa dell’eccezione. 
Quale codice corrisponde a quale tipo di eccezione è descritto nelle specifiche di fase 2 del progetto.
Nel momento in cui l'`exceptionHandler` cede il controllo, passa anche come parametro alla prossima funzione `previous_state`, ovvero lo stato del processo in esecuzione, prima che avvenisse l’eccezione.

## `void PASS_UP_OR_DIE(int except_type, state_t *previous_state)`
Nel caso in cui `exception_code` sia un intero compreso tra 24 e 28 ( inclusi ) allora il controllo dell’esecuzione viene passato a `PASS_UP_OR_DIE`. 
A questa funzione, oltre a `previous_state`, viene passato anche come parametro `except_type`, che ha come valore `PGFAULTEXCEPT` per eccezioni TLB oppure `GENERALEXCEPT` per le altre eccezioni.
A seconda del fatto che il campo `p_supportStruct` del processo corrente sia `NULL` oppure no, questa funzione si comporta in due maniere differenti.
Nel primo caso il processo deve essere “terminato” : viene chiamata la funzione ausiliaria `KILL_PROCESS` sul processo corrente e subito dopo lo `scheduler()`.
Nel secondo caso la gestione dell’eccezione deve essere “passata avanti” :per fare ciò, innanzitutto lo stato dell’eccezione salvato dal BIOS, ovvero `previous_state`, viene copiato nel campo `sup_exceptState[except_type]` della struttura di supporto del processo corrente; dopodichè  il controllo viene trasferito all’”handler” del livello supporto specificato dal processo.
In pratica quello che facciamo è permettere al processo di gestire autonomamente l’eccezione, sfruttando il suo `support_t` , se fornito.

## `void PROGRAM_TRAP_EXCEPTION_HANDLER(state_t *saved_exception_state)`
Una eccezione Program Trap si verifica quando il processo corrente cerca di compiere azioni illegali oppure indefinite. In questo caso viene semplicemente chiamato `PASS_UP_OR_DIE(GENERALEXCEPT, saved_exception_state)`, per gestire propriamente l’eccezione.

## `void SYSCALL_EXCEPTION_HANDLER(state_t *saved_exception_state)`
Nel caso in cui `exception_code` sia uguale a 8 oppure ad 11 allora il controllo dell’esecuzione viene passato a `SYSCALL_EXCEPTION_HANDLER`, ovvero la funzione che ha il compito di riconoscere l’arrivo di una  syscall da parte di un processo e di gestirla di conseguenza.
Per fare ciò viene innanzitutto verificato che il processo sia effettivamente abilitato per compiere una syscall (quindi è in `KernelMode`) e se la risposta è affermativa allora utilizziamo un costrutto `SWITCH`, che controlla il valore del registro `a0` di `saved_exception_state`, per passare la gestione effettiva della syscall alla sua funzione corrispondente (può essere `CreateProcess`,`TerminateProcess`,`Passeren` ecc.); al contrario se il processo corrente non è in `KernelMode` allora si forza il valore  `PRIVINSTR` (Privileged Instruction)
 nel campo `cause` dello `state_t`, per segnalare l'eccezione come privilegiata.

## `void CREATE_PROCESS(state_t *saved_exception_state)`
Se il valore in `a0` è `-1` allora la syscall è  `CREATE_PROCESS`. La `CREATE_PROCESS` crea un nuovo processo figlio del processo corrente, inizializzando il suo Pcb e mettendolo nella ready queue, così che lo scheduler possa successivamente eseguirlo.
Al termine di questa syscall il controllo viene ritornato al processo corrente attraverso la funzione `LDST(saved_exception_state)`.

## `void TERMINATE_PROCESS(state_t *saved_exception_state)`
Se il valore in `a0` è `-2` allora la syscall  è  `TERMINATE_PROCESS`. La `TERMINATE_PROCESS` termina un processo e tutti i suoi figli grazie a una funzione ausiliaria, ovvero `KILL_PROCESS`. Questa syscall si struttura in 2 rami in base al valore nel registro `a1` di `saved_exception_state`: se il valore dentro tale registro è `1` allora il processo corrente chiede di essere terminato e quindi viene chiamata `KILL_PROCESS(CURRENT_PROC[getPRID()])`; altrimenti dentro il registro `a1` sappiamo che sarà presente l'identificativo (il PID) del processo da terminare.
Per trovare questo processo tra tutti quelli attivi abbiamo implementato una funzione ausiliaria, ovvero `GetPCBbyPid`, che dato un PID restituisce il processo avente quel PID.
Al termine della syscall verifichiamo che ci sia effettivamente ancora un processo in esecuzione: se c’è allora il controllo viene ritornato a quest’ultimo, altrimenti viene chiamato lo `scheduler()`.

## `pcb_t *GetPCBbyPid(int pid)`
Per facilitare la ricerca di un PCB dato il suo PID abbiamo scelto di implementare la funzione ausiliaria `GetPCBbyPid` nel file pcb.c di phase 1 . Nello specifico questa funzione, attraverso un ciclo `FOR` (con  `i<MAXPROC`),  controlla l’identificativo del PCB di indice `i` nella `pcbFree_table` ; se `p_pid` di tale processo è uguale al `pid` passato come parametro a `GetPCBbyPid`,  allora viene restituito un puntatore a tale processo; `NULL` se nessun processo corrisponde a quel pid.

## `void KILL_PROCESS(pcb_t *process)`
Questa funzione ausiliaria è stata implementata per gestire la terminazione di un processo e di tutti i suoi figli; nel concreto esegue una rimozione ricorsiva di un albero di processi. Per fare ciò questa funzione rimuove uno alla volta i figli del processo dato e per ognuno di essi chiama `KILL_PROCESS`, finchè non arriva all’ultimo dei figli. 
Nel concreto per terminare un processo, come primo passaggio controlliamo  attraverso un ciclo `FOR` su `CURRENT_PROC` se il processo da terminare è attivo; se è così allora lo rimuoviamo dall’array (impostando `CURRENT_PROC[i]` a `NULL`). 
Dopodichè chiamiamo `freePcb` su `process` : questa chiamata marca il processo come "morto" impostando il suo `p_pid` a `-1` (così facendo non c’è la possibilità che un altro processo tenti di terminare un processo già “morto”), rimuove il PCB da tutte le liste in cui potrebbe trovarsi (`p_child`, `p_sib`, `p_list`) e infine inserisce il PCB terminato nella lista dei PCB liberi. 
Come ultima operazione decrementiamo `PROC_COUNT` di 1.

## `int getSemaphoreDeviceIndex(memaddr *commandAddr)`
Funzione ausiliaria, implementata per calcolare l’indice corretto in `DEVICE_SEMAPHORES` del semaforo associato ad un determinato dispositivo I/O. 
È bene ricordare che all’interno del nostro OS, i dispositivi si dividono in due categorie:
- Non terminali
- Terminali (composti da due sotto-dispositivi: trasmettitore e ricevitore)

Ogni dispositivo ha un’area riservata in memoria che occupa rispettivamente 16 byte per un non terminale e 8 byte per ciascuna metà di un terminale (quindi in totale 16 byte, ma le due parti sono gestite come dispositivi indipendenti).
I parametri noti a questa funzione sono: un puntatore alla cella di memoria del campo "command" del registro del dispositivo (`commandAddr`), l’indirizzo d'inizio  in memoria dei registri dei non terminali (`0x10000054`) e l’indirizzo d'inizio  in memoria dei registri dei terminali (`0x10000254`).
Per trovare il semaforo associato al dispositivo, come primo passaggio decrementiamo `commandAddr` di un word (4 byte) così da ottenere l’indirizzo base di quel dispositivo. Dopodiché tramite una ramificazione `IF ELSE` controlliamo se il dispositivo in questione è o non è un terminale. 
Se l’indirizzo del dispositivo è prima dell’inizio dei terminali (quindi `devAddrBase < terminalReg`), allora è un non terminale. In tal caso calcoliamo l’offset in byte rispetto alla base dei dispositivi non terminali e dividiamo il risultato per `0x10` (cioè 16 in hex), perché sappiamo che ogni registro di dispositivo non terminale è lungo 16 byte. Così facendo otteniamo l'indice corretto nel range `0-31` (per i primi 32 dispositivi non terminali).
Nell’altro caso il dispositivo è un terminale. Di conseguenza calcoliamo come prima l’offset in byte rispetto alla base dei terminali ma dividiamo il risultato per `0x8` (cioè 8 in hex ), perché ogni sottodispositivo (trasmettitore/ricevitore) ha un registro di 8 byte. Infine a questo intero trovato viene sommato 32, perché i primi 32 semafori (0–31) sono già usati dai dispositivi non terminali; in questo modo otteniamo un indice nel range `32-47` per i dispositivi terminali (l’ultimo indice, ovvero il 48esimo, è riservato al semaforo dello Pseudo-clock).

Il valore dell’indice del semaforo in `DEVICE_SEMAPHORES` associato al dispositivo,   viene ritornato come `int` al termine della funzione.

## `void SEM_PASSEREN(state_t *saved_exception_state)`
Se il valore in `a0` è `-3` allora la syscall è `SEM_PASSAREN` ovvero la funzione che implementa l'operazione "wait" (P) su un semaforo (binario). Il semaforo su cui eseguire la P è salvato nel registro `a1` del processo chiamante. Prima di svolgere qualsiasi operazione sul semaforo è necessario controllarne il valore: se il semaforo è a `0` allora non si può procedere con l'operazione, quindi il processo che ha chiamato la P va bloccato; al contrario se è a `1` allora il semaforo può essere “preso” dal processo corrente.
Nel caso in cui il processo corrente debba essere bloccato, viene inserito nella coda dei processi bloccati associata al semaforo, il suo stato viene salvato nel campo `p_s`, il tempo CPU (`p_time`) accumulato dal processo viene aggiornato e `CURRENT_PROC[getPRID()]` viene impostato a `NULL`. Infine viene chiamato lo `scheduler()`.
Nel caso contrario si effettua l’operazione di decremento del semaforo (P), impostandolo a 0. Tuttavia nel caso in cui ci siano altri processi in attesa su quel semaforo, la situazione cambia. Infatti se `headBlocked(sem) != NULL` , allora il processo corrente deve sbloccare il primo dei processi bloccati, perché il semaforo potrà  essere considerato "libero per lui" dopo l’uscita di quello corrente. Inoltre proprio perché il processo sbloccato non è ancora stato eseguito, dobbiamo lasciare il valore del semaforo (`sem`) a 1,  per permettergli di eseguire `PASSAREN`. In altre parole l’istruzione `*sem = 1` serve a garantire che anche un processo appena sbloccato possa eseguire la P con successo. 
Come ultima operazione ripristiniamo lo stato del processo corrente, facendolo ripartire come se la syscall fosse terminata normalmente.

## `void SEM_VERHOGEN(state_t *saved_exception_state)`
Se il valore in `a0` è `-4` allora la syscall è `SEM_VERHOGEN` ovvero la funzione che implementa l'operazione "signal" (V) su un semaforo (binario). Questa syscall ha una struttura molto simile alla `PASSAREN` ma presenta delle differenze cruciali.
Innanzitutto, prima di effettuare una V su un semaforo si controlla se questo è già rilasciato (ovvero `*sem=1`); se è così allora il processo corrente viene bloccato. Se questo non è il caso allora il semaforo viene incrementato e come nella `PASSAREN`, controlliamo se ci sono dei processi bloccati su di esso, per assicurarci che quest’ultimi possano fare una V con successo.

## `void DO_IO(state_t *saved_exception_state)`
Se il valore in `a0` è `-5` allora la syscall è`DO_IO` (Do Input Output); Tale funzione serve per inviare un comando a un dispositivo e poi bloccare l'esecuzione fino a quando il dispositivo non segnala la fine dell’operazione con un interrupt. 	Inizialmente, in questa syscall, viene recuperato l’indirizzo del campo “command” del dispositivo (salvato nel registro `a1`) per poi calcolare l’indice in `DEVICE_SEMAPHORES` di quel dispositivo tramite la funzione ausiliaria . Successivamente si incrementa il valore del semaforo per quel dispositivo e si inserisce il processo corrente nella coda dei processi bloccati su quel semaforo, in attesa che il dispositivo completi l’operazione; è opportuno notare come l'istruzione `(DEVICE_SEMAPHORES[semIndex])++` non esegue un’operazione di tipo semaforico (quindi P o V), ma serve, invece,  solo a indicare che c’è un processo in attesa su quel dispositivo. Sarà poi compito del `device_interrupt_handler` segnalare l’arrivo di un interrupt e di conseguenza decrementare il valore di `DEVICE_SEMAPHORES[semIndex]`.
Proseguendo, viene salvato lo stato aggiornato del processo in `CURRENT_PROC[getPRID()]->p_s` e si aggiorna il tempo di CPU accumulato dal processo, sommando il tempo attuale. Infine viene recuperato il valore del comando da inviare al dispositivo (memorizzato nel registro `a2`) e lo si scrive nel campo “command” del dispositivo.

## `void GetCPUTime(state_t *saved_exception_state)`
Se il valore in `a0` è `-6` allora la syscall è `GetCPUTime`; questa syscall è richiesta per aggiornare il campo `p_time` del processo corrente (ovvero il tempo impiegato dal processo per tutte le sue esecuzioni accumulate) e restituirlo all’interno del registro a0. L’aggiornamento di `p_time` è compatibile con il meccanismo di gestione del tempo descritto nella sezione relativa allo scheduler.

## `void WaitForClock(state_t *saved_exception_state)`
Se il valore in `a0` è `-7` allora la syscall è `WaitForClock`; questa syscall viene richiesta da un processo che desidera essere messo in attesa del prossimo "tick" dello pseudo-clock: di conseguenza, ciò porterà il processo a transitare dallo stato "running" allo stato "blocked". All'interno della funzione eseguiamo una P sul semaforo dello Pseudo-clock e blocchiamo su di esso il processo che ha richiesto la syscall.  Dopodichè andiamo anche a gestire il campo `p_time` del processo incrementandolo con il tempo corrente; al termine di ciò andremo a richiedere l'intervento dello scheduler.

## `void GetSupportData(state_t *saved_exception_state)`
Se il valore in `a0` è `-8` allora la syscall è `GetSupportData`; questa syscall viene chiamata quando è richiesto un puntatore alla struttura di supporto del processo corrente. Ricaviamo tale puntatore dal campo `p_supportStruct` contenuto all'interno della struttura di ogni istanza `pcb_t`. In caso il campo `p_supportStruct` risultasse vuoto la funzione ritorna `NULL`.

## `void GetProcessID(state_t *saved_exception_state)`
Se il valore in `a0` è `-9` allora la syscall è `GetProcessID`; usiamo questa syscall per ritornare l'ID del processo inserendolo nel registro `a0`. Per prima cosa, all’interno di questa funzione, andiamo ad eseguire un controllo sul registro `a1`:
- Se questo risulta uguale a `0` allora il processo richiedente è lo stesso del quale dobbiamo ritornare l'ID;
- Se risulta diverso da 0 allora eseguiamo un ulteriore controllo: se il processo ha un padre ritorniamo l'ID del padre; se non ha un padre, e quindi il processo risulta essere la radice dell'albero dei processi, allora inseriamo il valore `0` nel registro `a0`.
Al termine di tutto restituiamo il controllo al processo che avevo richiesto la syscall.

Ci sono due ultimi concetti da analizzare: il primo è che, in tutte le syscall, quando vogliamo modificare i valori di una o più VARIABILI GLOBALI allora lavoriamo in mutua esclusione acquisendo il `GLOBAL_LOCK` all’inizio della funzione e rilasciandolo alla fine. Facciamo questo per evitare che due processi tentino di accedere alla stessa risorsa nello stesso momento. Il secondo invece è che, ogni volta che vogliamo restituire il controllo a un processo che ha richiesto una syscall (`LDST(saved_exception_state)`), prima di farlo incrementiamo il Program Counter (`pc_epc`) di tale processo di un `WORDLEN` (4 byte ovvero la lunghezza di un’istruzione) per evitare che la syscall venga ripetuta all’infinito dopo che il processo ritorna dalla gestione dell’eccezione.

# interrupts.c

La gestione delle eccezioni causate da uno qualsiasi dei tre tipi di interrupts avviene all’interno del file interrupts.c, il quale contiene una funzione principale che instrada i vari interrupts in determinati gestori a seconda della loro causa. In particolare, il punto di ingresso è la procedura `void INTERRUPT_HANDLER()`, la quale prende in ingresso:
- lo stato del processore al momento dell’eccezione
- il codice dell’eccezione, che in questo caso coinciderà con un interrupt
Dopodichè, per classificare correttamente l’interrupt exception code, viene effettuato un AND bit a bit tra l’intero passato in input - calcolato precedentemente all’interno del gestore `exceptionHandler()` tramite la macro `getCAUSE()` - e la costante `CAUSE_EXCCODE_MASK`.

Ora, tramite il costrutto dello switch, l’interrupt e i parametri necessari per gestirlo saranno passati a una delle tre seguenti funzioni, sulla base della categoria:
in caso di interrupt causato dal PLT (Processor Local Timer) - costante `IL_CPUTIMER` - il controllo viene passato alla funzione `void local_timer_interrupt_handler()`;
in caso di interrupt causato dallo Pseudo-clock - costante `IL_TIMER` - il controllo viene passato alla funzione `void pseudoclock_interrupt_handler()`;
in caso di non-timer interrupts, ovvero causati da uno degli otto device - costanti `IL_DISK, IL_FLASH, IL_ETHERNET, IL_PRINTER, IL_TERMINAL`- il controllo viene passato alla funzione `void  device_interrupt_handler()`
Prima di passare all’illustrazione del funzionamento di questi gestori, occorre far presente che l’ordine in cui sono stati inseriti i casi all’interno dello switch non è casuale, bensì, è basato sulle priorità dei vari interrupts. 

## Processor Local Timer (PLT) Interrupts
La funzione che gestisce gli interrupts PLT è la `void local_timer_interrupt_handler()`, che prende in input un solo parametro, lo stato del processore al momento dell’eccezione. Questa eccezione viene sollevata nel momento in cui il processo in stato di running, non essendo andato incontro a nessuna terminazione né avendo effettuato una transizione allo stato “bloccato”, ha terminato il suo tempo quantum/slice ma non completato il suo CPU Burst. Le operazioni da effettuare in questo caso sono:
confermare l’interrupt PLT caricando un nuovo valore per il timer usando la macro `setTIMER`;
copiare lo stato del processore al momento dell’eccezione all’interno del processo attualmente in atto della CPU corrente;
spostare il processo corrente dallo stato running all’interno della Ready Queue;
infine, chiamare lo scheduler

## The System-wide Interval Timer and the Pseudo-clock
La funzione che gestisce gli interrupts causati dallo Pseudo-clock è la `void pseudoclock_interrupt_handler()`, che prende in input un solo parametro, lo stato del processore al momento dell’eccezione. Questa eccezione viene sollevata a seguito di un tick dello Pseudo-clock, in quanto sarà necessario liberare la totalità dei processi che erano rimasti bloccati in attesa del tic dopo aver evocato la syscall `WaitForClock`. Occorrerà:
come nel caso precedente, confermare l’interrupt l’Interval Timer con un nuovo valore, 100 millisecondi, dato dalla costante `PSECOND`, operazione che può essere effettuata tramite la macro `LDIT(T)`;
liberare tutti i PCB bloccati in attesa dello Pseudo-clock tick: ricordiamo che, se esistono, questi PCB sono fermi al semaforo indicato dall’ultima posizione all’interno dell’array `DEVICE_SEMAPHORES`, ovvero, si effettuerà una `removeBlocked(&DEVICE_SEMAPHORES[SEMDEVLEN - 1])`. Ciascuno dei PCB liberati da questa operazione verrà poi inserito all’interno della Ready Queue e verrà infine effettuata una operazione P sul corretto semaforo. 
infine, bisogna gestire il ritorno del controllo dell’esecuzione al processo corrente della CPU corrente, tramite la macro `LDST`; nel caso in cui questo non esista, occorrerà chiamare lo Scheduler.

## Non-Timer Interrupts
I non-timer interrupts sono tutte quelle eccezioni non sollevate dai timer, ovvero, causate da uno degli otto device disponibili; ci sono una serie di operazioni da effettuare per poter gestire questa situazione:
Calcolare il numero di linea dell’interrupt - `IntLineNo`;
Calcolare il numero del device che ha causato l’interrupt - `devNo`;
Effettuare una V sul semaforo associato a tale device;
Ritornare il controllo.

1. Per il calcolo dell’IntLineNo è stata usata una funzione ausiliaria, la `int getIntLineNo()`, la quale, dato in input il valore della cause, restituisce il corretto numero di linea dell’interrupt in questione. Si noti che i vari casi del costrutto if sono ordinati in modo da rispettare la priorità dei vari interrupts, a partire dai PLT fino all’ultimo dei device interrupts.

2. Per il calcolo del devNo, viene innanzitutto inizializzato un puntatori a interi linePendingInterrupts all’indirizzo in memoria in cui è allocata la Bit Map degli interrupting device, 0x10000040, e poi si calcola il device number tramite una serie di if che contengono l’AND bit a bit tra la bitmap e le varie costanti `DEV0ON…DEV7ON`. Se, ad esempio, se si entrerà nel ramo if relativo alla costante `DEV3ON`, allora a devNo viene assegnato 3, e così via. 

3. Ora che sono stati calcolati il numero di linea e il numero del device, è possibile ragionare sul registro allocato per tale device e, di conseguenza, anche sul corrispondente semaforo all’interno del `DEVICE_SEMAPHORES`. In particolare, l’indirizzo del registro si calcola tramite l’operazione: `*devAddrBase = 0x10000054 + ((IntLineNo - 3) * 0x80) + (devNo * 0x10)`. A partire da questo indirizzo:
Si verifica che non si tratti di un dispositivo terminale - `IntLineNo = 7` - e se è quello il caso, allora occorre controllare se si tratta di un dispositivo in trasmissione o in ricezione e modificare il puntatore di conseguenza. 
Si salva un puntatore al campo relativo al comando corretto e si va ad inserire la costante `ACK` all’interno di quest’ultimo - tutto questo per riconoscere effettivamente che è avvenuto l’interrupt. Inoltre, è necessario salvare anche lo status code. 
Ora, a seguito di queste operazioni preliminari, è necessario effettuare una V sul semaforo associato a tale device e, per farlo, occorre andare a calcolare l’indice per accedervi tramite l’array di `DEVICE_SEMAPHORES`. A tale scopo, è stata utilizzata la funzione ausiliaria `int getSemaphoreDeviceIndex()` - per informazioni in merito all’implementazione di quest’ultima si veda la sezione relativa al file exceptions.c. Nel caso in cui vi fosse un processo bloccato su quest’ultimo, si libera il corrispondente PCB e lo si inserisce nella Ready Queue, andando a salvare lo status_code trovato nei passi precedenti all’interno del suo campo status. 

Infine, è necessario ritornare correttamente il controllo a seguito dell’interrupt, in particolare, se non ci sono processi attualmente in esecuzione, verrà effettuata una chiamata allo scheduler, altrimenti, si effettuerà la chiamata alla macro `LDST(GET_EXCEPTION_STATE_PTR(getPRID()))`.

NOTA: Risulta necessario soffermarsi sul funzionamento dei semafori relativi all’array `DEVICE_SEMAPHORES` i quali, almeno per quanto riguarda i dispositivi, vengono gestiti dalla syscall `DOIO` e dalla funzione `void device_interrupt_handler()`. In particolare, al contrario del funzionamento dei classici semafori, il valore 1 indica la presenza di un dispositivo bloccato su tale semaforo: è per questo motivo che, all’interno della syscall `DOIO`, il corrispondente semaforo del dispositivo viene incrementato - proprio per segnalare la presenza di qualcuno bloccato - e decrementato all’interno della `void device_interrupt_handler()`, ovvero, nel momento in cui si effettua l’operazione V e si libera dunque il dispositivo. 
