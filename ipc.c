#include "ipc.h"

// Definicje zmiennych globalnych (dla wszystkich plików)
int shm_id  = -1;
int sem_id  = -1;
SharedData *g_data = NULL;

/*
   Tworzenie pamięci dzielonej
*/
int create_shared_memory(void)
{
    key_t key = ftok(IPC_KEY_FILE, IPC_KEY_ID);
    if(key == -1){
        perror("ftok() for shared memory");
        return -1;
    }

    // Pamięć dzielona - tylko właściciel może r/w
    shm_id = shmget(key, sizeof(SharedData), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if(shm_id == -1){
        perror("shmget");
        return -1;
    }
    return shm_id;
}

/*
   Usuwanie pamięci dzielonej
*/
void remove_shared_memory(void)
{
    if(shm_id != -1){
        if(shmctl(shm_id, IPC_RMID, NULL) == -1){
            perror("shmctl(IPC_RMID)");
        }
    }
}

/*
   Dołączanie do przestrzeni procesu
*/
void attach_shared_memory(void)
{
    if(shm_id == -1){
        fprintf(stderr, "attach_shared_memory: shm_id invalid\n");
        return;
    }
    g_data = (SharedData*) shmat(shm_id, NULL, 0);
    if(g_data == (void*)-1){
        perror("shmat");
        g_data = NULL;
    }
}

/*
   Odłączanie pamięci dzielonej
*/
void detach_shared_memory(void)
{
    if(g_data != NULL){
        if(shmdt((void*)g_data) == -1){
            perror("shmdt");
        }
        g_data = NULL;
    }
}

/*
   Tworzenie zestawu semaforów
*/
int create_semaphores(void)
{
    key_t key = ftok(IPC_KEY_FILE, IPC_KEY_ID + 1);
    if(key == -1){
        perror("ftok() for semaphores");
        return -1;
    }

    // Tworzymy 3 semafory (0 - kolejka, 1 - clients, 2 - baseny)
    sem_id = semget(key, 3, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if(sem_id == -1){
        perror("semget");
        return -1;
    }

    // Inicjalizujemy na 1
    union semun arg;
    unsigned short vals[3] = {1, 1, 1};
    arg.array = vals;
    if(semctl(sem_id, 0, SETALL, arg) == -1){
        perror("semctl(SETALL)");
        return -1;
    }

    return sem_id;
}

/*
   Usuwanie semaforów
*/
void remove_semaphores(void)
{
    if(sem_id != -1){
        if(semctl(sem_id, 0, IPC_RMID) == -1){
            perror("semctl(IPC_RMID)");
        }
    }
}

/*
   Operacja na semaforze
*/
void sem_op(int semnum, int delta)
{
    struct sembuf sb;
    sb.sem_num = semnum;
    sb.sem_op  = delta;
    sb.sem_flg = 0;

    if(semop(sem_id, &sb, 1) == -1){
        perror("semop");
    }
}
