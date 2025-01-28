#include "basen.h"
#include "ipc.h"
#include "kasjer.h"
#include "ratownik.h"
#include "klient.h"
#include <sys/wait.h>

/*
   Tutaj:
   - definicja log_event (z basen.h)
   - definicja validate_parameters
   - generator klientów (w osobnym wątku)
   - funkcja main (tworzy pamięć, semafory, uruchamia procesy, prosty interfejs)
*/

// Implementacja log_event
void log_event(const char *msg)
{
    // Proste logowanie do pliku "basen.log"
    int fd = open("basen.log", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if(fd == -1){
        perror("open(basen.log)");
        return;
    }
    if(write(fd, msg, strlen(msg)) == -1){
        perror("write to basen.log");
    }
    // Nowa linia
    if(write(fd, "\n", 1) == -1){
        perror("write newline to basen.log");
    }
    close(fd);
}

// Walidacja parametrów
int validate_parameters(int Tp, int Tk, int X1, int X2, int X3)
{
    if(Tp < 0 || Tk < 0 || X1 < 0 || X2 < 0 || X3 < 0) return 0;
    if(Tp >= Tk) return 0;
    return 1;
}

// Generator klientów (wątek)
static void* client_generator_thread(void *arg)
{
    srand(time(NULL) ^ getpid());

    while(1){
        // Szukamy wolnego miejsca w tablicy clients
        sem_op(1, -1);
        int freeIndex = -1;
        for(int i=0; i<MAX_CLIENTS; i++){
            if(g_data->clients[i].pid == 0){
                freeIndex = i;
                break;
            }
        }
        if(freeIndex == -1){
            // Brak wolnych slotów
            sem_op(1, +1);
            sleep(1);
            continue;
        }

        // Generujemy dane nowego klienta
        int age = rand() % 70 + 1;   // 1..70
        int isVIP = ((rand()%100) < VIP_PROB) ? 1 : 0;
        int ticketTime = 10 + rand() % 20; // bilet 10-30 sekund

        // Tworzymy proces potomny
        pid_t pid = fork();
        if(pid == -1){
            perror("fork klient");
            sem_op(1, +1);
            break;
        }
        if(pid == 0){
            // Proces klienta
            attach_shared_memory(); 
            run_client_process(freeIndex);
            // Nie powinno dojść do powrotu
            exit(0);
        } else {
            // Proces rodzica
            g_data->clients[freeIndex].pid        = pid;
            g_data->clients[freeIndex].age        = age;
            g_data->clients[freeIndex].isVIP      = isVIP;
            g_data->clients[freeIndex].ticketTime = ticketTime;
            g_data->clients[freeIndex].currentPool= -1;
            g_data->clients[freeIndex].enterTime  = 0;

            // Dodajemy do kolejki kasjera
            add_client_to_queue(pid, isVIP);
            sem_op(1, +1); 
        }

        // Czekamy 2 sekundy i generujemy kolejnego klienta
        sleep(2);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    if(argc < 6){
        fprintf(stderr, "Użycie: %s <Tp> <Tk> <X1> <X2> <X3>\n", argv[0]);
        exit(1);
    }

    // Odczyt parametrów
    int Tp = atoi(argv[1]);
    int Tk = atoi(argv[2]);
    int X1 = atoi(argv[3]);
    int X2 = atoi(argv[4]);
    int X3 = atoi(argv[5]);

    // Walidacja
    if(!validate_parameters(Tp, Tk, X1, X2, X3)){
        fprintf(stderr, "Niepoprawne parametry!\n");
        exit(2);
    }

    // Tworzymy plik do ftok (jeśli nie istnieje)
    int fd = creat(IPC_KEY_FILE, S_IRUSR | S_IWUSR);
    if(fd == -1 && errno != EEXIST){
        perror("creat ipc_key_file");
        exit(3);
    }
    close(fd);

    // Tworzymy pamięć dzieloną
    if(create_shared_memory() == -1){
        exit(4);
    }
    attach_shared_memory();
    if(!g_data){
        remove_shared_memory();
        exit(5);
    }

    // Tworzymy semafory
    if(create_semaphores() == -1){
        detach_shared_memory();
        remove_shared_memory();
        exit(6);
    }

    // Inicjalizujemy strukturę w pamięci dzielonej
    memset(g_data, 0, sizeof(SharedData));
    g_data->openTime      = Tp;
    g_data->closeTime     = Tk;
    g_data->capacity[0]   = X1;
    g_data->capacity[1]   = X2;
    g_data->capacity[2]   = X3;
    g_data->poolOpen[0]   = 1;
    g_data->poolOpen[1]   = 1;
    g_data->poolOpen[2]   = 1;
    g_data->facilityOpen  = 1;

    log_event("Rozpoczynam symulację basenów...");

    // Uruchamiamy proces kasjera
    pid_t pid_cashier = fork();
    if(pid_cashier == -1){
        perror("fork kasjer");
        remove_semaphores();
        detach_shared_memory();
        remove_shared_memory();
        exit(7);
    }
    if(pid_cashier == 0){
        run_cashier_process();
        exit(0);
    }

    // Uruchamiamy procesy ratowników (po jednym dla każdego basenu)
    pid_t lifeguards[POOLS_COUNT];
    for(int i=0; i<POOLS_COUNT; i++){
        lifeguards[i] = fork();
        if(lifeguards[i] == -1){
            perror("fork ratownik");
            // (ew. sprzątanie i exit)
            exit(8);
        }
        if(lifeguards[i] == 0){
            run_lifeguard_process(i);
            exit(0);
        }
    }

    // Uruchamiamy wątek generatora klientów
    pthread_t genThread;
    pthread_create(&genThread, NULL, client_generator_thread, NULL);
    // Nie potrzebujemy join – odpinamy
    pthread_detach(genThread);

    // Proste "UI" w pętli – pokazuje stan do czasu Tk
    time_t startTime = time(NULL);
    while(1){
        time_t now = time(NULL);
        if( (now - startTime) >= (Tk - Tp) ){
            // Zamykamy obiekt
            sem_op(2, -1);
            g_data->facilityOpen = 0;
            g_data->poolOpen[0]  = 0;
            g_data->poolOpen[1]  = 0;
            g_data->poolOpen[2]  = 0;
            sem_op(2, +1);
            break;
        }

        // Wyświetlamy stan
        sem_op(2, -1);
        printf("\n===== STAN BASENU =====\n");
        printf("Kolejka: %d osób\n", g_data->queueCount);
        for(int i=0; i<POOLS_COUNT; i++){
            printf("Basen %d: %d/%d osób (open=%d)\n",
                   i,
                   g_data->currentInPool[i],
                   g_data->capacity[i],
                   g_data->poolOpen[i]);
        }
        printf("Obiekt: %s\n", g_data->facilityOpen ? "otwarty" : "zamknięty");
        sem_op(2, +1);

        sleep(2);
    }

    printf("Zamykamy obiekt (wymiana wody) i kończymy symulację.\n");

    // Zabijamy proces kasjera
    kill(pid_cashier, SIGTERM);

    // Zabijamy procesy ratowników
    for(int i=0; i<POOLS_COUNT; i++){
        kill(lifeguards[i], SIGTERM);
    }

    // Zabijamy wszystkich klientów
    sem_op(1, -1);
    for(int i=0; i<MAX_CLIENTS; i++){
        if(g_data->clients[i].pid != 0){
            kill(g_data->clients[i].pid, SIGTERM);
        }
    }
    sem_op(1, +1);

    // Czekamy na potomków (można w pętli, tutaj w uproszczeniu jednorazowo)
    wait(NULL);

    // Usuwamy semafory, pamięć dzieloną i plik do ftok
    remove_semaphores();
    detach_shared_memory();
    remove_shared_memory();
    unlink(IPC_KEY_FILE);

    printf("Koniec symulacji.\n");
    return 0;
}
