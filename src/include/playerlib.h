#ifndef PLAYERLIB_H
#define PLAYERLIB_H

#include <structs.h>

game_state_t *getState();
game_sync_t *getSync();
jugador_t *getPlayer(game_state_t *state, pid_t playerPid, int *playerListIndex);
int freeNeighborCount(game_state_t *state, unsigned short x, unsigned short y);
char (*getMoveMap())[3];

#endif