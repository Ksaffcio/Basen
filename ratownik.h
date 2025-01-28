#ifndef RATOWNIK_H
#define RATOWNIK_H

#include "basen.h"
#include "ipc.h"

/*
   Funkcja procesu ratownika, obsługująca sygnały
   (opróżnianie basenu, ponowne otwieranie).
*/

void run_lifeguard_process(int poolId);

#endif
