CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -g

all: master jugador vista

master: src/master.c
	$(CC) $(CFLAGS) -o build/master src/master.c -lrt -pthread

jugador: src/jugador.c
	$(CC) $(CFLAGS) -o build/jugador src/jugador.c

vista: src/vista.c
	$(CC) $(CFLAGS) -o build/vista src/vista.c -lrt -pthread

clean:
	rm -f build/master build/jugador build/vista
