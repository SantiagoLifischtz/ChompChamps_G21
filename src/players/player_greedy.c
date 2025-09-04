#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <playerlib.h>

int main() {
    game_state_t *state = getState();
    jugador_t *playerData = getPlayer(state, getpid());
    char (*moveMap)[3] = getMoveMap();

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