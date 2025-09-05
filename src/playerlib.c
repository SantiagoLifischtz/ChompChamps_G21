#define _POSIX_C_SOURCE 200809L
#include <playerlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

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

char (*getMoveMap())[3] {
    return moveMap;
}