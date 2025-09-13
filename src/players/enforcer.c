#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <playerlib.h>
#include <limits.h>

#define CLOSE_THRESHOLD 6

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
        
        int max = 0;
        int min = INT_MAX;
        int toClosest;
        int moveX = 0, moveY = 0, chaseX = 0, chaseY = 0, escapeX = 0, escapeY = 0;
        for (int offY = -1; offY <= 1; offY++)
        {
            int y = playerData->y + offY;
            if (y < 0 || y >= state->height) continue;

            for (int offX = -1; offX <= 1; offX++)
            {
                int x = playerData->x + offX;
                if (offX == 0 && offY == 0) {
                    toClosest = sqrDistClosestOther(state, playerListIndex, x, y);
                    continue;
                }
                if (x < 0 || x >= state->width) continue;

                if (state->tablero[y * state->width + x] <= 0) continue;

                int distance = sqrDistClosestOther(state, playerListIndex, x, y);
                if (distance > max) {
                    max = distance;
                    escapeX = offX;
                    escapeY = offY;
                }
                if (distance < min) {
                    min = distance;
                    chaseX = offX;
                    chaseY = offY;
                }
            }
        }

        if (toClosest < CLOSE_THRESHOLD*CLOSE_THRESHOLD) {
            moveX = escapeX;
            moveY = escapeY;
        }
        else {
            moveX = chaseX;
            moveY = chaseY;
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