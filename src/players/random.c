// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <playerlib.h>

double randNorm() {
    return (double)rand() / ((double)RAND_MAX + 1);
}

int randInt(int minInclusive, int maxExclusive) {
    return minInclusive + (int)(randNorm() * (maxExclusive - minInclusive));
}

int main() {
    srand(getpid() ^ time(NULL));

    game_state_t *state = getState();
    game_sync_t *sync = getSync();
    int playerListIndex;
    jugador_t *playerData = getPlayer(state, getpid(), &playerListIndex);

    while(!playerData->stuck) {
        sem_wait(&(sync->player_move_token[playerListIndex]));

        unsigned char move = randInt(0,8);
        write(STDOUT_FILENO, &move, sizeof(move));
    }
    return 0;
}