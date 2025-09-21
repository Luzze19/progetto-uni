#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>       
#include <unistd.h>      
#include <dirent.h>      
#include <stdio.h>       
#include <stdlib.h>      
#include <string.h>      
#include <errno.h>       
#include <sys/types.h> 
#include <mqueue.h> 
#include <ctype.h> 
#include <time.h>
#include <signal.h>


#include "macro.h"


int main(int argc, char* argv[]){

    mqd_t mq= mq_open("/emergenze675938", O_WRONLY);
    PROTECT_MQ(mq, "errore nell'apertura della coda");

    if(argc == 5){
        //modalità emergenza senza file
        char* nome_emergenza= argv[1];
        int coord_x= atoi(argv[2]);
        int coord_y= atoi(argv[3]);
        int delay= atoi(argv[4]);

        //pausa prima di inviare il messaggio
        sleep(delay);

        char messaggio[100];
        long time_stamp= (long) time(NULL);
        snprintf(messaggio,sizeof(messaggio), "%s,%d,%d,%ld", nome_emergenza, coord_x, coord_y, time_stamp );

        int risultato=mq_send(mq, messaggio, strlen(messaggio)+1, 0);
        PROTECT_MQ(risultato, "errore nell'invio del messaggio nella coda");
        printf("Messaggio inviato: %s\n", messaggio);

    }

    if((argc==3) && strcmp(argv[1], "-f")==0){
        //modalità da file
        FILE* f;
        f=fopen(argv[2], "r");
        PROTECT_OPEN(f, "errore nell'apertura del file passato come argomento al client");

        char stringa_appoggio[256];
        char nome_emergenza[EMERGENCY_NAME_LENGTH];
        int coord_x;
        int coord_y;
        int delay;
        while (fgets(stringa_appoggio, sizeof(stringa_appoggio),f) != NULL){
            if(sscanf(stringa_appoggio, "%s %d %d %d", nome_emergenza, &coord_x, &coord_y, &delay)==4){
                char messaggio[100];

                //simulo il delay
                sleep(delay);

                long time_stamp= (long) time(NULL);
                snprintf(messaggio,sizeof(messaggio), "%.64s,%d,%d,%ld", nome_emergenza, coord_x, coord_y, time_stamp );

                //invio messaggio nella coda
                int risultato=mq_send(mq, messaggio, strlen(messaggio)+1, 0);
                PROTECT_MQ(risultato, "errore nell'invio del messaggio nella coda");
                printf("Messaggio inviato: %s\n", messaggio);

            }
        }

        int risultato=fclose(f);
        PROTECT_CLOSE(risultato, "errore nella chiusura del file passato come parametro");
    }
    
    //caso in cui l'utente sbaglia a digitare
    if (argc != 5 && !(argc == 3 && strcmp(argv[1], "-f") == 0)) {
        fprintf(stderr, "Uso: %s <nome> <x> <y> <delay>  oppure  %s -f <file>\n", argv[0], argv[0]);
        return 1;
    }

    int risultato=mq_close(mq);
    PROTECT_MQ(risultato, "errore nella chiusura della coda nel client");

    return 0;

}