#ifndef EMERGENCY_H
#define EMERGENCY_H

#include "rescuers.h" //includo perchè rescuer_type_t è definito in rescuer.h

typedef struct{
    rescuer_type_t* type;
    int required_cont;
    int time_to_manage;
}rescuer_request_t;

typedef struct{
    short priority;
    char* emergency_descrition;
    rescuer_request_t* rescuers;
    int rescuers_request_numer;
}emergency_type_t;

typedef struct {
    emergency_type_t* array;
    int size;
} array_emergency_t;


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  - - - - - - - - - - - - */

int conta_soccorritori(char* stringa);

array_emergency_t crea_array_emergency(char* filename, arrays_rescuers array_soccorritori);



#endif