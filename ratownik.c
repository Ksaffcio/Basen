#include "ratownik.h"

// Obsługa sygnału SIG_EMPTY_POOL (w procesie ratownika)
static void lifeguard_empty_pool(int signo)
{
    (void)signo;
    // Dodatkowy log
    printf("[Ratownik] Otrzymano sygnal SIG_EMPTY_POOL (ratownik) - nic lokalnie.\n");
}

// Obsługa sygnału SIG_REOPEN_POOL (w procesie ratownika)
static void lifeguard_reopen_pool(int signo)
{
    (void)signo;
    printf("[Ratownik] Otrzymano sygnal SIG_REOPEN_POOL (ratownik) - nic lokalnie.\n");
}

void run_lifeguard_process(int poolId)
{
    // Ustawiamy obsługę sygnałów w procesie ratownika
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = lifeguard_empty_pool;
    sigaction(SIG_EMPTY_POOL, &sa, NULL);

    sa.sa_handler = lifeguard_reopen_pool;
    sigaction(SIG_REOPEN_POOL, &sa, NULL);

    char buf[64];
    snprintf(buf, sizeof(buf), "Ratownik %d startuje (proces ratownika).", poolId);
    log_event(buf);

    // Ratownik co pewien czas wysyła sygnał do klientów w danym basenie (opróżnienie),
    // potem zamyka basen, potem otwiera, wysyła sygnał ponownego otwarcia.
    while(1) {
        // Co np. 10 sekund wymuszamy ewakuację:
        sleep(10);

        sem_op(2, -1);
        if(g_data->poolOpen[poolId]) {
            // Opróżniamy
            for(int i=0; i<MAX_CLIENTS; i++){
                if(g_data->clients[i].pid != 0 && g_data->clients[i].currentPool == poolId){
                    kill(g_data->clients[i].pid, SIG_EMPTY_POOL);
                }
            }
            g_data->poolOpen[poolId] = 0;

            char msg[128];
            snprintf(msg, sizeof(msg),
                     "[Ratownik %d] Wydano sygnał opróżnienia (SIG_EMPTY_POOL). Basen zamknięty.", poolId);
            log_event(msg);
        }
        sem_op(2, +1);

        // Po 5 sekundach otwieramy ponownie
        sleep(5);

        sem_op(2, -1);
        g_data->poolOpen[poolId] = 1;
        // Informujemy wszystkich klientów, że basen ponownie otwarty
        for(int i=0; i<MAX_CLIENTS; i++){
            if(g_data->clients[i].pid != 0){
                kill(g_data->clients[i].pid, SIG_REOPEN_POOL);
            }
        }

        char msg[128];
        snprintf(msg, sizeof(msg),
                 "[Ratownik %d] Basen otwarty ponownie (SIG_REOPEN_POOL wysłany).", poolId);
        log_event(msg);

        sem_op(2, +1);

        sleep(2);
    }

    // exit(0) ewentualnie
}
