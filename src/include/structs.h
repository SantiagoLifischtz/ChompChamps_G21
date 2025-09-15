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
    int stuck;
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
    sem_t view_update_signal; // El máster le indica a la vista que hay cambios por imprimir
    sem_t view_done_signal; // La vista le indica al máster que terminó de imprimir
    sem_t master_access_mutex; // Mutex para evitar inanición del máster al acceder al estado
    sem_t game_state_mutex; // Mutex para el estado del juego
    sem_t reader_count_mutex; // Mutex para la siguiente variable
    unsigned int active_readers_count; // Cantidad de jugadores leyendo el estado
    sem_t player_move_token[9]; // Le indican a cada jugador que puede enviar 1 movimiento
} game_sync_t;

#endif