#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../structs.h"

// TODO: mover a libreria para todos los players
jugador_t *getPlayer(game_state_t *state, pid_t playerPid) {
    for (size_t i = 0; i < state->num_jugadores; i++)
    {
        if (state->jugadores[i].pid == playerPid) {
            return &(state->jugadores[i]);
        }
    }
    return NULL;
}

int main() {
    int fd = shm_open("/game_state", O_RDONLY, 0666);
    game_state_t *state = mmap(NULL, sizeof(game_state_t), PROT_READ, MAP_SHARED, fd, 0);
    jugador_t *playerData = getPlayer(state, getpid());
    char moveMap[3][3] = {
        {7,0,1},
        {6,8,2},
        {5,4,3}
    };

    while(1) {

        // Busca el lugar con mas puntaje
        int max = 0;
        int moveX = 0, moveY = 0;
        for (int offY = -1; offY <= 1; offY++)
        {
            int y = playerData->y + offY;
            if (y < 0 || y >= state->height) continue;

            for (int offX = -1; offX <= 1; offX++)
            {
                int x = playerData->x + offX;
                if (x < 0 || x >= state->width) continue;

                int foundScore = state->tablero[y][x];
                if (foundScore > max) {
                    max = foundScore;
                    moveX = offX;
                    moveY = offY;
                }
            }
        }
        
        unsigned char move = moveMap[moveY+1][moveX+1];
        write(STDOUT_FILENO, &move, sizeof(move));
        sleep(1); // Hasta que haya sincronizacion con el master
        // Necesita leer el game state correctamente antes de cada movimiento
    }
    return 0;
}