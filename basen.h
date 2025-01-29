#ifndef BASEN_H
#define BASEN_H

/*
  GŁÓWNY PLIK NAGŁÓWKOWY PROJEKTU
  --------------------------------
  Zawiera:
  - standardowe include'y
  - definicje stałych (np. liczbę basenów, rozmiar kolejki)
  - deklaracje struktur kluczowych (SharedData, ClientInfo)
  - prototypy funkcji pomocniczych (log_event, validate_parameters)
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <sys/wait.h>

// Identyfikatory trzech basenów
#define OLYMPIC_POOL      0
#define RECREATIONAL_POOL 1
#define KIDDIE_POOL       2
#define POOLS_COUNT       3

// Limity
#define MAX_QUEUE_SIZE 50   // maks. liczba osób w kolejce
#define MAX_CLIENTS 100     // maks. liczba "procesów klientów" w systemie
#define VIP_PROB 20         // prawdopodobieństwo (%) bycia VIP

// Sygnały użytkownika (ratownik -> klient)
#define SIG_EMPTY_POOL  SIGUSR1
#define SIG_REOPEN_POOL SIGUSR2

/*
  Struktura opisująca pojedynczego klienta.
  Jeśli jest to dziecko <10 lat, pole hasGuardian=1 oznacza
  "dziecko i opiekun" w jednym procesie.
*/
typedef struct {
    int  pid;             // PID procesu klienta (0 => slot wolny)
    int  age;             // wiek
    int  isVIP;           // flaga VIP
    int  ticketTime;      // czas ważności biletu (sekundy); -1 => "nieskończony"
    int  currentPool;     // basen, w którym obecnie przebywa (-1 => nie w basenie)
    time_t enterTime;     // czas wejścia do basenu
    int  isTicketFree;    // 1 => bilet darmowy (dzieci <10 lat)
    int  hasGuardian;     // 1 => to jest dziecko z opiekunem
    int  mustHavePampers; // 1 => wiek <3 => musi mieć pampers
} ClientInfo;

/*
  Struktura pamięci dzielonej, w której przechowujemy wspólny stan.
*/
typedef struct {
    // Parametry z wejścia
    int openTime;              // Tp
    int closeTime;             // Tk  (=-1 => symulacja nieskończona)
    int capacity[POOLS_COUNT]; // X1, X2, X3 => limity basenów

    // Kolejka klientów (przechowujemy PIDy)
    int queue[MAX_QUEUE_SIZE];
    int queueCount;

    // Tablica klientów (maks. MAX_CLIENTS)
    ClientInfo clients[MAX_CLIENTS];

    // Stan każdego basenu: ilu obecnie pływa, sumaryczny wiek (do średniej)
    int currentInPool[POOLS_COUNT];
    int sumAgesInPool[POOLS_COUNT];

    // Flagi otwarcia basenów
    int poolOpen[POOLS_COUNT]; // 1 => otwarty, 0 => zamknięty
    int facilityOpen;          // 1 => obiekt otwarty, 0 => zamknięty
} SharedData;

// Plik używany do ftok()
#define IPC_KEY_FILE "ipc_key_file"
#define IPC_KEY_ID   0x1234

// Deklaracje funkcji pomocniczych:

/*
  log_event:
    - Zapisuje komunikat (msg) do pliku basen.log, dopisując nową linię.
*/
void log_event(const char *msg);

/*
  validate_parameters:
    - Sprawdza poprawność parametrów (Tp, Tk, X1, X2, X3).
    - Zwraca 1 => poprawne, 0 => niepoprawne.
*/
int validate_parameters(int Tp, int Tk, int X1, int X2, int X3);

#endif
