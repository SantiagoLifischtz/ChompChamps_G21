#include <playerlib.h>
#include <fcntl.h>
#include <sys/mman.h>

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
    int fd = shm_open("/game_sync", O_RDONLY, 0666);
    game_sync_t *sync = mmap(NULL, sizeof(game_sync_t), PROT_READ, MAP_SHARED, fd, 0);
    return sync;
}

jugador_t *getPlayer(game_state_t *state, pid_t playerPid) {
    for (size_t i = 0; i < state->num_jugadores; i++)
    {
        if (state->jugadores[i].pid == playerPid) {
            return &(state->jugadores[i]);
        }
    }
    return NULL;
}

char (*getMoveMap())[3] {
    return moveMap;
}