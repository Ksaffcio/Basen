#ifndef IPC_H
#define IPC_H

#include "basen.h"

/*
  Deklaracje funkcji do:
  - tworzenia/usuwania pamięci dzielonej,
  - tworzenia/usuwania semaforów,
  - operacji sem_op.
*/

union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
};

// Zmienne globalne
extern int shm_id;       
extern int sem_id;       
extern SharedData *g_data; 

// Funkcje pamięci dzielonej
int  create_shared_memory(void);
void remove_shared_memory(void);
void attach_shared_memory(void);
void detach_shared_memory(void);

// Funkcje semaforów
int  create_semaphores(void);
void remove_semaphores(void);
void sem_op(int semnum, int delta);

#endif
