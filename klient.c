#include "klient.h"

static volatile int needToExitPool = 0;

// Obsługa sygnału SIG_EMPTY_POOL -> klient ma natychmiast opuścić basen
static void client_handle_empty_pool(int signo)
{
    (void)signo;
    needToExitPool = 1;
    printf("[Klient %d] Otrzymalem SIG_EMPTY_POOL -> opuszczam basen.\n", getpid());
}

// Obsługa sygnału SIG_REOPEN_POOL -> basen jest znów dostępny
static void client_handle_reopen_pool(int signo)
{
    (void)signo;
    printf("[Klient %d] Otrzymalem SIG_REOPEN_POOL -> basen ponownie otwarty.\n", getpid());
}

void run_client_process(int clientIndex)
{
    // Ustawienia sygnałów
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = client_handle_empty_pool;
    sigaction(SIG_EMPTY_POOL, &sa, NULL);

    sa.sa_handler = client_handle_reopen_pool;
    sigaction(SIG_REOPEN_POOL, &sa, NULL);

    // Główny loop
    while(1) {
        sem_op(1, -1);
        if(g_data->clients[clientIndex].pid == 0) {
            // Zwolniony slot -> kończymy
            sem_op(1, +1);
            break;
        }

        ClientInfo *ci = &g_data->clients[clientIndex];
        time_t now = time(NULL);

        // Sprawdzamy czas biletu (jeśli currentPool != -1)
        if(ci->currentPool != -1) {
            // Tylko jeśli ticketTime > 0 (czyli != darmowy/infinite)
            // Tutaj: jeżeli ticketTime == -1 -> pływa w nieskończoność
            if(ci->ticketTime != -1) {
                // Liczymy, czy minął
                if((now - ci->enterTime) >= ci->ticketTime) {
                    printf("[Klient %d] Koniec biletu -> wychodzę z basenu.\n", ci->pid);

                    // Zmniejszamy liczbę osób w basenie
                    sem_op(2, -1);
                    if(ci->currentPool != -1){
                        g_data->currentInPool[ci->currentPool]--;
                        g_data->sumAgesInPool[ci->currentPool] -= ci->age;
                        ci->currentPool = -1;
                    }
                    sem_op(2, +1);

                    // Zwalniamy slot
                    ci->pid = 0;
                    sem_op(1, +1);
                    break;
                }
            }
        }
        sem_op(1, +1);

        // Jeśli otrzymaliśmy sygnał opuszczenia
        if(needToExitPool) {
            sem_op(1, -1);
            // Opuszczamy basen (o ile w nim jesteśmy)
            if(ci->currentPool != -1) {
                sem_op(2, -1);
                g_data->currentInPool[ci->currentPool]--;
                g_data->sumAgesInPool[ci->currentPool] -= ci->age;
                ci->currentPool = -1;
                sem_op(2, +1);
            }
            sem_op(1, +1);

            // Symulacja, że stoimy obok 2s
            sleep(2);
            needToExitPool = 0;
        }

        // Symulacja pływania
        sleep(1);
    }

    exit(0);
}
