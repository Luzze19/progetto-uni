#ifndef RESCUERS_H
#define RESCUERS_H
/* - - - - - - - - - - - - - - - - -  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
typedef enum {
    IDLE,
    EN_ROUTE_TO_SCENE,
    ON_SCENE,
    RETURNING_TO_BASE,
} rescuer_status_t;

typedef struct{
    char* rescuer_type_name;
    int speed;
    int x;
    int y;
} rescuer_type_t;

typedef struct{
    int id;
    int x;
    int y;
    rescuer_type_t* rescuer;
    rescuer_status_t status;
}rescuer_digital_twin_t;

typedef struct{
    rescuer_type_t* array_rescuers;
    int size_array_rescuers;
    rescuer_digital_twin_t* array_twin_rescuer;
    int size_array_twin;
} arrays_rescuers;

/* - - - - - - - - - - - - - - - - -  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

arrays_rescuers arrays (char* filename);


#endif