// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <playerlib.h>
#include <math.h>

#define EPSILON 1e-4

int main() {
    game_state_t *state = getState();
    game_sync_t *sync = getSync();
    int playerListIndex;
    jugador_t *playerData = getPlayer(state, getpid(), &playerListIndex);
    char (*moveMap)[3] = getMoveMap();

    while(!playerData->stuck) {

        sem_wait(&(sync->player_move_token[playerListIndex]));

        sem_wait(&sync->master_access_mutex);
        sem_wait(&sync->reader_count_mutex);
        sync->active_readers_count++;
        if (sync->active_readers_count == 1) sem_wait(&sync->game_state_mutex);
        sem_post(&sync->reader_count_mutex);
        sem_post(&sync->master_access_mutex);

        // Busca el lugar con mas espacios libres alrededor, desempata con mayor puntaje
        char ties[8][2] = {0};
        int tieIndex = 0;

        float max = -1;
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

                if (state->tablero[y * state->width + x] <= 0) continue;

                float multiplier =  offX*offY != 0 ? 1 : 0.5;

                float freeSpaces = freeNeighborCount(state, x, y) * multiplier;
                
                
                if (fabs(freeSpaces - max) < EPSILON) {
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
                    tieIndex++;
                }
            }
        }

        int maxScore = 0;
        for (int i = 0; i < tieIndex; i++)
        {
            int x = playerData->x + ties[i][0];
            int y = playerData->y + ties[i][1];
            float multiplier =  ties[i][0]*ties[i][1] != 0 ? 1 : 0.5;
            float foundScore = state->tablero[y * state->width + x] * multiplier;
            if (foundScore > maxScore) {
                maxScore = foundScore;
                moveX = ties[i][0];
                moveY = ties[i][1];
            }
        }

        sem_wait(&sync->reader_count_mutex);
        sync->active_readers_count--;
        if (sync->active_readers_count == 0) sem_post(&sync->game_state_mutex);
        sem_post(&sync->reader_count_mutex);
        
        unsigned char move = moveMap[moveY+1][moveX+1];
        write(STDOUT_FILENO, &move, sizeof(move));
    }
    return 0;
}