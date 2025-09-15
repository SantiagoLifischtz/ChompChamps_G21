// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/**
 * @file vista.c
 * @brief Módulo de visualización del juego ChompChamps
 * 
 * Este módulo proporciona la interfaz visual para el juego ChompChamps.
 * Muestra el tablero de juego, estado de jugadores y tabla de posiciones
 * en tiempo real usando memoria compartida y semáforos para sincronización.
 * 
 * @author Grupo 21
 * @date 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <structs.h>
#include <score.h>

/**
 * @brief Códigos de color para salida de terminal
 * 
 * Arreglo de secuencias de escape ANSI para mostrar diferentes jugadores
 * en diferentes colores. Soporta hasta 9 jugadores (MAX_JUGADORES).
 */
static const char* colors[] = {
    "\033[34m",  // Azul
    "\033[31m",  // Rojo
    "\033[32m",  // Verde
    "\033[33m",  // Amarillo
    "\033[35m",  // Magenta
    "\033[36m",  // Cian
    "\033[37m",  // Blanco
    "\033[90m",  // Gris Oscuro
    "\033[93m"   // Amarillo Claro
};

/**
 * @brief Imprime una barra de progreso animada para indicar que el juego está corriendo
 * 
 * Crea un indicador visual de que el juego está ejecutándose activamente mostrando
 * una barra con un carácter móvil que cicla a través de las posiciones.
 * 
 * @param length La longitud de la barra animada
 * @param frame El número de frame actual para la animación
 */
void printAnimatedBar(int length, int frame) {
    // Barra animada para mostrar que el juego sigue corriendo
    int pos = ((frame % 10) + 10) % 10;
    for (int i = 0; i < length; i++)
    {
        if (i%10 != pos) putchar('-');
        else putchar('|');
        putchar(' ');
    }
    putchar('\n');
}

/**
 * @brief Imprime una barra decorativa para la pantalla de fin de juego
 * 
 * Crea una barra separadora visual usando signos iguales para la pantalla final del juego.
 * 
 * @param length La longitud de la barra a imprimir
 */
void printEndgameBar(int length) {
    for (int i = 0; i < length-1; i++)
    {
        printf("==");
    }
    printf("=\n");
}

/**
 * @brief Dibuja el tablero de juego con posiciones actuales de jugadores y celdas visitadas
 * 
 * Renderiza el tablero de juego mostrando:
 * - Posiciones actuales de jugadores como letras coloreadas (A, B, C, etc.)
 * - Celdas previamente visitadas como puntos coloreados
 * - Celdas no visitadas como sus valores numéricos
 * 
 * @param state Puntero al estado actual del juego
 */
void drawBoard(game_state_t *state) {
    if (!state || state->width <= 0 || state->height <= 0) {
        printf("Error: Estado de juego inválido o dimensiones incorrectas\n");
        return;
    }
    
    for (int y=0; y<state->height; y++) {
        for (int x=0; x<state->width; x++) {
            int val = state->tablero[y * state->width + x];
            
            // Verificar si hay un jugador en esta posición actual
            int current_player = -1;
            for (size_t i = 0; i < state->num_jugadores; i++) {
                if (state->jugadores[i].x == x && state->jugadores[i].y == y) {
                    current_player = i;
                    break;
                }
            }
            
            if (current_player != -1) {
                // Posición actual de un jugador: mostrar letra coloreada
                char player_char = 'A' + current_player;
                if (current_player < 9) {
                    printf("%s%c\033[0m ", colors[current_player], player_char);
                } else {
                    printf("%c ", player_char);  // Fallabck para índice de jugador inválido
                }
            } else if (val <= 0) {
                // Posiciones visitadas: 0 para jugador 0, -i para jugador i
                int player_id = -val;
                
                if (player_id < 9) {
                    printf("%s.\033[0m ", colors[player_id]);
                } else {
                    printf(". ");  // Fallback
                }
            } else {
                // Casilleros no visitados: mostrar valores (incluye 0)
                printf("%d ", val);
            }
        }
        putchar('\n');
    }
}

/**
 * @brief Imprime el estado actual del juego y la tabla de posiciones
 * 
 * Muestra una tabla de posiciones formateada con todos los jugadores ordenados por puntaje,
 * incluyendo sus nombres, puntajes, posiciones y estado de bloqueo.
 * 
 * @param state Puntero al estado actual del juego
 * @param title Título a mostrar encima de la tabla de posiciones
 */
void printStatus(game_state_t *state, char *title) {
    if (!state || !title) {
        printf("Error: Parámetros inválidos para printStatus\n");
        return;
    }
    
    printf("\n%28s\n", title);
    jugador_t *players = state->jugadores;
    int *idOrder = getPlayerOrder(state);
    if (!idOrder) return;

    for (size_t i = 0; i < state->num_jugadores; i++) {
        int id = idOrder[i];
        printf("%s%-16s\033[0m | %4u p | (%2d,%2d) |",
                (id < 9) ? colors[id] : "",
                players[id].nombre,
                players[id].puntaje,
                players[id].x,
                players[id].y);
        if (players[id].stuck) {
            printf(" %s(x_x)\033[0m", (id < 9) ? colors[id] : "");
        }
        putchar('\n');
    }
    free(idOrder);
}

/**
 * @brief Muestra el estado final del juego y el ganador
 * 
 * Muestra la tabla de posiciones final, el tablero de juego, y anuncia el ganador
 * con el puntaje más alto.
 * 
 * @param state Puntero al estado actual del juego
 */
void gameEnded(game_state_t *state) {
    if (!state) {
        printf("Error: Estado de juego inválido para gameEnded\n");
        return;
    }
    
    printStatus(state, "=== Game over ===");
    putchar('\n');

    unsigned int maxScore = 0;
    size_t winner = 0;
    for (size_t i=0; i<state->num_jugadores; i++) {
        unsigned int score = state->jugadores[i].puntaje;
        if (score > maxScore) {
            maxScore = score;
            winner = i;
        }
    }
    printEndgameBar(state->width);
    drawBoard(state);
    printEndgameBar(state->width);
    printf("\n=== Winner: [ %s%s\033[0m ] ===\n", 
           (winner < 9) ? colors[winner] : "", 
           state->jugadores[winner].nombre);
}

/**
 * @brief Función principal para el proceso de visualización del juego
 * 
 * Configura las conexiones de memoria compartida, sincroniza con el maestro del juego,
 * y muestra continuamente el estado del juego hasta que termine.
 * 
 * @return 0 en caso de éxito, 1 en caso de error
 */
int main() {
    // Abrir memoria compartida para el estado del juego (solo lectura)
    int shm_state_fd = shm_open("/game_state", O_RDONLY, 0);
    if (shm_state_fd == -1) {
        perror("shm_open game_state");
        return 1;
    }
    
    // Mapear el estado del juego en memoria
    game_state_t *state = mmap(NULL, sizeof(game_state_t),
                               PROT_READ, MAP_SHARED,
                               shm_state_fd, 0);
    if (state == MAP_FAILED) {
        perror("mmap game_state");
        close(shm_state_fd);
        return 1;
    }

    // Abrir memoria compartida para sincronización (lectura-escritura)
    int shm_sync_fd = shm_open("/game_sync", O_RDWR, 0);
    if (shm_sync_fd == -1) {
        perror("shm_open game_sync");
        munmap(state, sizeof(game_state_t));
        close(shm_state_fd);
        return 1;
    }
    
    // Mapear la estructura de sincronización en memoria
    game_sync_t *sync = mmap(NULL, sizeof(game_sync_t),
                             PROT_READ | PROT_WRITE, MAP_SHARED,
                             shm_sync_fd, 0);
    if (sync == MAP_FAILED) {
        perror("mmap game_sync");
        close(shm_sync_fd);
        munmap(state, sizeof(game_state_t));
        close(shm_state_fd);
        return 1;
    }

    fflush(stdout);

    int frameCounter = 0;

    // Bucle principal del juego - esperar actualizaciones del master y mostrar
    while (1) {
        sem_wait(&sync->A);

        if (state->terminado) break;
        printStatus(state, "=== Leaderboard ===");
        putchar('\n');
        
        printAnimatedBar(state->width, -1 - frameCounter);
        drawBoard(state);
        printAnimatedBar(state->width, frameCounter++);
        fflush(stdout);

        sem_post(&sync->B);
    }
    
    // El juego ha terminado - mostrar estado final
    gameEnded(state);
    sem_post(&sync->B);

    // Limpiar recursos
    munmap(state, sizeof(game_state_t));
    munmap(sync, sizeof(game_sync_t));
    close(shm_state_fd);
    close(shm_sync_fd);

    return 0;
}
