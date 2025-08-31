#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

#define MAX_WIDTH 10
#define MAX_HEIGHT 10

typedef struct {
    unsigned int puntaje;
    unsigned short x, y;
} jugador_t;

typedef struct {
    unsigned short width;
    unsigned short height;
    unsigned int num_jugadores;
    jugador_t jugadores[2];
    int terminado;
    int tablero[MAX_HEIGHT][MAX_WIDTH];
} game_state_t;

typedef struct {
    sem_t A;
    sem_t B;
} game_sync_t;

int main() {
    int shm_state_fd = shm_open("/game_state", O_RDWR, 0666);
    game_state_t *state = mmap(NULL, sizeof(game_state_t),
                               PROT_READ | PROT_WRITE, MAP_SHARED,
                               shm_state_fd, 0);

    int shm_sync_fd = shm_open("/game_sync", O_RDWR, 0666);
    game_sync_t *sync = mmap(NULL, sizeof(game_sync_t),
                             PROT_READ | PROT_WRITE, MAP_SHARED,
                             shm_sync_fd, 0);

    printf("[Vista] iniciada.\n");
    fflush(stdout);

    while (1) {
        sem_wait(&sync->A);

        if (state->terminado) break;

        printf("\n=== Estado del juego ===\n");
        for (int i=0; i<state->num_jugadores; i++) {
            printf("Jugador %d: puntaje=%u pos=(%d,%d)\n",
                   i,
                   state->jugadores[i].puntaje,
                   state->jugadores[i].x,
                   state->jugadores[i].y);
        }

        printf("\nTablero:\n");
        for (int y=0; y<state->height; y++) {
            for (int x=0; x<state->width; x++) {
                int val = state->tablero[y][x];
                if (val > 0) printf("%d", val);
                else if (val == 0) printf(".");
                else {
                    if (val <= -11) {
                        // Posiciones visitadas: mostrar puntos coloreados
                        int player_id = (-val) - 11;  // Convertir -11,-12 a 0,1
                        if (player_id == 0) {
                            printf("\033[34m.\033[0m");  // Punto azul para el recorrido del jugador A
                        } else if (player_id == 1) {
                            printf("\033[31m.\033[0m");  // Punto rojo para el recorrido del jugador B
                        } else {
                            printf(".");  // Fallback
                        }
                    } else {
                        // Posiciones actuales: mostrar letras coloreadas
                        int player_id = (-val) - 1;  // Convertir -1,-2 a 0,1
                        char player_char = 'A' + player_id;
                        if (player_char == 'A') {
                            printf("\033[34m%c\033[0m", player_char);  // A azul
                        } else if (player_char == 'B') {
                            printf("\033[31m%c\033[0m", player_char);  // B rojo
                        } else {
                            printf("%c", player_char);  // Fallback para otros jugadores
                        }
                    }
                }
            }
            printf("\n");
        }
        printf("====\n");
        fflush(stdout);

        sem_post(&sync->B);
    }

    return 0;
}
