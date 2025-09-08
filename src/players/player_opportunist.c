#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <playerlib.h>

int freeNeighborCount(game_state_t *state, unsigned short x, unsigned short y) {
    int count = 0;
    for (int offY = -1; offY <= 1; offY++)
    {
        int ny = y + offY;
        if (ny < 0 || ny >= state->height) continue;

        for (int offX = -1; offX <= 1; offX++)
        {
            if (offX == 0 && offY == 0) continue;
            int nx = x + offX;
            if (nx < 0 || nx >= state->width) continue;

            if (state->tablero[ny][nx] >= 0) {
                count++;
            }
        }
    }
    return count;
}

int main() {
    game_state_t *state = getState();
    game_sync_t *sync = getSync();
    int playerListIndex;
    jugador_t *playerData = getPlayer(state, getpid(), &playerListIndex);
    char (*moveMap)[3] = getMoveMap();

    while(1) {

        sem_wait(&(sync->G[playerListIndex]));

        sem_wait(&sync->C);
        sem_wait(&sync->E);
        sync->F++;
        if (sync->F == 1) sem_wait(&sync->D);
        sem_post(&sync->E);
        sem_post(&sync->C);

        // Busca el lugar con mas espacios libres alrededor, desempata con mayor puntaje
        char ties[8][2] = {0};
        int tieIndex = 0;

        int max = -1;
        int moveX = 0, moveY = 0;
        for (int offY = -1; offY <= 1; offY++)
        {
            int y = playerData->y + offY;
            if (y < 0 || y >= state->height) continue;

            for (int offX = -1; offX <= 1; offX++)
            {
                if (offX == 0 && offY == 0) continue;
                int x = playerData->x + offX;
                if (x < 0 || x >= state->width) continue;

                if (state->tablero[y][x] < 0) continue;

                int freeSpaces = freeNeighborCount(state, x, y);
                
                if (freeSpaces == max) {
                    ties[tieIndex][0] = offX;
                    ties[tieIndex][1] = offY;
                    tieIndex++;
                }
                if (freeSpaces > max) {
                    max = freeSpaces;
                    moveX = offX;
                    moveY = offY;
                    
                    tieIndex = 0;
                    ties[tieIndex][0] = offX;
                    ties[tieIndex][1] = offY;
                }
            }
        }

        int maxScore = 0;
        for (int i = 0; i < tieIndex; i++)
        {
            int x = playerData->x + ties[i][0];
            int y = playerData->y + ties[i][1];
            int foundScore = state->tablero[y][x];
            if (foundScore > maxScore) {
                maxScore = foundScore;
                moveX = ties[i][0];
                moveY = ties[i][1];
            }
        }

        sem_wait(&sync->E);
        sync->F--;
        if (sync->F == 0) sem_post(&sync->D);
        sem_post(&sync->E);
        
        unsigned char move = moveMap[moveY+1][moveX+1];
        write(STDOUT_FILENO, &move, sizeof(move));
    }
    return 0;
}