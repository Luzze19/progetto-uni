#ifndef ENV_H
#define ENV_H

extern char nome_queue[50];
extern int width;
extern int height;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void leggi_e_disponi(char* filename);

int** crea_griglia(int width, int height);


#endif


