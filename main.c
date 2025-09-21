#define _POSIX_C_SOURCE 200809L

#include <stdio.h>       
#include <stdlib.h>      
#include <string.h>      
#include <errno.h>       
#include <unistd.h>   
#include <mqueue.h> 
#include <ctype.h> 
#include <signal.h>
#include <stdatomic.h>
/*- - - -  - - - - - - -  - - - - - - -  - - - - - - -  - - - - - - -  - - - - - - -  - - - - - - -  - - - - - - -  - - - - - - -  - - - */

#include "macro.h" 
#include "log.h" 
#include "env.h" 
#include "rescuers.h" 
#include "emergency.h" 
#include "treat_emergency.h" 

/*- - - -  - - - - - - -  - - - - - - -  - - - - - - -  - - - - - - -  - - - - - - -  - - - - - - - - - - - - - - - - - - - - - -  - - - - */




//mi creo una variabile atomica per metterla nel while e leggere messaggi dalla coda, la cambio solo se il client invia il segnale di fine
atomic_int iteratore= 1; 

//mi creo 2 mutex, uno per accedere ai digital_twins e uno per le code di priorità  e 3 condition variables per le priorità
mtx_t accedi_twins;
mtx_t accedi_code;
cnd_t coda_prio0;
cnd_t coda_prio1;
cnd_t coda_prio2;

//creo contatori code
int prio0=0;
int prio1=0;
int prio2=0;


//mi dichiaro la struttura con dentro i rescuer e i digital twins globale almeno posso lavorarci anche nella funzione dei thread
int sizeof_twins_array;

//creo funzione per inizializzare il mutex per evre più pulizia nel main
void inizializza_mutex(mtx_t* mutex){
    if(mtx_init(mutex, mtx_plain) != thrd_success){
        perror("errore inizializzazione mutex in main.c");
        return ;
    }
}

//creo funzione per inizializzare condition variable
void inizializza_cond( cnd_t* cond){
    if(cnd_init(cond) != thrd_success){
        perror("errore inizializzazione condition variable in main.c");
        return ;
    }
}


//creo gestore per il segnale mandato dal client
void gestore_server(int signal){
    if(signal==SIGINT){
        printf("Segnale SIGINT ricevuto dal client!\n");
        atomic_store(&iteratore, 0);
    }
}

/*mi creo una funzione to_lower in modo da rendere una stringa tutta minuscola, non uso la chiamata di libreria perchè dovrei fare tutto 
 il for per rendere minuscolo un carattere alla volta*/
char* to_lower(char* string){
    int lunghezza_stringa=strlen(string);
    for(int i=0; i<lunghezza_stringa; i++){
        string[i]= tolower((unsigned char)string[i]); //uso unsigned char per evitare comportamenti indefiniti con caratteri speciali
    }
    return string;
}

//creo una funzione per ripristinare i valori prima di rifare il while una volta svegliato da una cnd_wait
void resetta_controllo(int* contatori, int* indici, int richiesti, int type_rescuers, emergency_t* e) {
    for (int i = 0; i < type_rescuers; i++) {
        contatori[i] = e->type.rescuers[i].required_cont;
    }
    for (int i = 0; i < richiesti; i++) {
        indici[i] = -1;
    }
}


//creo una funzione per vedere se ci sono i soccorritori richiesti per un emergenza
int verifica_soccorritori(int lunghezza_twin, int quanti_tipi_soccorritori, int accumulatore, int soccorritori_totali, int* contatori, int* indici_buoni, emergency_t* emergenza){
    for(int i=0; i<quanti_tipi_soccorritori; i++){
        for(int j=0; j<lunghezza_twin; j++){
            if((strcmp(emergenza->type.rescuers[i].type->rescuer_type_name, emergenza->rescuers_dt[j].rescuer->rescuer_type_name)== 0) &&(emergenza->rescuers_dt[j].status==IDLE)){
                if(contatori[i] !=0){
                    contatori[i]--;
                    indici_buoni[accumulatore]=j;
                    accumulatore++;
                } else {
                    continue;
                }
            } 
        }
    }
    if(accumulatore==soccorritori_totali){
        return 1;
    } else {
        return 0;
    } 
}

//funzione per convertire lo status di un soccorritore in stringa in modo da poterlo passare alla funzione per loggare l'evento
const char* rescuer_status_to_string(rescuer_status_t status) {
    switch (status) {
        case IDLE: return "IDLE";
        case EN_ROUTE_TO_SCENE: return "EN_ROUTE_TO_SCENE";
        case ON_SCENE: return "ON_SCENE";
        case RETURNING_TO_BASE: return "RETURNING_TO_BASE";
        default: return "UNKNOWN_STATUS";
    }
}

//funzione per scorrere l'array di digital twin e cambiare lo stato ai soccorritori
void cambia_stato(emergency_t* emergenza, int richiesti, int* indici, rescuer_status_t nuovo_stato){
    //vado nell'array twin e cambio i soccorritori in EN_ROUTE_TO_SCENE
    for(int i=0; i<richiesti; i++){
        emergenza->rescuers_dt[indici[i]].status=nuovo_stato;

        //mi creo una variabile per estrarre l'id e passarlo al file di configurazione
        char id[10];
        snprintf(id,sizeof(id), "%d", emergenza->rescuers_dt[indici[i]].id);
        
        //porto lo stato in stringa per metterlo nella funzione di log
        const char* tmp = rescuer_status_to_string(emergenza->rescuers_dt[indici[i]].status);
        char status[25];
        strncpy(status, tmp, sizeof(status));
        status[sizeof(status) - 1] = '\0'; // per sicurezza


        //loggo il cambiamento di stato
        log_event(time(NULL), id, "RESCUERS_STATUS", "il soccorritore con id: %d, è nello stato: %s",  emergenza->rescuers_dt[indici[i]].id, status);
    }
}

//funzione per mandare la signal a chi è in coda
void risveglia_prossimo_thread() {
    if (prio2 > 0) cnd_signal(&coda_prio2);
    else if (prio1 > 0) cnd_signal(&coda_prio1);
    else cnd_signal(&coda_prio0);
}


//funzione per emergenze a priorità 2
int gestisci_emergenza_prio2(void* arg){
    emergency_t* emergenza= (emergency_t*)arg;

    //variabile per il tempo priorità
    int tempo_per_priorità=10;

    mtx_lock(&accedi_twins);
    int lung_twin= sizeof_twins_array;
    int type_rescuers= emergenza->type.rescuers_request_numer;
    int contatori[type_rescuers];
    int totali= 0;
    int richiesti=emergenza->rescuer_count;
    int indici[richiesti];

        
    while(1){
        //riporto le variabili nei valori originali
        totali=0;
        richiesti= emergenza->rescuer_count;
        resetta_controllo(contatori, indici, richiesti, type_rescuers, emergenza);

        //verifico che ci siano tutti i soccorritori chiamando la funzione creata
        int controllo_soccorritori= verifica_soccorritori(lung_twin, type_rescuers, totali, richiesti, contatori, indici, emergenza);
        if(controllo_soccorritori==1){
            //vado nell'array twin e cambio i soccorritori in EN_ROUTE_TO_SCENE in questo caso non uso la funzione perchè devo anche loggare i soccorritori assegnati all'emergenza
            for(int i=0; i<richiesti; i++){
                emergenza->rescuers_dt[indici[i]].status=EN_ROUTE_TO_SCENE;

                //mi creo una variabile per estrarre l'id e passarlo al file di configurazione
                char id[10];
                snprintf(id,sizeof(id), "%d", emergenza->rescuers_dt[indici[i]].id);

                const char* tmp = rescuer_status_to_string(emergenza->rescuers_dt[indici[i]].status);
                char status[25];
                strncpy(status, tmp, sizeof(status));
                status[sizeof(status) - 1] = '\0'; // per sicurezza


                //loggo il cambiamento di stato
                log_event(time(NULL), id, "RESCUERS_STATUS", "il soccorritore con id: %d, è nello stato: %s",  emergenza->rescuers_dt[indici[i]].id, status);

                //loggo che un soccorritore è stato assegnato
                log_event(time(NULL), id, "RESCUERS_ASSIGN", "il soccorritore con id: %d, è stato assegnato all'emergenza %s", emergenza->rescuers_dt[indici[i]].id, emergenza->type.emergency_descrition);
            } 
            mtx_unlock(&accedi_twins);

            //calcolo il tempo trascorso
            time_t attuale=time(NULL);
            double tempo_trascorso= difftime(attuale, emergenza->time);

            if(tempo_trascorso<tempo_per_priorità){
                //cambio stato dell'emergenza e la loggo
                emergenza->status=ASSIGNED;
                log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "emergenza passata ASSIGNED");

                //tutta logica di gestione emergenza
                int distanza_soccorritori[type_rescuers];
                int tempo_percorrenza_soccorritori[type_rescuers];

                for(int i=0; i<type_rescuers; i++){
                    distanza_soccorritori[i]= abs(emergenza->type.rescuers[i].type->x - emergenza->x) + abs(emergenza->type.rescuers[i].type->y - emergenza->y);
                    tempo_percorrenza_soccorritori[i]= distanza_soccorritori[i]/emergenza->type.rescuers[i].type->speed;
                }

                //simulo il lavoro dei soccorritori un tipo alla volta 
                for(int i=0; i<type_rescuers; i++){
                    if(i!=(type_rescuers-1)){
                        //simulo il tragitto all'andata e loggo il cambiamento di stato dei soccorritori 
                        sleep(tempo_percorrenza_soccorritori[i]);

                        //simulo il tempo di lavoro
                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, ON_SCENE);
                        sleep(emergenza->type.rescuers[i].time_to_manage);
                        //cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, ON_SCENE);

                        //simulo il ritorno alla base
                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, RETURNING_TO_BASE);
                        sleep(tempo_percorrenza_soccorritori[i]);
                        //cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, RETURNING_TO_BASE);

                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, IDLE);

                        //svuoto l'array dei primi 5 elementi in modo da poter al prossimo giro del for lavorare su quelli rimanenti
                        for(int j= emergenza->type.rescuers[i].required_cont; j<richiesti; j++){
                            indici[j - emergenza->type.rescuers[i].required_cont] = indici[j];
                        }
                        richiesti -= emergenza->type.rescuers[i].required_cont;
                    } else {
                        //simulo il tragitto all'andata e loggo il cambiamento di stato dei soccorritori e dell'emergenza
                        sleep(tempo_percorrenza_soccorritori[i]);
                        emergenza->status=IN_PROGRESS;
                        log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "emergenza passata IN_PROGRESS");

                        //simulo il tempo di lavoro
                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, ON_SCENE);
                        sleep(emergenza->type.rescuers[i].time_to_manage);
                        //cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, ON_SCENE);
                        emergenza->status=COMPLETED;
                        log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "emergenza passata COMPLETED");

                        //simulo il ritorno alla base
                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, RETURNING_TO_BASE);
                        sleep(tempo_percorrenza_soccorritori[i]);
                    
                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, IDLE);
                    }
                }
                //ho servito l'emergenza e liberato i soccorritori posso rilasciare le risorse
                free(emergenza->type.emergency_descrition);
                free(emergenza);

                //faccio la signal
                risveglia_prossimo_thread();

                //ritorno il successo del thread
                return thrd_success;
            } else {
                //è passato troppo tempo nel servire l'emergenza vado in timedout ma prima rimetto i soccorritori allocati ad IDLE
                cambia_stato(emergenza, richiesti, indici, IDLE );

                emergenza->status=TIMEOUT;
                log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "l'emergenza è passata nello stato TIMEDOUT perchè non è stato possibile servirla in tempo");

                //la risorsa è andata in Timeout e non può più essere gestita libero risorse e termino il thread
                free(emergenza->type.emergency_descrition);
                free(emergenza);

                //faccio la signal
                risveglia_prossimo_thread();

                //ritorno il successo del thread
                return thrd_success;
            }
        } else {
            //non ci sono soccorritori mi metto in coda
            log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "Carenza di soccorritori, emergenza aggiunta in coda");
            mtx_lock(&accedi_code);
            prio2++;
            mtx_unlock(&accedi_code);
            cnd_wait(&coda_prio2, &accedi_twins);

            // appena risvegliato, decrementa il contatore
            mtx_lock(&accedi_code);
            prio2--;
            mtx_unlock(&accedi_code);
        }
    }
}

//funzione per emergenza a priorità 1
int gestisci_emergenza_prio1(void* arg){
    emergency_t* emergenza= (emergency_t*)arg;

    //variabile per il tempo priorità
    int tempo_per_priorità=30;
        
    mtx_lock(&accedi_twins);
    int lung_twin= sizeof_twins_array;
    int type_rescuers= emergenza->type.rescuers_request_numer;
    int contatori[type_rescuers];
    int totali= 0;
    int richiesti=emergenza->rescuer_count;
    int indici[richiesti];

        
    while(1){
        //riporto le variabili nei valori originali
        totali=0;
        richiesti= emergenza->rescuer_count;
        resetta_controllo(contatori, indici, richiesti, type_rescuers, emergenza);

        //verifico che ci siano tutti i soccorritori chiamando la funzione creata
        int controllo_soccorritori= verifica_soccorritori(lung_twin, type_rescuers, totali, richiesti, contatori, indici, emergenza);
        if(controllo_soccorritori==1){
            //vado nell'array twin e cambio i soccorritori in EN_ROUTE_TO_SCENE in questo caso non uso la funzione perchè devo anche loggare i soccorritori assegnati all'emergenza
            for(int i=0; i<richiesti; i++){
                emergenza->rescuers_dt[indici[i]].status=EN_ROUTE_TO_SCENE;

                //mi creo una variabile per estrarre l'id e passarlo al file di configurazione
                char id[10];
                snprintf(id,sizeof(id), "%d", emergenza->rescuers_dt[indici[i]].id);

                const char* tmp = rescuer_status_to_string(emergenza->rescuers_dt[indici[i]].status);
                char status[25];
                strncpy(status, tmp, sizeof(status));
                status[sizeof(status) - 1] = '\0'; // per sicurezza


                //loggo il cambiamento di stato
                log_event(time(NULL), id, "RESCUERS_STATUS", "il soccorritore con id: %d, è nello stato: %s",  emergenza->rescuers_dt[indici[i]].id, status);

                //loggo che un soccorritore è stato assegnato
                log_event(time(NULL), id, "RESCUERS_ASSIGN", "il soccorritore con id: %d, è stato assegnato all'emergenza %s", emergenza->rescuers_dt[indici[i]].id, emergenza->type.emergency_descrition);
            } 
            mtx_unlock(&accedi_twins);

            //calcolo il tempo trascorso
            time_t attuale=time(NULL);
            double tempo_trascorso= difftime(attuale, emergenza->time);

            if(tempo_trascorso<tempo_per_priorità){
                //cambio stato dell'emergenza e la loggo
                emergenza->status=ASSIGNED;
                log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "emergenza passata ASSIGNED");

                //tutta logica di gestione emergenza
                int distanza_soccorritori[type_rescuers];
                int tempo_percorrenza_soccorritori[type_rescuers];

                for(int i=0; i<type_rescuers; i++){
                    distanza_soccorritori[i]= abs(emergenza->type.rescuers[i].type->x - emergenza->x) + abs(emergenza->type.rescuers[i].type->y - emergenza->y);
                    tempo_percorrenza_soccorritori[i]= distanza_soccorritori[i]/emergenza->type.rescuers[i].type->speed;
                }

                //simulo il lavoro dei soccorritori un tipo alla volta 
                for(int i=0; i<type_rescuers; i++){
                    if(i!=(type_rescuers-1)){
                        //simulo il tragitto all'andata e loggo il cambiamento di stato dei soccorritori 
                        sleep(tempo_percorrenza_soccorritori[i]);

                        //simulo il tempo di lavoro
                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, ON_SCENE);
                        sleep(emergenza->type.rescuers[i].time_to_manage);

                        //simulo il ritorno alla base
                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, RETURNING_TO_BASE);
                        sleep(tempo_percorrenza_soccorritori[i]);

                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, IDLE);

                        //svuoto l'array dei primi 5 elementi in modo da poter al prossimo giro del for lavorare su quelli rimanenti
                        for(int j= emergenza->type.rescuers[i].required_cont; j<richiesti; j++){
                            indici[j - emergenza->type.rescuers[i].required_cont] = indici[j];
                        }
                        richiesti -= emergenza->type.rescuers[i].required_cont;
                    } else {
                        //simulo il tragitto all'andata e loggo il cambiamento di stato dei soccorritori e dell'emergenza
                        sleep(tempo_percorrenza_soccorritori[i]);
                        emergenza->status=IN_PROGRESS;
                        log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "emergenza passata IN_PROGRESS");

                        //simulo il tempo di lavoro
                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, ON_SCENE);
                        sleep(emergenza->type.rescuers[i].time_to_manage);
                        emergenza->status=COMPLETED;
                        log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "emergenza passata COMPLETED");

                        //simulo il ritorno alla base
                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, RETURNING_TO_BASE);
                        sleep(tempo_percorrenza_soccorritori[i]);
                        
                        cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, IDLE);
                    }
                }
                //ho servito l'emergenza e liberato i soccorritori posso rilasciare le risorse
                free(emergenza->type.emergency_descrition);
                free(emergenza);

                //faccio la signal
                risveglia_prossimo_thread();

                //ritorno il successo del thread
                return thrd_success;
            } else {
                //è passato troppo tempo nel servire l'emergenza vado in timedout ma prima rimetto i soccorritori allocati ad IDLE
                cambia_stato(emergenza, richiesti, indici, IDLE );

                emergenza->status=TIMEOUT;
                log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "l'emergenza è passata nello stato TIMEDOUT perchè non è stato possibile servirla in tempo");

                //la risorsa è andata in Timeout e non può più essere gestita libero risorse e termino il thread
                free(emergenza->type.emergency_descrition);
                free(emergenza);

                //faccio la signal
                risveglia_prossimo_thread();

                //ritorno il successo del thread
                return thrd_success;
            }
        } else {
            //non ci sono soccorritori mi metto in coda
            log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "Carenza di soccorritori, emergenza aggiunta in coda");
            mtx_lock(&accedi_code);
            prio1++;
            mtx_unlock(&accedi_code);
            cnd_wait(&coda_prio1, &accedi_twins);

            // appena risvegliato, decrementa il contatore
            mtx_lock(&accedi_code);
            prio1--;
            mtx_unlock(&accedi_code);
        }
    }
}

//funzione per emergenza a priorità 0
int gestisci_emergenza_prio0(void* arg){
    emergency_t* emergenza= (emergency_t*)arg;

    mtx_lock(&accedi_twins);
    int lung_twin= sizeof_twins_array;
    int type_rescuers= emergenza->type.rescuers_request_numer;
    int contatori[type_rescuers];
    int totali= 0;
    int richiesti=emergenza->rescuer_count;
    int indici[richiesti];

    while(1){
        //riporto le variabili nei valori originali
        totali=0;
        richiesti= emergenza->rescuer_count;
        resetta_controllo(contatori, indici, richiesti, type_rescuers, emergenza);

        //verifico che ci siano tutti i soccorritori chiamando la funzione creata
        int controllo_soccorritori= verifica_soccorritori(lung_twin, type_rescuers, totali, richiesti, contatori, indici, emergenza);
        if(controllo_soccorritori==1){
            //vado nell'array twin e cambio i soccorritori in EN_ROUTE_TO_SCENE in questo caso non uso la funzione perchè devo anche loggare i soccorritori assegnati all'emergenza
            for(int i=0; i<richiesti; i++){
                emergenza->rescuers_dt[indici[i]].status=EN_ROUTE_TO_SCENE;

                //mi creo una variabile per estrarre l'id e passarlo al file di configurazione
                char id[10];
                snprintf(id,sizeof(id), "%d", emergenza->rescuers_dt[indici[i]].id);

                const char* tmp = rescuer_status_to_string(emergenza->rescuers_dt[indici[i]].status);
                char status[25];
                strncpy(status, tmp, sizeof(status));
                status[sizeof(status) - 1] = '\0'; // per sicurezza


                //loggo il cambiamento di stato
                log_event(time(NULL), id, "RESCUERS_STATUS", "il soccorritore con id: %d, è nello stato: %s",  emergenza->rescuers_dt[indici[i]].id, status);

                //loggo che un soccorritore è stato assegnato
                log_event(time(NULL), id, "RESCUERS_ASSIGN", "il soccorritore con id: %d, è stato assegnato all'emergenza %s", emergenza->rescuers_dt[indici[i]].id, emergenza->type.emergency_descrition);
            } 
            mtx_unlock(&accedi_twins);

            //cambio stato dell'emergenza e la loggo
            emergenza->status=ASSIGNED;
            log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "emergenza passata ASSIGNED");

            //tutta logica di gestione emergenza
            int distanza_soccorritori[type_rescuers];
            int tempo_percorrenza_soccorritori[type_rescuers];

            for(int i=0; i<type_rescuers; i++){
                distanza_soccorritori[i]= abs(emergenza->type.rescuers[i].type->x - emergenza->x) + abs(emergenza->type.rescuers[i].type->y - emergenza->y);
                tempo_percorrenza_soccorritori[i]= distanza_soccorritori[i]/emergenza->type.rescuers[i].type->speed;
            }

            //simulo il lavoro dei soccorritori un tipo alla volta 
            for(int i=0; i<type_rescuers; i++){
                if(i!=(type_rescuers-1)){
                    //simulo il tragitto all'andata e loggo il cambiamento di stato dei soccorritori 
                    sleep(tempo_percorrenza_soccorritori[i]);

                    //simulo il tempo di lavoro
                    cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, ON_SCENE);
                    sleep(emergenza->type.rescuers[i].time_to_manage);

                    //simulo il ritorno alla base
                    cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, RETURNING_TO_BASE);
                    sleep(tempo_percorrenza_soccorritori[i]);

                    cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, IDLE);

                    //svuoto l'array dei primi 5 elementi in modo da poter al prossimo giro del for lavorare su quelli rimanenti
                    for(int j= emergenza->type.rescuers[i].required_cont; j<richiesti; j++){
                        indici[j - emergenza->type.rescuers[i].required_cont] = indici[j];
                    }
                    richiesti -= emergenza->type.rescuers[i].required_cont;
                } else {
                     //simulo il tragitto all'andata e loggo il cambiamento di stato dei soccorritori e dell'emergenza
                    sleep(tempo_percorrenza_soccorritori[i]);
                    emergenza->status=IN_PROGRESS;
                    log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "emergenza passata IN_PROGRESS");

                    //simulo il tempo di lavoro
                    cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, ON_SCENE);
                    sleep(emergenza->type.rescuers[i].time_to_manage);
                    emergenza->status=COMPLETED;
                    log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "emergenza passata COMPLETED");

                    //simulo il ritorno alla base
                    cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, RETURNING_TO_BASE);
                    sleep(tempo_percorrenza_soccorritori[i]);
                        
                    cambia_stato(emergenza, emergenza->type.rescuers[i].required_cont, indici, IDLE);
                }
            }
            //ho servito l'emergenza e liberato i soccorritori posso rilasciare le risorse
            free(emergenza->type.emergency_descrition);
            free(emergenza);

            //faccio la signal
            risveglia_prossimo_thread();

            //ritorno il successo del thread
            return thrd_success;
        } else {
            //non ci sono soccorritori mi metto in coda
            log_event(time(NULL), emergenza->type.emergency_descrition, "EMERGENCY_STATUS", "Carenza di soccorritori, emergenza aggiunta in coda");
            mtx_lock(&accedi_code);
            prio0++;
            mtx_unlock(&accedi_code);
            cnd_wait(&coda_prio0, &accedi_twins);

            // appena risvegliato, decrementa il contatore
            mtx_lock(&accedi_code);
            prio0--;
            mtx_unlock(&accedi_code);
        }
    }
}


int main(){

    //inizializzo i mutex e le condition variables
    inizializza_mutex(&accedi_twins);
    inizializza_mutex(&accedi_code);
    inizializza_cond(&coda_prio0);
    inizializza_cond(&coda_prio1);
    inizializza_cond(&coda_prio2);


    //inizializzo il mutex di log.c
    inizializza_mutex(&mutex_log);
    

    //chiamo i miei parser in ordine env.conf --> rescuers.conf --> emergency.conf
    leggi_e_disponi("env.conf");
    int** griglia= crea_griglia(width, height);
    
    //aggiungo il carattere '/' per inizializzare la coda
    char nome_coda[51];
    if(nome_queue[0] != '/'){
        snprintf(nome_coda, sizeof(nome_coda), "/%s", nome_queue );
        strcpy(nome_queue, nome_coda);
    }

    mqd_t mq;
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;     // max messaggi in coda
    attr.mq_msgsize = 100;   // dimensione max di un messaggio
    attr.mq_curmsgs = 0;
    
    mq=mq_open(nome_queue, O_RDWR|O_CREAT, 0666, &attr);
    PROTECT_MQ(mq, "errore nell'apertura della message queue nel main");

    // Carico i soccorritori e copio la grandezza dell'array twins in una variabile globale perchè servirà nella funzione dei thread
    arrays_rescuers rescuers_data = arrays("rescuers.conf");
    sizeof_twins_array=rescuers_data.size_array_twin;

    if (rescuers_data.array_rescuers == NULL || rescuers_data.array_twin_rescuer == NULL) {
        printf("Errore: array dei soccorritori non creato correttamente.\n");
        return 1;
    }

    // Carico le emergenze
    array_emergency_t emergencies = crea_array_emergency("emergency_types.conf", rescuers_data);

    if (emergencies.array == NULL || emergencies.size == 0) {
        printf("Errore: nessuna emergenza caricata.\n");
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = gestore_server;
    sigaction(SIGINT, &sa, NULL);

 
    //mi creo il buffer per leggere dalla coda
    char buffer[SIZE_RIGA_LETTA];


    //creo una variabile che incremento per dare l'id al thread
    thrd_t* threads=NULL;
    int size_threads=0;

    while((iteratore==1) || (attr.mq_curmsgs !=0)){
        emergency_request_t request;
        
        //ricevo messaggio dalla coda
        int risultato=mq_receive(mq, buffer, SIZE_RIGA_LETTA, NULL);
        if(risultato== -1){
            //gestisco la receive quando chiamo il segnale di fine programma
            if(errno==4){
                break;
            }

            //loggo l'errore 
            log_event(time(NULL), "emergenza", "MESSAGE_QUEUE", "errore nella ricezione del messaggio dalla coda");
            perror("errore");
            
            //aggiorno attributi perchè un messaggio l'ho tolto
            mq_getattr(mq, &attr);

            continue;
        }

        

        //aggiorno quanti messaggi ci sono all'interno della cosa
        mq_getattr(mq, &attr);
        
        //divido il messaggio nei campi
        if(sscanf(buffer, "%[^,],%d,%d,%ld", request.emergency_name, &request.x, &request.y, &request.timestamp)==4){
            //verifico coordinata x
            if((request.x<0) || (request.x > width)){
                log_event(time(NULL), request.emergency_name, "MESSAGGE_QUEUE", "coordinata x non valida");

                continue;
            }
            
            //verifico coordinata y
            if((request.y<0) || (request.y > height)){
                log_event(time(NULL), request.emergency_name, "MESSAGGE_QUEUE", "coordinata y non valida");
                
                continue;
            }

            //verifico il timestamp
            if(request.timestamp <0){
                log_event(time(NULL), request.emergency_name, "MESSAGE_QUEUE", "timestamp non valido");

                continue;
            }
            
            //verifico se esiste l'emergenza
            int visit_all_array=0;
            for(int i=0; i<emergencies.size; i++){
                if(strcmp(to_lower(request.emergency_name), emergencies.array[i].emergency_descrition)==0){
                    break;
                } else {
                    visit_all_array++;
                }
            }
            //se la variabile visit_all_array è arrivata alla grandezza dell'array delle emergenze allora l'emergenza non esiste
            if(visit_all_array==emergencies.size){
                log_event(time(NULL), request.emergency_name, "MESSAGE_QUEUE", "nome dell'emergenza non valido, emergenza non riconosciuta");

                continue;
            }
            log_event(time(NULL), request.emergency_name, "MESSAGE_QUEUE", "emergenza valida: %s, %d, %d, %ld", request.emergency_name, request.x, request.y, request.timestamp);

            //tutti i controlli sono andati a buon fine creo la struttura emergency_t, prima però creo emergency_type_t
            emergency_type_t tipo_emergenza;
            for(int i=0; i<emergencies.size; i++){
                if(strcmp(request.emergency_name, emergencies.array[i].emergency_descrition)==0){
                    tipo_emergenza.emergency_descrition= strdup(request.emergency_name);
                    tipo_emergenza.priority=emergencies.array[i].priority;
                    tipo_emergenza.rescuers=emergencies.array[i].rescuers;
                    tipo_emergenza.rescuers_request_numer=emergencies.array[i].rescuers_request_numer;
                }
            }

            //mi creo una variabile per capire il numero di soccorritori totali
            int soccorritori_totali_emergenza=0;
            for(int i=0; i<tipo_emergenza.rescuers_request_numer; i++){
                soccorritori_totali_emergenza+= tipo_emergenza.rescuers[i].required_cont;
            }

            //ora ho tutto il necessario per creare emergency_t
            emergency_t* emergenza_ricevuta = malloc(sizeof(emergency_t));
            emergenza_ricevuta->type= tipo_emergenza;
            emergenza_ricevuta->status= WAITING;
            emergenza_ricevuta->x=request.x;
            emergenza_ricevuta->y=request.y;
            emergenza_ricevuta->time= request.timestamp;
            emergenza_ricevuta->rescuer_count= soccorritori_totali_emergenza;
            emergenza_ricevuta->rescuers_dt=rescuers_data.array_twin_rescuer;

            //alloco spazio per il thread
            thrd_t* tmp = realloc(threads, (size_threads + 1) * sizeof(thrd_t));
            PROTECT_MEMORY_DINAMIC(tmp, "errore nella allocazione del thread in main.c");
            threads = tmp;
            size_threads++;

            //creo il thread che si occuperà della gestione dell'emergenza in base alla priorità
            switch(emergenza_ricevuta->type.priority){
                case 2: //priorità è 2
                    thrd_create(&threads[size_threads-1], gestisci_emergenza_prio2, emergenza_ricevuta);
                    break;
                case 1: //priorità 1
                    thrd_create(&threads[size_threads-1], gestisci_emergenza_prio1, emergenza_ricevuta);
                    break;
                default: //priorità 0
                    thrd_create(&threads[size_threads-1], gestisci_emergenza_prio0, emergenza_ricevuta);
                    break;
            }
        } else {
            log_event(time(NULL), request.emergency_name, "MESSAGE_QUEUE", "errore nella sscanf la riga dell'emergenza non è ben formattata");
        }
    }
    //quando esco dal while vuoldire che il programma è finito quindi faccio pulizia totale

    //faccio la join dei threads e libero la memori allocata per l'array di threads
    for(int i=0; i< size_threads; i++){
        thrd_join(threads[i], NULL);
    }

    free(threads);

    //distruggo mutex
    mtx_destroy(&mutex_log);
    mtx_destroy(&accedi_code);
    mtx_destroy(&accedi_twins);

    //elimino condition varible
    cnd_destroy(&coda_prio0);
    cnd_destroy(&coda_prio1);
    cnd_destroy(&coda_prio2);

    //chiudo la mq_queue e faccio unlink
    int ritorno=mq_close(mq);
    PROTECT_MQ(ritorno, "errore nella chiusura della coda nel main.c");

    mq_unlink(nome_coda);

    //libero la memoria allocata negli array
    for (int i = 0; i < emergencies.size; i++) {
        free(emergencies.array[i].emergency_descrition);
        free(emergencies.array[i].rescuers);
    }
    free(emergencies.array);

    for (int i = 0; i < rescuers_data.size_array_rescuers; i++) {
        free(rescuers_data.array_rescuers[i].rescuer_type_name);
    }
    free(rescuers_data.array_rescuers);
    free(rescuers_data.array_twin_rescuer);

    //libero la griglia
    for (int i = 0; i < width; i++) {
        free(griglia[i]);  // libera ogni riga
    }
    free(griglia);  // libera l'array di puntatori


    printf("Programma concluso con successo. Memoria liberata.\n");
    return 0;
}
