// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <playerlib.h>
#include <limits.h>
#include <math.h>

// Constantes para el algoritmo de estrategia
#define HORIZON_DEPTH 6        // Profundidad máxima de búsqueda en el horizonte
#define DECAY_FACTOR 0.8       // Factor de decaimiento para la distancia
#define VORONOI_WEIGHT 0.6     // Peso del factor de territorio Voronoi
#define HORIZON_WEIGHT 0.4     // Peso del potencial del horizonte
#define EPSILON 1e-4           // Tolerancia para comparaciones de punto flotante

// Variables globales para limpieza
static game_state_t *g_state = NULL;
static game_sync_t *g_sync = NULL;

// Manejador de limpieza para señales de terminación
void cleanup_handler(int sig) {
    (void)sig; // Suprimir advertencia de parámetro no utilizado
    if (g_state) releaseState(g_state);
    if (g_sync) releaseSync(g_sync);
    _exit(0);
}

// Calcular la distancia de Chebyshev entre dos puntos
int chebyshevDistance(int x1, int y1, int x2, int y2) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    return dx > dy ? dx : dy;
}

// Calcular territorio Voronoi para una celda dada
// Retorna 1 si este jugador es el más cercano, 0 en caso contrario
int isVoronoiTerritory(game_state_t *state, unsigned int myPlayerId, int x, int y) {
    int myDist = chebyshevDistance(state->jugadores[myPlayerId].x, 
                                  state->jugadores[myPlayerId].y, x, y);
    
    for (unsigned int i = 0; i < state->num_jugadores; i++) {
        if (i == myPlayerId) continue;
        int otherDist = chebyshevDistance(state->jugadores[i].x, 
                                         state->jugadores[i].y, x, y);
        if (otherDist <= myDist) {
            return 0; // Otro jugador está más cerca o a la misma distancia
        }
    }
    return 1; // Este jugador es el más cercano
}

// Calcular el potencial de búsqueda del horizonte con decaimiento por distancia
double calculateHorizonPotential(game_state_t *state, int startX, int startY, unsigned int myPlayerId) {
    double totalPotential = 0.0;
    int visited[state->height][state->width];
    
    // Inicializar array de visitados
    for (int i = 0; i < state->height; i++) {
        for (int j = 0; j < state->width; j++) {
            visited[i][j] = 0;
        }
    }
    
    // Cola BFS para búsqueda del horizonte
    typedef struct {
        int x, y, depth;
    } QueueNode;
    
    QueueNode queue[state->height * state->width];
    int queueHead = 0, queueTail = 0;
    
    // Comenzar desde la posición actual
    queue[queueTail++] = (QueueNode){startX, startY, 0};
    visited[startY][startX] = 1;
    
    while (queueHead < queueTail) {
        QueueNode current = queue[queueHead++];
        
        if (current.depth >= HORIZON_DEPTH) continue;
        
        // Verificar las 8 direcciones
        for (int offY = -1; offY <= 1; offY++) {
            for (int offX = -1; offX <= 1; offX++) {
                if (offX == 0 && offY == 0) continue;
                
                int newX = current.x + offX;
                int newY = current.y + offY;
                
                // Verificar límites
                if (newX < 0 || newX >= state->width || 
                    newY < 0 || newY >= state->height) continue;
                
                // Verificar si ya fue visitado
                if (visited[newY][newX]) continue;
                
                int cellValue = state->tablero[newY * state->width + newX];
                if (cellValue <= 0) continue; // Omitir celdas vacías u ocupadas
                
                visited[newY][newX] = 1;
                
                // Agregar a la cola para exploración posterior
                queue[queueTail++] = (QueueNode){newX, newY, current.depth + 1};
                
                // Calcular potencial con decaimiento por distancia
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

// Calcular utilidad para un movimiento potencial
double calculateMoveUtility(game_state_t *state, unsigned int myPlayerId, int moveX, int moveY) {
    int newX = state->jugadores[myPlayerId].x + moveX;
    int newY = state->jugadores[myPlayerId].y + moveY;
    
    // Verificar límites
    if (newX < 0 || newX >= state->width || newY < 0 || newY >= state->height) {
        return -1.0; // Movimiento inválido
    }
    
    // Verificar si la celda está disponible
    int cellValue = state->tablero[newY * state->width + newX];
    if (cellValue <= 0) {
        return -1.0; // Movimiento inválido
    }
    
    // Calcular potencial del horizonte desde la nueva posición
    double horizonPotential = calculateHorizonPotential(state, newX, newY, myPlayerId);
    
    // Calcular valor del territorio Voronoi
    double voronoiValue = 0.0;
    if (isVoronoiTerritory(state, myPlayerId, newX, newY)) {
        voronoiValue = cellValue * 2.0; // Bonificación por reclamar territorio
    } else {
        voronoiValue = cellValue * 0.5; // Penalización por territorio disputado
    }
    
    // Combinar ambos factores
    double utility = VORONOI_WEIGHT * voronoiValue + HORIZON_WEIGHT * horizonPotential;
    
    return utility;
}

int main() {
    // Configurar manejadores de señales para limpieza
    signal(SIGTERM, cleanup_handler);
    signal(SIGINT, cleanup_handler);
    
    game_state_t *state = getState();
    if (!state) return 1;
    g_state = state; // Almacenar para limpieza
    
    game_sync_t *sync = getSync();
    if (!sync) {
        releaseState(state);
        return 1;
    }
    g_sync = sync; // Almacenar para limpieza
    
    int playerListIndex;
    jugador_t *playerData = getPlayer(state, getpid(), &playerListIndex);
    if (!playerData) {
        releaseState(state);
        releaseSync(sync);
        return 1;
    }
    char (*moveMap)[3] = getMoveMap();

    while(!playerData->stuck) {
        sem_wait(&(sync->player_move_token[playerListIndex]));

        sem_wait(&sync->master_access_mutex);
        sem_wait(&sync->reader_count_mutex);
        sync->active_readers_count++;
        if (sync->active_readers_count == 1) sem_wait(&sync->game_state_mutex);
        sem_post(&sync->reader_count_mutex);
        sem_post(&sync->master_access_mutex);

        // Encontrar el mejor movimiento usando estrategia VPH
        double maxUtility = -1.0;
        int bestMoveX = 0, bestMoveY = 0;
        int bestFreeNeighbors = -1;
        
        // Verificar las 8 direcciones + quedarse (0,0)
        for (int offY = -1; offY <= 1; offY++) {
            int y = playerData->y + offY;
            if (y < 0 || y >= state->height) continue;

            for (int offX = -1; offX <= 1; offX++) {
                if (offX == 0 && offY == 0) continue;
                int x = playerData->x + offX;
                if (x < 0 || x >= state->width) continue;

                if (state->tablero[y * state->width + x] <= 0) continue;

                double utility = calculateMoveUtility(state, playerListIndex, offX, offY);
                
                // Calcular vecinos libres para desempate
                int freeNeighbors = freeNeighborCount(state, x, y);
                
                // Actualizar mejor movimiento
                if (utility > maxUtility || 
                    (fabs(utility - maxUtility) < EPSILON && freeNeighbors > bestFreeNeighbors)) {
                    maxUtility = utility;
                    bestMoveX = offX;
                    bestMoveY = offY;
                    bestFreeNeighbors = freeNeighbors;
                }
            }
        }

        sem_wait(&sync->reader_count_mutex);
        sync->active_readers_count--;
        if (sync->active_readers_count == 0) sem_post(&sync->game_state_mutex);
        sem_post(&sync->reader_count_mutex);
        
        unsigned char move = moveMap[bestMoveY+1][bestMoveX+1];
        write(STDOUT_FILENO, &move, sizeof(move));
    }
    return 0;
}
