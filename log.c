#include <threads.h>     
#include <stdio.h>       
#include <stdlib.h>      
#include <string.h>      
#include <errno.h>       
#include <stdarg.h>  

#include "log.h"
#include "macro.h"

mtx_t mutex_log;


void log_event(long timestamp, const char* id, const char* event, const char* message, ...){
    mtx_lock(&mutex_log); //accesso esclusivo al file di log

    FILE* f;
    f=fopen("operazioni.log", "a");
    if(f==NULL){
        perror("errore nell'apertura del file di log"); //non uso la macro perchè mi serve l'if per lasciare la mutex
        mtx_unlock(&mutex_log);
        return;
    }
    
    // Inizio gestione argomenti variabili
    va_list args;
    va_start(args, message);

    fprintf(f, "[%ld] [%s] [%s] ", timestamp, id, event);
 

    // Scrivo il messaggio formattato con gli argomenti variabili
    vfprintf(f, message, args);
    fprintf(f, "\n");

    va_end(args); // chiudo la lista

    int risultato=fclose(f);
    PROTECT_CLOSE(risultato, "errore nella chiusura file di log");

    mtx_unlock(&mutex_log);

}
