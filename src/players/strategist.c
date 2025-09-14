// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <playerlib.h>
#include <limits.h>

#define HORIZON_DEPTH 6
#define DECAY_FACTOR 0.8
#define VORONOI_WEIGHT 0.6
#define HORIZON_WEIGHT 0.4

// Global variables for cleanup
static game_state_t *g_state = NULL;
static game_sync_t *g_sync = NULL;

void cleanup_handler(int sig) {
    (void)sig; // Suppress unused parameter warning
    if (g_state) releaseState(g_state);
    if (g_sync) releaseSync(g_sync);
    _exit(0);
}

// Calculate Chebyshev distance between two points
int chebyshevDistance(int x1, int y1, int x2, int y2) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    return dx > dy ? dx : dy;
}

// Calculate Voronoi territory for a given cell
// Returns 1 if this player is closest, 0 otherwise
int isVoronoiTerritory(game_state_t *state, unsigned int myPlayerId, int x, int y) {
    int myDist = chebyshevDistance(state->jugadores[myPlayerId].x, 
                                  state->jugadores[myPlayerId].y, x, y);
    
    for (unsigned int i = 0; i < state->num_jugadores; i++) {
        if (i == myPlayerId) continue;
        int otherDist = chebyshevDistance(state->jugadores[i].x, 
                                         state->jugadores[i].y, x, y);
        if (otherDist <= myDist) {
            return 0; // Another player is closer or equidistant
        }
    }
    return 1; // This player is closest
}

// Calculate horizon search potential with distance decay
double calculateHorizonPotential(game_state_t *state, int startX, int startY, unsigned int myPlayerId) {
    double totalPotential = 0.0;
    int visited[state->height][state->width];
    
    // Initialize visited array
    for (int i = 0; i < state->height; i++) {
        for (int j = 0; j < state->width; j++) {
            visited[i][j] = 0;
        }
    }
    
    // BFS queue for horizon search
    typedef struct {
        int x, y, depth;
    } QueueNode;
    
    QueueNode queue[state->height * state->width];
    int queueHead = 0, queueTail = 0;
    
    // Start from current position
    queue[queueTail++] = (QueueNode){startX, startY, 0};
    visited[startY][startX] = 1;
    
    while (queueHead < queueTail) {
        QueueNode current = queue[queueHead++];
        
        if (current.depth >= HORIZON_DEPTH) continue;
        
        // Check all 8 directions
        for (int offY = -1; offY <= 1; offY++) {
            for (int offX = -1; offX <= 1; offX++) {
                if (offX == 0 && offY == 0) continue;
                
                int newX = current.x + offX;
                int newY = current.y + offY;
                
                // Check bounds
                if (newX < 0 || newX >= state->width || 
                    newY < 0 || newY >= state->height) continue;
                
                // Check if already visited
                if (visited[newY][newX]) continue;
                
                int cellValue = state->tablero[newY * state->width + newX];
                if (cellValue <= 0) continue; // Skip empty or occupied cells
                
                visited[newY][newX] = 1;
                
                // Add to queue for further exploration
                queue[queueTail++] = (QueueNode){newX, newY, current.depth + 1};
                
                // Calculate potential with distance decay
                double decay = 1.0;
                for (int d = 0; d < current.depth + 1; d++) {
                    decay *= DECAY_FACTOR;
                }
                double voronoiBonus = isVoronoiTerritory(state, myPlayerId, newX, newY) ? 1.5 : 1.0;
                totalPotential += cellValue * decay * voronoiBonus;
            }
        }
    }
    
    return totalPotential;
}

// Calculate utility for a potential move
double calculateMoveUtility(game_state_t *state, unsigned int myPlayerId, int moveX, int moveY) {
    int newX = state->jugadores[myPlayerId].x + moveX;
    int newY = state->jugadores[myPlayerId].y + moveY;
    
    // Check bounds
    if (newX < 0 || newX >= state->width || newY < 0 || newY >= state->height) {
        return -1.0; // Invalid move
    }
    
    // Check if cell is available
    int cellValue = state->tablero[newY * state->width + newX];
    if (cellValue <= 0) {
        return -1.0; // Invalid move
    }
    
    // Calculate horizon potential from new position
    double horizonPotential = calculateHorizonPotential(state, newX, newY, myPlayerId);
    
    // Calculate Voronoi territory value
    double voronoiValue = 0.0;
    if (isVoronoiTerritory(state, myPlayerId, newX, newY)) {
        voronoiValue = cellValue * 2.0; // Bonus for claiming territory
    } else {
        voronoiValue = cellValue * 0.5; // Penalty for contested territory
    }
    
    // Combine both factors
    double utility = VORONOI_WEIGHT * voronoiValue + HORIZON_WEIGHT * horizonPotential;
    
    return utility;
}

int main() {
    // Set up signal handlers for cleanup
    signal(SIGTERM, cleanup_handler);
    signal(SIGINT, cleanup_handler);
    
    game_state_t *state = getState();
    if (!state) return 1;
    g_state = state; // Store for cleanup
    
    game_sync_t *sync = getSync();
    if (!sync) {
        releaseState(state);
        return 1;
    }
    g_sync = sync; // Store for cleanup
    
    int playerListIndex;
    jugador_t *playerData = getPlayer(state, getpid(), &playerListIndex);
    if (!playerData) {
        releaseState(state);
        releaseSync(sync);
        return 1;
    }
    char (*moveMap)[3] = getMoveMap();

    while(!playerData->stuck) {
        sem_wait(&(sync->G[playerListIndex]));

        sem_wait(&sync->C);
        sem_wait(&sync->E);
        sync->F++;
        if (sync->F == 1) sem_wait(&sync->D);
        sem_post(&sync->E);
        sem_post(&sync->C);

        // Find best move using VPH strategy
        double maxUtility = -1.0;
        int bestMoveX = 0, bestMoveY = 0;
        int bestFreeNeighbors = -1;
        
        // Check all 8 directions + stay (0,0)
        for (int offY = -1; offY <= 1; offY++) {
            int y = playerData->y + offY;
            if (y < 0 || y >= state->height) continue;

            for (int offX = -1; offX <= 1; offX++) {
                if (offX == 0 && offY == 0) continue;
                int x = playerData->x + offX;
                if (x < 0 || x >= state->width) continue;

                if (state->tablero[y * state->width + x] <= 0) continue;

                double utility = calculateMoveUtility(state, playerListIndex, offX, offY);
                
                // Calculate free neighbors for tiebreaking
                int freeNeighbors = freeNeighborCount(state, x, y);
                
                // Update best move
                if (utility > maxUtility || 
                    (utility == maxUtility && freeNeighbors > bestFreeNeighbors)) {
                    maxUtility = utility;
                    bestMoveX = offX;
                    bestMoveY = offY;
                    bestFreeNeighbors = freeNeighbors;
                }
            }
        }

        sem_wait(&sync->E);
        sync->F--;
        if (sync->F == 0) sem_post(&sync->D);
        sem_post(&sync->E);
        
        unsigned char move = moveMap[bestMoveY+1][bestMoveX+1];
        write(STDOUT_FILENO, &move, sizeof(move));
    }
    return 0;
}
