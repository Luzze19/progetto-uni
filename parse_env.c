#include <stdio.h>       
#include <stdlib.h>      
#include <string.h>      
#include <errno.h>       

#include "env.h" 
#include "macro.h" 
#include "log.h" 

int width;
int height;
char nome_queue[50];

void leggi_e_disponi(char* filename){
    FILE* f;
    f=fopen(filename, "r"); //apro il file
    
    if(f==NULL){
        //loggo l'errore
        log_event(time(NULL),filename, "FILE_PARSING", "errore nell'apertura del file env.conf");

        //termino l'esecuzione con la macro
        PROTECT_OPEN(f, "errore nell'apertura del file env.conf");
    } else {
        //andato tutto ok quindi loggo il successo
        log_event(time(NULL),filename, "FILE_PARSING", "file env.conf aperto con successo");
    }

    


    char riga_letta[SIZE_RIGA_LETTA];
    //int index=0;
    while(fgets(riga_letta, sizeof(riga_letta), f) != NULL){ //leggo una riga alla volta 
        
        //controllo per vedere a cosa corrisponde e faccio la sscanf e poi vedo se è andato tutto ok e metto anche le varie scritture nel file di log
        if (strncmp(riga_letta, "queue=", 6) == 0) {
            int controllo=sscanf(riga_letta, "queue=%s", nome_queue);
            if(controllo != 1){
                log_event(time(NULL),filename, "FILE_PARSING", "riga mal formattata. ");
            } 
            log_event(time(NULL),filename, "FILE_PARSING", "parsing della riga riuscito: queue=emergenze123456 ");

        } else if (strncmp(riga_letta, "width=", 6) == 0) {
            int controllo=sscanf(riga_letta, "width=%d", &width);
            if(controllo != 1){
                log_event(time(NULL),filename, "FILE_PARSING", "riga mal formattata. ");
            }
            log_event(time(NULL),filename, "FILE_PARSING", "parsing della riga riuscito: width=300");
            } else if (strncmp(riga_letta, "height=", 7) == 0) {
                int controllo=sscanf(riga_letta, "height=%d", &height);
                if(controllo != 1){
                    log_event(time(NULL),filename, "FILE_PARSING", "riga mal formattata. ");
                }
                log_event(time(NULL),filename, "FILE_PARSING", "parsing della riga riuscito: height=200");
            }
    }
    
    //chiudo il file 
    int ritorno= fclose(f);
    if(ritorno != 0){
        //loggo l'errore
        log_event(time(NULL),filename, "FILE_PARSING", "errore nella chiusura del file env.conf");

        //termino l'esecuzione con la macro
        PROTECT_CLOSE(ritorno, "errore nella chiusura del file env.conf");
    } else {
        //andato tutto ok quindi loggo il successo
        log_event(time(NULL),filename, "FILE_PARSING", "file env.conf chiuso con successo");
    }

    log_event(time(NULL),filename, "FILE_PARSING", "parsing completato con successo");

    return;
}

//funzione per creare la griglia 
int** crea_griglia(int width, int height){
    int** griglia= malloc(width*sizeof(int*));
    PROTECT_MEMORY_DINAMIC(griglia, "errore nella creazione della griglia nel file parse_env.c");

    for(int i=0; i<width; i++){
        griglia[i]=malloc(height*sizeof(int));
        PROTECT_MEMORY_DINAMIC(griglia[i], "errore nella creazione della griglia nel file parse_env.c");
    }
    return griglia;
}
