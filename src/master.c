#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>

#define NUM_JUGADORES 2
#define MAX_WIDTH 10
#define MAX_HEIGHT 10

// ----- estructuras -----
typedef struct {
    unsigned int puntaje;
    unsigned short x, y;  // posición actual
} jugador_t;

typedef struct {
    unsigned short width;
    unsigned short height;
    unsigned int num_jugadores;
    jugador_t jugadores[NUM_JUGADORES];
    int terminado;
    int tablero[MAX_HEIGHT][MAX_WIDTH];
} game_state_t;

typedef struct {
    sem_t A; // master -> vista: hay cambios
    sem_t B; // vista -> master: ya imprimí
} game_sync_t;
// -----------------------

int main() {
    int pipes[NUM_JUGADORES][2];
    pid_t jugadores[NUM_JUGADORES];
    pid_t vista;

    // ----- shm estado -----
    int shm_state_fd = shm_open("/game_state", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_state_fd, sizeof(game_state_t));
    game_state_t *state = mmap(NULL, sizeof(game_state_t),
                               PROT_READ | PROT_WRITE, MAP_SHARED,
                               shm_state_fd, 0);

    state->width = MAX_WIDTH;
    state->height = MAX_HEIGHT;
    state->num_jugadores = NUM_JUGADORES;
    state->terminado = 0;

    // tablero inicial
    srand(time(NULL));
    for (int y=0; y<state->height; y++)
        for (int x=0; x<state->width; x++)
            state->tablero[y][x] = 1 + rand() % 9;

    // jugadores iniciales
    state->jugadores[0].x = 0; state->jugadores[0].y = 0;
    state->jugadores[1].x = 9; state->jugadores[1].y = 9;
    state->tablero[0][0] = -0;
    state->tablero[9][9] = -1;
    for (int i=0; i<NUM_JUGADORES; i++) state->jugadores[i].puntaje = 0;

    // ----- shm sync -----
    int shm_sync_fd = shm_open("/game_sync", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_sync_fd, sizeof(game_sync_t));
    game_sync_t *sync = mmap(NULL, sizeof(game_sync_t),
                             PROT_READ | PROT_WRITE, MAP_SHARED,
                             shm_sync_fd, 0);

    sem_init(&sync->A, 1, 0);
    sem_init(&sync->B, 1, 0);

    // ----- pipes + jugadores -----
    for (int i = 0; i < NUM_JUGADORES; i++) {
        pipe(pipes[i]);
        pid_t pid = fork();
        if (pid == 0) {
            // hijo = jugador
            close(pipes[i][0]);                     // cierro lectura
            dup2(pipes[i][1], STDOUT_FILENO);       // stdout → escritura del pipe

            if (i == 0) {
                execl("./build/jugador1", "jugador1", NULL);
                perror("execl jugador1");
            } else if (i == 1) {
                execl("./build/jugador2", "jugador2", NULL);
                perror("execl jugador2");
            }
            exit(1); // solo llega si execl falla
        } else if (pid > 0) {
            jugadores[i] = pid;
            close(pipes[i][1]); // master no escribe en este pipe
        } else {
            perror("fork jugador");
            exit(1);
        }
    }

    // ----- vista -----
    vista = fork();
    if (vista == 0) {
        execl("./build/vista", "vista", NULL);
        perror("execl vista");
        exit(1);
    }

    // ----- bucle master -----
    int steps = 0;
    while (!state->terminado && steps < 20) {
        for (int i = 0; i < NUM_JUGADORES; i++) {
            unsigned char move;
            ssize_t n = read(pipes[i][0], &move, sizeof(move));
            if (n > 0) {
                unsigned short x = state->jugadores[i].x;
                unsigned short y = state->jugadores[i].y;
                unsigned short nx = x, ny = y;

                // interpretar movimiento (0 arriba, 2 derecha, 4 abajo, 6 izquierda)
                if (move == 0 && y>0) ny--;
                if (move == 2 && x+1<state->width) nx++;
                if (move == 4 && y+1<state->height) ny++;
                if (move == 6 && x>0) nx--;

                // actualizar puntaje y tablero
                int reward = state->tablero[ny][nx];
                if (reward > 0) state->jugadores[i].puntaje += reward;
                state->tablero[ny][nx] = -i;
                state->jugadores[i].x = nx;
                state->jugadores[i].y = ny;

                // avisar a vista
                sem_post(&sync->A);
                sem_wait(&sync->B);
            }
        }
        steps++;
        sleep(1);
    }

    state->terminado = 1;
    sem_post(&sync->A);

    for (int i=0; i<NUM_JUGADORES; i++) waitpid(jugadores[i], NULL, 0);
    waitpid(vista, NULL, 0);

    shm_unlink("/game_state");
    shm_unlink("/game_sync");

    return 0;
}
