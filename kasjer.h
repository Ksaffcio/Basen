#ifndef KASJER_H
#define KASJER_H

#include "basen.h"
#include "ipc.h"

/*
   Deklaracje funkcji kasjera oraz pomocniczych
   do obsługi kolejki (add_client_to_queue, pop_client_from_queue).
*/

// Dodawanie klienta do kolejki
void add_client_to_queue(int clientPid, int isVIP);

// Pobieranie pierwszego klienta z kolejki (z uwzględnieniem VIP przed zwykłymi?)
int pop_client_from_queue(void);

// Główna funkcja (pętla) procesu kasjera
void run_cashier_process(void);

#endif
