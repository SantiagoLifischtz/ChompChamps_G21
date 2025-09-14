#define _POSIX_C_SOURCE 200809L
#include <playerlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>
#include <string.h>

static char moveMap[3][3] = {
    {7,0,1},
    {6,8,2},
    {5,4,3}
};

game_state_t *getState() {
    int fd = shm_open("/game_state", O_RDONLY, 0666);
    if (fd == -1) {
        return NULL;
    }
    game_state_t *state = mmap(NULL, sizeof(game_state_t), PROT_READ, MAP_SHARED, fd, 0);
    close(fd); // Close file descriptor after mapping
    if (state == MAP_FAILED) {
        return NULL;
    }
    return state;
}

game_sync_t *getSync() {
    int fd = shm_open("/game_sync", O_RDWR, 0666);
    if (fd == -1) {
        return NULL;
    }
    game_sync_t *sync = mmap(NULL, sizeof(game_sync_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd); // Close file descriptor after mapping
    if (sync == MAP_FAILED) {
        return NULL;
    }
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


typedef struct {
    unsigned short x, y;
    unsigned int depth;
} Node;

int bfsExplore(game_state_t *state, unsigned short startX, unsigned short startY, unsigned int maxDepth, int *exploredSpaces) {
    if (startX >= state->width || startY >= state->height) return 0;
    int totalScore = 0;
    int count = 0;

    int visited[state->height][state->width];
    memset(visited, 0, sizeof(visited));

    int s = 2*maxDepth+1;
    Node queue[s*s];
    int qHead = 0, qTail = 0;
    queue[qTail++] = (Node){startX, startY, 0};
    visited[startY][startX] = 1;

    while(qHead < qTail) {
        Node current = queue[qHead++];
        if (current.depth >= maxDepth) continue;

        for (int offY = -1; offY <= 1; offY++) {
            int y = current.y + offY;
            if (y < 0 || y >= state->height) continue;

            for (int offX = -1; offX <= 1; offX++) {
                if (offX == 0 && offY == 0) continue;
                int x = current.x + offX;
                if (x < 0 || x >= state->width) continue;
                
                int score = state->tablero[y*state->width+x];
                if (visited[y][x] || score <= 0) continue;

                visited[y][x] = 1;
                queue[qTail++] = (Node){x, y, current.depth+1};
                totalScore += score;
                count++;
            }
        }
    }
    if (exploredSpaces != NULL) *exploredSpaces = count;
    return totalScore;
}

void releaseState(game_state_t *state) {
    if (state != NULL) {
        munmap(state, sizeof(game_state_t));
    }
}

void releaseSync(game_sync_t *sync) {
    if (sync != NULL) {
        munmap(sync, sizeof(game_sync_t));
    }
}