#include <unistd.h>      
#include <stdio.h>       
#include <stdlib.h>      
#include <string.h>      
#include <errno.h>       
#include <ctype.h> 

#include "rescuers.h"
#include "macro.h"
#include "log.h"

arrays_rescuers arrays (char* filename){
    rescuer_type_t* array_rescuers= NULL;  // mi creo i due array
    int size_array_rescuers=0;
    rescuer_digital_twin_t* array_twin_rescuers= NULL;
    int size_array_twins=0;


    FILE* f;
    f=fopen(filename, "r"); //apro il file 
    if(f==NULL){
        //loggo l'errore
        log_event(time(NULL),filename, "FILE_PARSING", "errore nell'apertura del file rescuers.conf");

        //termino l'esecuzione con la macro
        PROTECT_OPEN(f, "errore nell'apertura del file rescuers.conf");
    } else {
        //andato tutto ok quindi loggo il successo
        log_event(time(NULL),filename, "FILE_PARSING", "file rescuers.conf aperto con successo");
    }
    char riga_letta[SIZE_RIGA_LETTA];
    char nome[20];
    int unità;
    int velocità;
    int x;
    int y;

    while(fgets(riga_letta, sizeof(riga_letta), f)!= NULL){
        
        //separo le variabili che mi servono
        if((strlen(riga_letta) > 1) && sscanf(riga_letta, "[%[^]]] [%d] [%d] [%d;%d]", nome, &unità, &velocità, &x, &y)==5){

            //scrivo nel file di log le informazioni lette
            log_event(
                time(NULL),filename, "FILE_PARSING",
                "Parsing riuscito. Nome: %s, Unità: %d, Velocità: %d, Coordinate: [%d, %d]", nome, unità, velocità, x, y
            );
            
            
            //alloco il tipo del soccorritore a array_rescuers
            rescuer_type_t* tmp= realloc(array_rescuers, (size_array_rescuers+ 1) * sizeof(rescuer_type_t));
            PROTECT_MEMORY_DINAMIC(tmp, "errore allocazione variabile di appoggio in parse_rescuers.c");
            array_rescuers= tmp;

            array_rescuers[size_array_rescuers].rescuer_type_name= strdup(nome);
            PROTECT_MEMORY_DINAMIC(array_rescuers[size_array_rescuers].rescuer_type_name, "errore nella duplicazione del nome in parse_rescuers.c");
            array_rescuers[size_array_rescuers].speed= velocità;
            array_rescuers[size_array_rescuers].x= x;
            array_rescuers[size_array_rescuers].y= y;

            //creo i gemelli digitali
            for( int i=0; i< unità; i++){
                rescuer_digital_twin_t* tmp= realloc(array_twin_rescuers, (size_array_twins+1)*sizeof(rescuer_digital_twin_t));
                PROTECT_MEMORY_DINAMIC(tmp, "errore allocazione variabile di appoggio dei twins in parse_rescuers.c");
                array_twin_rescuers= tmp;

                array_twin_rescuers[size_array_twins].id= size_array_twins;
                array_twin_rescuers[size_array_twins].status= IDLE;
                array_twin_rescuers[size_array_twins].x=x;
                array_twin_rescuers[size_array_twins].y=y;
                array_twin_rescuers[size_array_twins].rescuer= NULL; //li creo momentaneamente a NULL poichè sennò con la
                size_array_twins++;                       //realloc potrei avere problemi di dangling reference
            } 
        } else {
            log_event(time(NULL),filename, "FILE_PARSING", "riga mal formattata");
            continue;
        }
        size_array_rescuers++;
    }
    int ritorno= fclose(f); //chiudo il file 
    if(ritorno != 0){
        //loggo l'errore
        log_event(time(NULL),filename, "FILE_PARSING", "errore nella chiusura del file rescuers.conf");

        //termino l'esecuzione con la macro
        PROTECT_CLOSE(ritorno, "errore nella chiusura del file rescuers.conf");
    } else {
        //andato tutto ok quindi loggo il successo
        log_event(time(NULL),filename, "FILE_PARSING", "file rescuers.conf chiuso con successo");
    }
    
    //aggiungo il campo che avevo impostato a null;
    int index=0;
    for (int i=0; i<size_array_twins; i++){
        if((array_twin_rescuers[i].x != array_rescuers[index].x) || (array_twin_rescuers[i].y != array_rescuers[index].y)){
            index++; //confronto indice x ed y per vedere se sono lo stesso ed allocare al gemello digitale il tipo giusto di soccorritore
        }
        array_twin_rescuers[i].rescuer= &array_rescuers[index];
        
    }
    
    //creo la struttura finale da ritornare
    arrays_rescuers array_da_ritornare;
    array_da_ritornare.array_rescuers= array_rescuers;
    array_da_ritornare.size_array_rescuers= size_array_rescuers;
    array_da_ritornare.array_twin_rescuer= array_twin_rescuers;
    array_da_ritornare.size_array_twin= size_array_twins;

    log_event(time(NULL),filename, "FILE_PARSING", "parsing completato con successo");

    return array_da_ritornare;
    
}