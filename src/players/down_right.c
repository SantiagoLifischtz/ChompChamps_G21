#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <playerlib.h>

// jugador simple: se mueve en zig-zag derecha/abajo
int main() {
    
    game_state_t *state = getState();
    game_sync_t *sync = getSync();
    int playerListIndex;
    getPlayer(state, getpid(), &playerListIndex);

    for (int i=0; 1; i++) {
        sem_wait(&(sync->G[playerListIndex]));
        unsigned char move;
        if (i % 2 == 0)
            move = 2; // derecha
        else
            move = 4; // abajo
        write(STDOUT_FILENO, &move, sizeof(move));
    }
    return 0;
}
