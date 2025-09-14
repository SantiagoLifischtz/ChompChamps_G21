#ifndef PLAYERLIB_H
#define PLAYERLIB_H

#include <structs.h>

game_state_t *getState();
game_sync_t *getSync();
void releaseState(game_state_t *state);
void releaseSync(game_sync_t *sync);
jugador_t *getPlayer(game_state_t *state, pid_t playerPid, int *playerListIndex);
int freeNeighborCount(game_state_t *state, unsigned short x, unsigned short y);
char (*getMoveMap())[3];
int squareDistanceToPlayer(game_state_t *state, int targetPlayerId, unsigned short fromX, unsigned short fromY);
int sqrDistClosestOther(game_state_t *state, unsigned int callerId, unsigned short fromX, unsigned short fromY);
int bfsExplore(game_state_t *state, unsigned short x, unsigned short y, unsigned int maxDepth, int *exploredSpaces);

#endif