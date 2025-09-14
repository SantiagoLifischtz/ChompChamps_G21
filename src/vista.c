// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <structs.h>

#define MAX_WIDTH 100
#define MAX_HEIGHT 100

static const char* colors[] = {"\033[34m", "\033[31m", "\033[32m", "\033[33m", "\033[35m", "\033[36m", "\033[37m", "\033[90m", "\033[93m"};

void printAnimatedBar(int length, int frame) {
    // Barra animada para mostrar que el juego sigue corriendo
    int pos = ((frame % 10) + 10) % 10;
    for (int i = 0; i < length; i++)
    {
        if (i%10 != pos) putchar('-');
        else putchar('|');
        putchar(' ');
    }
    putchar('\n');
}

void printEndgameBar(int length) {
    for (int i = 0; i < length-1; i++)
    {
        printf("==");
    }
    printf("=\n");
}

void drawBoard(game_state_t *state) {
    for (int y=0; y<state->height; y++) {
        for (int x=0; x<state->width; x++) {
            int val = state->tablero[y * state->width + x];
            
            // Verificar si hay un jugador en esta posición actual
            int current_player = -1;
            for (size_t i = 0; i < state->num_jugadores; i++) {
                if (state->jugadores[i].x == x && state->jugadores[i].y == y) {
                    current_player = i;
                    break;
                }
            }
            
            if (current_player != -1) {
                // Posición actual de un jugador: mostrar letra coloreada
                char player_char = 'A' + current_player;
                printf("%s%c\033[0m ", colors[current_player], player_char);
            } else if (val <= 0) {
                // Posiciones visitadas: 0 para jugador 0, -i para jugador i
                int player_id;
                if (val == 0) {
                    player_id = 0;  // Jugador 0
                } else {
                    player_id = -val;  // Convertir -1,-2,etc a 1,2,etc
                }
                
                if (player_id >= 0 && player_id < 9) {
                    printf("%s.\033[0m ", colors[player_id]);
                } else {
                    printf(". ");  // Fallback
                }
            } else {
                // Casilleros no visitados: mostrar valores (incluye 0)
                printf("%d ", val);
            }
        }
        putchar('\n');
    }
}

int *getPlayerOrder(game_state_t *state) {
    int n = state->num_jugadores;
    int *idOrder = malloc(sizeof(int) * n);
    if (!idOrder) return NULL;

    for (int i = 0; i < n; i++) {
        idOrder[i] = i;
    }

    // Selection sort
    for (int i = 0; i < n - 1; i++) {
        int best = i;
        for (int j = i + 1; j < n; j++) {
            if (state->jugadores[idOrder[j]].puntaje >
                state->jugadores[idOrder[best]].puntaje) {
                best = j;
            }
        }
        if (best != i) {
            int tmp = idOrder[i];
            idOrder[i] = idOrder[best];
            idOrder[best] = tmp;
        }
    }

    return idOrder;
}

void printStatus(game_state_t *state, char *title) {
    printf("\n%28s\n", title);
    jugador_t *players = state->jugadores;
    int *idOrder = getPlayerOrder(state);
    if (!idOrder) return;

    for (size_t i = 0; i < state->num_jugadores; i++) {
        int id = idOrder[i];
        printf("%s%-16s\033[0m | %4u p | (%2d,%2d) |",
                colors[id],
                players[id].nombre,
                players[id].puntaje,
                players[id].x,
                players[id].y);
        if (players[id].stuck) {
            printf(" %s(x_x)\033[0m",colors[id]);
        }
        putchar('\n');
    }
    free(idOrder);
}

void gameEnded(game_state_t *state) {
    printStatus(state, "=== Game over ===");
    putchar('\n');

    unsigned int maxScore = 0;
    size_t winner = 0;
    for (size_t i=0; i<state->num_jugadores; i++) {
        unsigned int score = state->jugadores[i].puntaje;
        if (score > maxScore) {
            maxScore = score;
            winner = i;
        }
    }
    printEndgameBar(state->width);
    drawBoard(state);
    printEndgameBar(state->width);
    printf("\n=== Winner: [ %s%s\033[0m ] ===\n", colors[winner], state->jugadores[winner].nombre);
}

int main() {
    int shm_state_fd = shm_open("/game_state", O_RDONLY, 0);
    if (shm_state_fd == -1) {
        perror("shm_open game_state");
        return 1;
    };
    game_state_t *state = mmap(NULL, sizeof(game_state_t),
    PROT_READ, MAP_SHARED,
    shm_state_fd, 0);
    if (state == MAP_FAILED) {
        perror("mmap game_state");
        close(shm_state_fd);
        return 1;
    }

    int shm_sync_fd = shm_open("/game_sync", O_RDWR, 0);
    if (shm_sync_fd == -1) {
        perror("shm_open game_sync");
        munmap(state, sizeof(game_state_t));
        close(shm_state_fd);
        return 1;
    }
    
    game_sync_t *sync = mmap(NULL, sizeof(game_sync_t),
                             PROT_READ | PROT_WRITE, MAP_SHARED,
                             shm_sync_fd, 0);
    if (sync == MAP_FAILED) {
        perror("mmap game_sync");
        close(shm_sync_fd);
        munmap(state, sizeof(game_state_t));
        close(shm_state_fd);
        return 1;
    }

    fflush(stdout);

    int frameCounter = 0;

    while (1) {
        sem_wait(&sync->A);

        if (state->terminado) break;
        printStatus(state, "=== Leaderboard ===");
        putchar('\n');
        
        printAnimatedBar(state->width,-1-frameCounter);
        drawBoard(state);
        printAnimatedBar(state->width,frameCounter++);
        fflush(stdout);

        sem_post(&sync->B);
    }
    
    gameEnded(state);
    sem_post(&sync->B);

    // Limpiar recursos
    munmap(state, sizeof(game_state_t));
    munmap(sync, sizeof(game_sync_t));
    close(shm_state_fd);
    close(shm_sync_fd);

    return 0;
}
