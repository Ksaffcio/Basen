#ifndef BASEN_H
#define BASEN_H

/*
  Główny plik nagłówkowy: zawiera:
  - standardowe includy,
  - definicje stałych i makr,
  - definicje struktur danych (SharedData, ClientInfo),
  - deklaracje funkcji log_event(), validate_parameters().
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
#include <sys/wait.h> // <- dla wait()

// ----- Stałe ----- //

// Maksymalny rozmiar kolejki
#define MAX_QUEUE_SIZE 50
// Maksymalna liczba klientów
#define MAX_CLIENTS 100
// Prawdopodobieństwo (w %) bycia VIP
#define VIP_PROB 20

// Identyfikatory basenów
#define OLYMPIC_POOL      0
#define RECREATIONAL_POOL 1
#define KIDDIE_POOL       2
#define POOLS_COUNT       3

// Specjalne sygnały (ratownik -> klient)
#define SIG_EMPTY_POOL  SIGUSR1
#define SIG_REOPEN_POOL SIGUSR2

// ----- Struktury ----- //

// Struktura przechowująca info o kliencie
typedef struct {
    int  pid;             // PID procesu (0 oznacza, że slot wolny)
    int  age;             // wiek
    int  isVIP;           // 0 - zwykły, 1 - VIP
    int  ticketTime;      // czas ważności biletu (sekundy)
    int  currentPool;     // aktualny basen (-1 jeśli nie w basenie)
    time_t enterTime;     // czas wejścia do basenu
    int  hasGuardian;     // 1 - to dziecko <10 lat z opiekunem w jednym procesie
    int  mustHavePampers; // 1 - jeśli wiek <3
    int  isTicketFree;    // 1 - jeśli nie płaci (wiek<10)
} ClientInfo;

// Struktura pamięci dzielonej
typedef struct {
    // Parametry z wejścia
    int openTime;              // Tp
    int closeTime;             // Tk (jeśli == -1 -> symulacja nieskończona)
    int capacity[POOLS_COUNT]; // X1, X2, X3 (max. osób na basen)

    // Kolejka oczekujących (PID-y)
    int queue[MAX_QUEUE_SIZE];
    int queueCount;

    // Tablica klientów
    ClientInfo clients[MAX_CLIENTS];

    // Stan basenów
    int currentInPool[POOLS_COUNT]; // ile osób aktualnie
    int sumAgesInPool[POOLS_COUNT]; // sumaryczny wiek (do liczenia średniej)

    // Flagi otwarcia
    int poolOpen[POOLS_COUNT]; // 1 - otwarty, 0 - zamknięty
    int facilityOpen;          // 1 - obiekt otwarty, 0 - zamknięty
} SharedData;

// Plik do ftok() i klucz
#define IPC_KEY_FILE "ipc_key_file"
#define IPC_KEY_ID   0x1234

// ----- Funkcje pomocnicze ----- //

// Zapis zdarzeń do pliku log
void log_event(const char *msg);

// Walidacja parametrów (Tp, Tk, X1..X3)
int validate_parameters(int Tp, int Tk, int X1, int X2, int X3);

#endif
