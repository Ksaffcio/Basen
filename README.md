II. Raport
5.1. Raport – założenia projektowe, opis kodu, co zrobiono, problemy, elementy specjalne, problemy z testami
Założenia projektowe
Program symuluje kompleks basenów z trzema basenami (olimpijski, rekreacyjny, brodzik) i ograniczeniami wynikającymi z regulaminu (18+ w olimpijskim, dzieci <10 z opiekunem, do 5 lat => brodzik, średnia <= 40 w rekreacyjnym, itp.).
Czas działania: od Tp do Tk sekund (jeśli Tk=-1, symulacja się nie zamyka automatycznie).
Kasjer przyjmuje klientów z kolejki i wpuszcza do basenów.
Ratownicy co 10s każą opróżnić basen (SIG_EMPTY_POOL), po 5s otwierają go ponownie (SIG_REOPEN_POOL).
Klienci to procesy potomne, generowane przez wątek generatora. Mogą mieć bilet czasowy 10..30s albo -1 => bez limitu.


Numeracja basenów:




Limity oraz szansa na VIP:


Opis kodu – kluczowe moduły
ipc.c/h: tworzenie i usuwanie pamięci dzielonej i semaforów, operacje sem_op().
kasjer.c/h: proces kasjera, funkcje do obsługi kolejki add_client_to_queue i pop_client_from_queue.
ratownik.c/h: procesy ratowników – sygnały do klientów, zamykanie/otwieranie basenów.
klient.c/h: proces klienta – reagowanie na sygnały, sprawdzanie biletu.
main.c: główna logika – inicjalizacja IPC, uruchomienie kasjera i ratowników (fork()), wątek generatora klientów (pthread_create()), pętla wyświetlająca stan basenów, kończenie symulacji po czasie.
Co się udało zrobić
Spełniono większość punktów regulaminu (pełnoletność w olympic, dzieci <10 = hasGuardian, do 5 lat => brodzik, <3 => pampers, rekreacyjny <=40 itp.).
Dodano obsługę VIP – kolejka z priorytetem.
Pamięć dzielona, semafory, sygnały, wątki, procesy – wszystko w jednym projekcie.
Dodano mechanizm Tk=-1 => działanie bez końca.
Z czym były problemy
Zaimplementowanie pary dziecko+opiekun w jednym procesie – uproszczono do pola hasGuardian=1.
Ewentualne kolizje semaforów, jeśli ktoś usuwa semafory z zewnątrz.
Trudności w testach z automatycznym parsowaniem logów.
Dodane elementy specjalne
Mechanizm VIP – klienci VIP trafiają na początek kolejki.
Obsługa biletów nieskończonych (ticketTime=-1).
Rozbudowany plik testowy test_program.c.
Zauważone problemy z testami
W testach, które ingerują zewnętrznie w zasoby IPC (np. usuwanie pliku ipc_key_file czy ipcrm sem w trakcie) – program może się „wysypać” lub wyrzucić błędy semop, shmget itp.
Test 5 opiera się na parsowaniu logu. Jeśli zmieni się format basen.log, test może nie wykryć poprawnie warunków.

5.2. Linki do istotnych fragmentów kodu (GitHub)
(W realnym projekcie w repozytorium GitHub można dodać linki do konkretnych commitów/plików/fragmentów. Poniżej przykładowo pokazano, jak może to wyglądać. Proszę zastąpić YOUR_REPO_LINK swoim adresem w GitHub.)
Tworzenie i obsługa plików (creat(), open(), close(), write(), unlink())
Kod w main.c, linie ~170-180 (tworzenie pliku IPC_KEY_FILE)
Funkcja log_event w main.c, linie ~110-130 (open/close/write)
Usuwanie pliku ipc_key_file w main.c, linia ~290 (unlink)
Tworzenie procesów (fork(), exec(), exit(), wait())
main.c, linie ~190-210 (fork kasjera, lifeguards, exit() w potomnych)
test_program.c, linia ~28 (fork i exec „basen”)
Czekanie wait(NULL) w main.c, linie ~280-285
Tworzenie i obsługa wątków (pthread_create(), pthread_detach(), etc.)
main.c, linie ~220-225 – pthread_create(&genThread, NULL, client_generator_thread, NULL)
main.c, linia ~221 – pthread_detach(genThread)
Obsługa sygnałów (kill(), raise(), signal(), sigaction())
ratownik.c, wysyłanie sygnału SIG_EMPTY_POOL i SIG_REOPEN_POOL do klientów, linie ~58-65 i 80-87
klient.c, definicja client_handle_empty_pool i client_handle_reopen_pool z sigaction(), linie ~20-45
Synchronizacja procesów/wątków (ftok(), semget(), semctl(), semop())
ipc.c, tworzenie semaforów w create_semaphores(), linie ~58-72
ipc.c, sem_op(int semnum, int delta), linie ~98-113
Łącza nazwane i nienazwane (tu nie używamy w tym projekcie – można by dodać, ale nie jest wymagane, o ile mamy 4 różne konstrukcje)
Segmenty pamięci dzielonej (ftok(), shmget(), shmat(), shmdt(), shmctl())
ipc.c, create_shared_memory(), linie ~20-35
ipc.c, attach_shared_memory(), linie ~48-57
Kolejki komunikatów (nie użyte w tym projekcie – można by dodać)
Gniazda (socket) (nie użyte w tym projekcie)
Przykład linków do konkretnych commitów / fragmentów kodu można wygenerować w GitHub, zaznaczając fragment i wybierając opcję „copy permalink”.

5.3. Punktacja i podsumowanie
Zgodność programu z opisem: Obsłużono 3 baseny, limity, zasady wiekowe, VIP.
Poprawność funkcjonalna: Program przeszedł testy:
Test 1: Pojemność basenu ustawiona na 0
Raport: Klienci rejestrują się przy kasie i czekają na wejście do basenu. Ratownicy poprawnie wysyłają sygnały do opuszczenia basenu.

Test 2: Nagłe zabicie głownego procesu
Raport: Program poprawnie identyfikuje SIGINT i poprawnie czyści pamięć unikając problemów przy zamknięciu


Test 3: Testowanie Średniej w basenie rekreacyjnym


Cztery mechanizmy z listy:
Tworzenie i obsługa procesów (kasjer, ratownik, klienci),
Wątki (generator klientów),
Synchronizacja (semafory) i pamięć dzielona,
Obsługa sygnałów (ratownik -> klient).
Wyróżnienia:
Mechanizm VIP w kolejce,
Obsługa biletów nieskończonych (-1),
Logi w pliku basen.log dla czytelnej analizy.



