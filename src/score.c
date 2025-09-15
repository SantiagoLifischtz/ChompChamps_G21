// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score.h>

int comparePlayers(jugador_t j1, jugador_t j2) {
    // 1. Mayor puntaje gana (orden descendente)
    int cmp = j1.puntaje - j2.puntaje;
    if (cmp == 0) {
        // 2. En caso de empate, menor cantidad de movimientos válidos gana (mayor eficiencia)
        cmp = j2.validRequests - j1.validRequests;
        if (cmp == 0) {
            // 3. Si persiste el empate, menor cantidad de movimientos inválidos gana
            cmp = j2.invalidRequests - j1.invalidRequests;
        }
    }
    return cmp;
}

int *getPlayerOrder(game_state_t *state) {
    int n = state->num_jugadores;
    int *idOrder = malloc(sizeof(int) * n);
    if (!idOrder) return NULL;

    for (int i = 0; i < n; i++) {
        idOrder[i] = i;
    }

    // Selection Sort
    for (int i = 0; i < n - 1; i++) {
        int best = i;
        for (int j = i + 1; j < n; j++) {
            if (comparePlayers(state->jugadores[idOrder[j]], state->jugadores[idOrder[best]]) > 0) {
                best = j;
            }
        }
        if (best != i) {
            int tmp = idOrder[i];
            idOrder[i] = idOrder[best];
            idOrder[best] = tmp;
        }
    }

    return idOrder;
}