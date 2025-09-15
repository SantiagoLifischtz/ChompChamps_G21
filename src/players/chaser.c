// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <playerlib.h>
#include <limits.h>

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
        
        int min = INT_MAX;
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

                int distance = sqrDistClosestOther(state, playerListIndex, x, y);
                if (distance < min) {
                    min = distance;
                    moveX = offX;
                    moveY = offY;
                }
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