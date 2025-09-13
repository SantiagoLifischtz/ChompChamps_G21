#define _POSIX_C_SOURCE 200809L
#include <playerlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>

static char moveMap[3][3] = {
    {7,0,1},
    {6,8,2},
    {5,4,3}
};

game_state_t *getState() {
    int fd = shm_open("/game_state", O_RDONLY, 0666);
    game_state_t *state = mmap(NULL, sizeof(game_state_t), PROT_READ, MAP_SHARED, fd, 0);
    return state;
}

game_sync_t *getSync() {
    int fd = shm_open("/game_sync", O_RDWR, 0666);
    game_sync_t *sync = mmap(NULL, sizeof(game_sync_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    return sync;
}

jugador_t *getPlayer(game_state_t *state, pid_t playerPid, int *playerListIndex) {
    for (size_t i = 0; i < state->num_jugadores; i++)
    {
        if (state->jugadores[i].pid == playerPid) {
            *playerListIndex = i;
            return &(state->jugadores[i]);
        }
    }
    return NULL;
}

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

            if (state->tablero[ny * state->width + nx] > 0) {
                count++;
            }
        }
    }
    return count;
}

int squareDistanceToPlayer(game_state_t *state, int targetPlayerId, unsigned short fromX, unsigned short fromY) {
    int dx = state->jugadores[targetPlayerId].x - fromX;
    int dy = state->jugadores[targetPlayerId].y - fromY;
    return dx*dx+dy*dy;
}

int sqrDistClosestOther(game_state_t *state, unsigned int callerId, unsigned short fromX, unsigned short fromY) {
    int min = INT_MAX;
    for (size_t i = 0; i < state->num_jugadores; i++)
    {
        if (i == callerId) continue;
        int dist = squareDistanceToPlayer(state, i, fromX, fromY);
        if (dist < min && dist != 0) {
            min = dist;
        }
    }
    return min;
}

char (*getMoveMap())[3] {
    return moveMap;
}