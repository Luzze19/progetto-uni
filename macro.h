#ifndef MACRO_H
#define MACRO_H

#define PROTECT_OPEN(r, e) do { if(r  == NULL) { perror(e); exit(EXIT_FAILURE); } } while(0)

#define PROTECT_CLOSE(r, e) do { if(r  != 0 ) { perror(e); exit(EXIT_FAILURE); } } while(0)


#define PROTECT_MEMORY_DINAMIC(r,e) do {if (r==NULL){ perror(e); exit(EXIT_FAILURE); } } while(0)

#define PROTECT_MQ(r,e) do {if (r==-1){ perror(e); exit(EXIT_FAILURE); } } while(0)

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#define SIZE_RIGA_LETTA 256

#define EMERGENCY_NAME_LENGTH 64

#endif