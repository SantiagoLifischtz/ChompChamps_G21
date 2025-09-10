#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdlib.h>
#include <sys/types.h>
#include <semaphore.h>

#define PLAYER_NAME_LENGTH 16
#define MAX_JUGADORES 9
#define MAX_WIDTH 100
#define MAX_HEIGHT 100

typedef struct {
    char nombre[PLAYER_NAME_LENGTH];
    unsigned int puntaje;
    unsigned int invalidRequests;
    unsigned int validRequests;
    unsigned short x, y;
    pid_t pid;
    int blocking;
} jugador_t;

typedef struct {
    unsigned short width;
    unsigned short height;
    unsigned int num_jugadores;
    jugador_t jugadores[MAX_JUGADORES];
    int terminado;
    int tablero[MAX_WIDTH * MAX_HEIGHT];
} game_state_t;

typedef struct {
    sem_t A; // El máster le indica a la vista que hay cambios por imprimir
    sem_t B; // La vista le indica al máster que terminó de imprimir
    sem_t C; // Mutex para evitar inanición del máster al acceder al estado
    sem_t D; // Mutex para el estado del juego
    sem_t E; // Mutex para la siguiente variable
    unsigned int F; // Cantidad de jugadores leyendo el estado
    sem_t G[9]; // Le indican a cada jugador que puede enviar 1 movimiento
} game_sync_t;

#endif