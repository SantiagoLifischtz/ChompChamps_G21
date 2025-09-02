#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include "structs.h"

#define MAX_WIDTH 100
#define MAX_HEIGHT 100

void printAnimatedBar(int length, int frame) {
    // Barra animada para mostrar que el juego sigue corriendo
    frame = ((frame % length) + length) % length;
    for (int i = 0; i < length; i++)
    {
        if (i != frame) putchar('=');
        else putchar('|');
        putchar(' ');
    }
    putchar('\n');
}

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

    int frameCounter = 0;

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

        printAnimatedBar(state->width,-frameCounter);
        for (int y=0; y<state->height; y++) {
            for (int x=0; x<state->width; x++) {
                int val = state->tablero[y][x];

                if (val > 0) printf("%d ", val);
                else if (val == 0) printf(". ");
                else {
                    if (val <= -11) {
                        // Posiciones visitadas: mostrar puntos coloreados
                        int player_id = (-val) - 11;  // Convertir -11,-12,etc a 0,1,etc
                        const char* colors[] = {"\033[34m", "\033[31m", "\033[32m", "\033[33m", 
                                              "\033[35m", "\033[36m", "\033[37m", "\033[90m", "\033[93m"};
                        if (player_id >= 0 && player_id < 9) {
                            printf("%s.\033[0m ", colors[player_id]);
                        } else {
                            printf(". ");  // Fallback
                        }
                    } else {
                        // Posiciones actuales: mostrar letras coloreadas
                        int player_id = (-val) - 1;  // Convertir -1,-2,etc a 0,1,etc
                        if (player_id >= 0 && player_id < 9) {
                            char player_char = 'A' + player_id;
                            const char* colors[] = {"\033[34m", "\033[31m", "\033[32m", "\033[33m", 
                                                  "\033[35m", "\033[36m", "\033[37m", "\033[90m", "\033[93m"};

                            printf("%s%c\033[0m ", colors[player_id], player_char);
                        } else {
                            printf("? ");  // Fallback para jugadores fuera de rango
                        }
                    }
                }
            }
            printf("\n");
        }
        printAnimatedBar(state->width,frameCounter++);
        fflush(stdout);

        sem_post(&sync->B);
    }

    return 0;
}
