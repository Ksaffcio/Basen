#include "kasjer.h"

/*
   Implementacja funkcji do obsługi kolejki klientów.
   Jeśli klient jest VIP, wstawiamy go na początek kolejki.
   Zwykły klient - na koniec.
*/

void add_client_to_queue(int clientPid, int isVIP)
{
    sem_op(0, -1); // chronimy kolejkę

    if(g_data->queueCount >= MAX_QUEUE_SIZE) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Kolejka pełna! Klient PID=%d odrzucony.", clientPid);
        log_event(msg);
        sem_op(0, +1);
        return;
    }

    if(isVIP) {
        // Wsuwamy na początek
        // Przesuwamy obecnych w dół
        for(int i = g_data->queueCount; i > 0; i--){
            g_data->queue[i] = g_data->queue[i-1];
        }
        g_data->queue[0] = clientPid;
        g_data->queueCount++;
    } else {
        // Zwykły - na koniec
        g_data->queue[g_data->queueCount] = clientPid;
        g_data->queueCount++;
    }

    sem_op(0, +1);
}

int pop_client_from_queue(void)
{
    int resultPid = -1;
    sem_op(0, -1);

    if(g_data->queueCount > 0) {
        resultPid = g_data->queue[0];
        // Przesuwamy kolejkę
        for(int i = 1; i < g_data->queueCount; i++){
            g_data->queue[i-1] = g_data->queue[i];
        }
        g_data->queueCount--;
    }

    sem_op(0, +1);
    return resultPid;
}

/*
   Główna pętla kasjera:
   - odczytuje klientów z kolejki,
   - sprawdza warunki regulaminu i wpuszcza do odpowiednich basenów,
   - uwzględnia limity i reguły (np. tylko pełnoletni do olympic, <=5 do kiddie, średnia <=40 w rekreacyjnym, itd.).
*/

void run_cashier_process(void)
{
    log_event("Kasjer startuje (proces kasjera)");

    while(1){
        // Sprawdzamy, czy obiekt otwarty
        if(!g_data->facilityOpen){
            // Gdy obiekt zamknięty, kasjer może czekać
            // (lub ewentualnie się zakończyć - zależy od logiki)
            sleep(1);
            continue;
        }

        int pid = pop_client_from_queue();
        if(pid == -1){
            // Brak klientów w kolejce
            sleep(1);
            continue;
        }

        // Znajdujemy klienta w tablicy
        sem_op(1, -1);
        int foundIndex = -1;
        for(int i=0; i<MAX_CLIENTS; i++){
            if(g_data->clients[i].pid == pid){
                foundIndex = i;
                break;
            }
        }
        if(foundIndex == -1){
            // Nie znaleziono – klient mógł już odejść
            sem_op(1, +1);
            continue;
        }

        ClientInfo *ci = &g_data->clients[foundIndex];

        // --- Logika doboru basenu zgodnie z regulaminem ---

        // 1. Tylko pełnoletni (>=18) do Olympic
        // 2. Dzieci <10 lat mają opiekuna (tutaj to jest "jedno" w polu hasGuardian=1)
        // 3. Dzieci <=5 lat -> tylko do KIDDIE_POOL
        // 4. Średnia wieku w RECREATIONAL_POOL <= 40
        // 5. Dziecko <3 lat -> musi mieć pampers (odnotowane w mustHavePampers)

        int chosenPool = -1;

        // Zdecydujmy, do którego basenu chcą wejść (w uproszczeniu):
        // - Jeśli wiek >= 18 -> Olympic (o ile dostępne / otwarte)
        // - else if wiek <= 5 -> KIDDIE_POOL
        // - else -> RECREATIONAL_POOL
        if(ci->age >= 18){
            chosenPool = OLYMPIC_POOL;
        } else if(ci->age <= 5){
            // Brodzik
            chosenPool = KIDDIE_POOL;
        } else {
            chosenPool = RECREATIONAL_POOL;
        }

        sem_op(2, -1); // chronimy stany basenów
        int cap = g_data->capacity[chosenPool];
        int cur = g_data->currentInPool[chosenPool];

        // Sprawdzenie, czy basen otwarty
        if(!g_data->poolOpen[chosenPool]){
            char ms[128];
            snprintf(ms, sizeof(ms),
                     "Kasjer: Basen %d jest zamkniety. PID=%d czeka...", chosenPool, pid);
            log_event(ms);
            sem_op(2, +1);
            sem_op(1, +1);
            continue;
        }

        // Jeśli basen rekreacyjny, sprawdźmy średnią <= 40
        if(chosenPool == RECREATIONAL_POOL){
            int newSum   = g_data->sumAgesInPool[RECREATIONAL_POOL] + ci->age;
            int newCount = g_data->currentInPool[RECREATIONAL_POOL] + 1;
            double avg   = (double)newSum / (double)newCount;
            if(avg > 40.0){
                char ms[128];
                snprintf(ms, sizeof(ms),
                         "Kasjer: Odrzucono klienta PID=%d (rekreacyjny > 40).", pid);
                log_event(ms);
                sem_op(2, +1);
                sem_op(1, +1);
                continue;
            }
        }

        // Sprawdzenie limitu
        if(cur >= cap){
            // Brak miejsca
            char ms[128];
            snprintf(ms, sizeof(ms),
                     "Kasjer: Brak miejsca w basenie %d (PID=%d).", chosenPool, pid);
            log_event(ms);
            sem_op(2, +1);
            sem_op(1, +1);
            continue;
        }

        // Wpuszczamy
        g_data->currentInPool[chosenPool]++;
        g_data->sumAgesInPool[chosenPool] += ci->age;
        ci->currentPool = chosenPool;
        ci->enterTime   = time(NULL);

        char ms[256];
        snprintf(ms, sizeof(ms),
                 "Kasjer: Wpuszczam PID=%d (age=%d) do basenu %d. (VIP=%d, freeTicket=%d, hasGuardian=%d, pampers=%d)",
                 pid, ci->age, chosenPool, ci->isVIP, ci->isTicketFree, ci->hasGuardian, ci->mustHavePampers);
        log_event(ms);

        sem_op(2, +1);
        sem_op(1, +1);

        // Krótka pauza
        usleep(200000);
    }

    // Ewentualnie exit(0);
}
