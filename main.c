#include "basen.h"
#include "ipc.h"
#include "kasjer.h"
#include "ratownik.h"
#include "klient.h"

/*
  PLIK GŁÓWNY:
   - Tworzy zasoby IPC (pamięć dzieloną, semafory)
   - Uruchamia proces kasjera i ratowników
   - Uruchamia wątek generatora klientów
   - Wyświetla co pewien czas stan basenu
   - Po upływie (Tk - Tp) sekund (lub nigdy, jeśli Tk=-1),
     kończy symulację, usuwa zasoby IPC.
*/

// log_event => zapis do pliku basen.log
void log_event(const char *msg)
{
    int fd = open("basen.log", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if(fd == -1){
        perror("open(basen.log)");
        return;
    }
    if(write(fd, msg, strlen(msg)) == -1){
        perror("write to basen.log");
    }
    if(write(fd, "\n", 1) == -1){
        perror("write newline to basen.log");
    }
    close(fd);
}

// Prosta walidacja parametrów
int validate_parameters(int Tp, int Tk, int X1, int X2, int X3)
{
    if(Tp < 0) return 0;
    if(Tk < -1) return 0; // -1 => nieskończony
    if(X1 < 0 || X2 < 0 || X3 < 0) return 0;
    if(Tk != -1 && Tp >= Tk) return 0;
    return 1;
}

// WĄTEK GENERATORA KLIENTÓW
static void* client_generator_thread(void *arg)
{
    srand(time(NULL) ^ getpid());

    while(1){
        // Szukamy wolnego miejsca w tablicy
        sem_op(1, -1);
        int freeIdx = -1;
        for(int i=0; i<MAX_CLIENTS; i++){
            if(g_data->clients[i].pid == 0){
                freeIdx = i;
                break;
            }
        }
        if(freeIdx == -1){
            // brak slotów
            sem_op(1, +1);
            sleep(1);
            continue;
        }

        // Losujemy parametry klienta
        int age = rand()%70 + 1; // 1..70
        int isVIP = ( (rand()%100) < VIP_PROB ) ? 1 : 0;
        // Czas biletu 10..30 sek, z szansą 1/10 na -1
        int ticketTime = 10 + (rand()%21); 
        if((rand()%10) == 0){
            ticketTime = -1; // dożywotni
        }

        // Dzieci <10 => free ticket, hasGuardian=1, <3 => pampers=1
        int isFree = (age<10)?1:0;
        int guard  = (age<10)?1:0;
        int pamp   = (age<3)?1:0;

        // Tworzymy proces potomny klienta
        pid_t pid = fork();
        if(pid == -1){
            perror("fork klient");
            sem_op(1, +1);
            break;
        }
        if(pid == 0){
            // PROCES KLIENTA
            attach_shared_memory();
            run_client_process(freeIdx);
            exit(0);
        } else {
            // PROCES RODZICA
            g_data->clients[freeIdx].pid             = pid;
            g_data->clients[freeIdx].age             = age;
            g_data->clients[freeIdx].isVIP           = isVIP;
            g_data->clients[freeIdx].ticketTime      = ticketTime;
            g_data->clients[freeIdx].currentPool     = -1;
            g_data->clients[freeIdx].enterTime       = 0;
            g_data->clients[freeIdx].isTicketFree    = isFree;
            g_data->clients[freeIdx].hasGuardian     = guard;
            g_data->clients[freeIdx].mustHavePampers = pamp;

            // Log
            char msg[256];
            snprintf(msg,sizeof(msg),
                "Generator: Tworze nowego klienta PID=%d (age=%d, VIP=%d, ticketTime=%d, free=%d, guardian=%d, pampers=%d)",
                pid, age, isVIP, ticketTime, isFree, guard, pamp);
            log_event(msg);

            // Dodajemy do kolejki kasjera
            add_client_to_queue(pid, isVIP);
            sem_op(1, +1);
        }

        // Kolejny klient za 2 sek
        sleep(2);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if(argc < 6){
        fprintf(stderr, "Uzycie: %s <Tp> <Tk> <X1> <X2> <X3>\n", argv[0]);
        exit(1);
    }

    int Tp = atoi(argv[1]);
    int Tk = atoi(argv[2]);
    int X1 = atoi(argv[3]);
    int X2 = atoi(argv[4]);
    int X3 = atoi(argv[5]);

    if(!validate_parameters(Tp,Tk,X1,X2,X3)){
        fprintf(stderr,"Bledne parametry!\n");
        exit(2);
    }

    // Tworzymy plik do ftok()
    int fd = creat(IPC_KEY_FILE, S_IRUSR|S_IWUSR);
    if(fd == -1 && errno != EEXIST){
        perror("creat ipc_key_file");
        exit(3);
    }
    close(fd);

    // Pamięć dzielona
    if(create_shared_memory() == -1){
        exit(4);
    }
    attach_shared_memory();
    if(!g_data){
        remove_shared_memory();
        exit(5);
    }

    // Semafory
    if(create_semaphores() == -1){
        detach_shared_memory();
        remove_shared_memory();
        exit(6);
    }

    // Inicjalizacja sharedData
    memset(g_data, 0, sizeof(SharedData));
    g_data->openTime = Tp;
    g_data->closeTime= Tk;
    g_data->capacity[0] = X1;
    g_data->capacity[1] = X2;
    g_data->capacity[2] = X3;
    g_data->poolOpen[0] = 1;
    g_data->poolOpen[1] = 1;
    g_data->poolOpen[2] = 1;
    g_data->facilityOpen= 1;

    log_event("Rozpoczynam symulację basenów...");

    // Uruchomienie procesu kasjera
    pid_t cashier = fork();
    if(cashier == -1){
        perror("fork kasjer");
        remove_semaphores();
        detach_shared_memory();
        remove_shared_memory();
        exit(7);
    }
    if(cashier == 0){
        // proces potomny
        run_cashier_process();
        exit(0);
    }

    // Procesy ratowników
    pid_t lifeguards[POOLS_COUNT];
    for(int i=0; i<POOLS_COUNT; i++){
        lifeguards[i] = fork();
        if(lifeguards[i] == -1){
            perror("fork ratownik");
            exit(8);
        }
        if(lifeguards[i] == 0){
            run_lifeguard_process(i);
            exit(0);
        }
    }

    // Wątek generatora klientów
    pthread_t genThread;
    pthread_create(&genThread, NULL, client_generator_thread, NULL);
    pthread_detach(genThread);

    // "UI" - co pewien czas
    time_t startTime = time(NULL);
    while(1){
        if(Tk == -1){
            // Symulacja nieskończona => brak break
        } else {
            time_t now = time(NULL);
            if((now - startTime) >= (Tk - Tp)){
                // Zamykamy
                sem_op(2, -1);
                g_data->facilityOpen = 0;
                g_data->poolOpen[0]  = 0;
                g_data->poolOpen[1]  = 0;
                g_data->poolOpen[2]  = 0;
                sem_op(2, +1);
                break;
            }
        }

        sem_op(2, -1);
        printf("\n=== STAN BASENU ===\n");
        printf("Kolejka: %d\n", g_data->queueCount);
        for(int i=0; i<POOLS_COUNT; i++){
            printf("Basen %d: %d/%d [open=%d]\n",
                   i,
                   g_data->currentInPool[i],
                   g_data->capacity[i],
                   g_data->poolOpen[i]);
        }
        printf("Obiekt: %s\n", g_data->facilityOpen ? "otwarty":"zamkniety");
        sem_op(2, +1);

        sleep(3);

        if(Tk == -1){
            // brak break => pętla nieskończona
        }
    }

    printf("Zamykamy obiekt.\n");
    log_event("Zamykamy obiekt - koniec symulacji.");

    // Zabijamy kasjera
    kill(cashier, SIGTERM);
    // Zabijamy ratowników
    for(int i=0; i<POOLS_COUNT; i++){
        kill(lifeguards[i], SIGTERM);
    }

    // Zabijamy klientów
    sem_op(1, -1);
    for(int i=0; i<MAX_CLIENTS; i++){
        if(g_data->clients[i].pid != 0){
            kill(g_data->clients[i].pid, SIGTERM);
        }
    }
    sem_op(1, +1);

    // Czekamy (opcjonalnie)
    wait(NULL);

    remove_semaphores();
    detach_shared_memory();
    remove_shared_memory();
    unlink(IPC_KEY_FILE);

    printf("Koniec symulacji.\n");
    return 0;
}
