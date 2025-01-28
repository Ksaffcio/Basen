# Makefile - kompilacja całego projektu

CC = gcc
CFLAGS = -Wall -pedantic -O2 -pthread

OBJ = ipc.o kasjer.o ratownik.o klient.o main.o

all: basen

basen: $(OBJ)
	$(CC) $(CFLAGS) -o basen $(OBJ)

ipc.o: ipc.c ipc.h basen.h
	$(CC) $(CFLAGS) -c ipc.c

kasjer.o: kasjer.c kasjer.h basen.h ipc.h
	$(CC) $(CFLAGS) -c kasjer.c

ratownik.o: ratownik.c ratownik.h basen.h ipc.h
	$(CC) $(CFLAGS) -c ratownik.c

klient.o: klient.c klient.h basen.h ipc.h
	$(CC) $(CFLAGS) -c klient.c

main.o: main.c basen.h ipc.h kasjer.h ratownik.h klient.h
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f *.o basen basen.log
