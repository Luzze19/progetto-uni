#include <stdio.h>       
#include <stdlib.h>      
#include <string.h>      
#include <errno.h>       
#include <stdbool.h> 


#include "macro.h"
#include "emergency.h"
#include "rescuers.h"
#include "log.h"

//funzione di appoggio per contare i soccorritori in una linea
int conta_soccorritori(char* stringa) {
    int count = 0;
    char copia[SIZE_RIGA_LETTA];
    snprintf(copia, sizeof(copia), "%s", stringa); //metto stringa in copia per non lavorare con l'originale 

    char* token = strtok(copia, ";");
    while (token != NULL) {
        count++;
        token = strtok(NULL, ";");
    }
    //ritorno il contatore che mi servirà a allocare spazio con l'array di rescuer_request_t
    return count;
}



array_emergency_t crea_array_emergency(char* filename, arrays_rescuers array_soccorritori){
    FILE* f;
    f=fopen(filename, "r"); //apro il file 
    if(f==NULL){
        //loggo l'errore
        log_event(time(NULL),filename, "FILE_PARSING", "errore nell'apertura del file emergency_types.conf");

        //termino l'esecuzione con la macro
        PROTECT_OPEN(f, "errore nell'apertura del file emergency_types.conf");
    } else {
        //andato tutto ok quindi loggo il successo
        log_event(time(NULL),filename, "FILE_PARSING", "file emergency_types.conf aperto con successo");
    }

    emergency_type_t* array_emergency=NULL;
    int size_array_emergency=0;

    short priorità;
    char descrizione_emergenza[50];
    char stringa_soccorritori[50]; //stringa per i soccorritori
    char stringa_appoggio[SIZE_RIGA_LETTA]; //riga completa
    int soccorritori_totali=0;

    while(fgets(stringa_appoggio, sizeof(stringa_appoggio), f)!= NULL){
        
        //divido i tre campi
        if(sscanf(stringa_appoggio, "[%[^]]] [%hd] %[^\n]", descrizione_emergenza,&priorità, stringa_soccorritori )==3){

            //scrivo nel file di log la riga letta
            log_event(
                time(NULL),filename, "FILE_PARSING",
                "Parsing riuscito. Descrizione emergenza: %s, Priorità: %d, Stringa soccorritori: %s", 
                descrizione_emergenza, priorità, stringa_soccorritori
            );

            //variabile per inizializzare l'array soccorritori_richiesti
            int count= conta_soccorritori(stringa_soccorritori); 

            rescuer_request_t* soccorritori_richiesti= (rescuer_request_t*)malloc(count*sizeof(rescuer_request_t)); //creo array per l'mergenza gia inizializzato
            PROTECT_MEMORY_DINAMIC(soccorritori_richiesti, "errore nella malloc dei soccorritori nel file parse_emergency_types.c");



            //ora opero sulla stringa originale per popolare l'array appena creato
            char nome[20]; //nome soccorritori
            int quanti; //quanti soccorritori
            int tempo_sul_posto; // tempo sul posto
            char* token=strtok(stringa_soccorritori, ";");
            int index=0; //indice per l'array soccorritori_richiesti

            while(token != NULL){
                if(sscanf(token, "%[^:]:%d,%d", nome, &quanti, &tempo_sul_posto) == 3){
                    rescuer_type_t* type=NULL;//creo la struttura e la popolo confrontandola con i tipi di soccorritori salvati nella'array creato in parse_rescuers.c
                    for(int i=0; i<array_soccorritori.size_array_rescuers; i++){
                        if(strcmp(array_soccorritori.array_rescuers[i].rescuer_type_name, nome) == 0){
                            type = &array_soccorritori.array_rescuers[i];
                            break;
                        }
                    }
                    
                    //faccio un controllo su type per vedere se ho trovato il tipo di soccorritore
                    if (type == NULL) {
                        log_event(time(NULL),filename, "FILE_PARSING", "tipo di soccorritore non riconosciuto: %s", nome);
                        token = strtok(NULL, ";"); // vai al prossimo token
                        continue;
                    }
                    
                    // a questo punto popolo l'array con i soccorritori trovati fino ad ora
                    soccorritori_richiesti[index].type=type;
                    soccorritori_richiesti[index].required_cont=quanti;
                    soccorritori_richiesti[index].time_to_manage=tempo_sul_posto;
                    soccorritori_totali++; //aumento la variabile per vedere quanti tipi di soccorritori ho
                    index++; //aumento l'indice per il prossimo tipo di soccorritori
                } else {
                    // gestisco il caso della riga mal formattata
                    log_event(time(NULL),filename, "FILE_PARSING", "riga mal formattata");
                    }
                token=strtok(NULL, ";");

            }
            
            //riempio l'array  array_emergency
            emergency_type_t* tmp=realloc(array_emergency, (size_array_emergency+1)*sizeof(emergency_type_t));
                PROTECT_MEMORY_DINAMIC(tmp, "errore realloc variabile d'appoggio in parse_emergency_types.c");
                array_emergency=tmp;
                if((priorità>=0) && (priorità<3)){ //controllo se la priorità è valida e nel caso creo l'array
                    array_emergency[size_array_emergency].emergency_descrition= strdup(descrizione_emergenza);
                    PROTECT_MEMORY_DINAMIC(array_emergency[size_array_emergency].emergency_descrition, "errore nella strdup della descrizione emergenza");
                    array_emergency[size_array_emergency].priority=priorità;
                    array_emergency[size_array_emergency].rescuers_request_numer=soccorritori_totali;
                    array_emergency[size_array_emergency].rescuers= soccorritori_richiesti;
                    size_array_emergency++; //incremento grandezza array
                } else {
                    log_event(time(NULL),filename, "FILE_PARSING", "riga mal formattata la priorità non è valida");
                }
            soccorritori_totali=0; //rimposto a zero per la prossima riga

        } else {
            // gestisco il caso della riga mal formattata
            log_event(time(NULL),filename, "FILE_PARSING", "riga mal formattata");
            }
    }
    
    //ceo la struttura finale composta dall'array di emergenze e la sua grandezza
    array_emergency_t array_emergenze_finale;
    array_emergenze_finale.array=array_emergency;
    array_emergenze_finale.size=size_array_emergency;

    //chiudo il file prima di ritornare
    int ritorno=fclose(f); 
    if(ritorno != 0){
        //loggo l'errore
        log_event(time(NULL),filename, "FILE_PARSING", "errore nella chiusura del file emergency_types.conf");

        //termino l'esecuzione con la macro
        PROTECT_CLOSE(ritorno, "errore nella chiusura del file emergency_types.conf");
    } else {
        //andato tutto ok quindi loggo il successo
        log_event(time(NULL),filename, "FILE_PARSING", "file emergency_types.conf chiuso con successo");
    }

    //loggo la fine del parsing
    log_event(time(NULL),filename, "FILE_PARSING", "parsing completato con successo");


    return array_emergenze_finale;
}