// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/**
 * @file master.c
 * @brief Módulo maestro del juego ChompChamps
 * 
 * Este módulo actúa como el coordinador principal del juego, manejando la lógica
 * del juego, la sincronización entre jugadores, la vista y el estado global.
 * Utiliza memoria compartida y semáforos para la comunicación entre procesos.
 * 
 * @author Grupo 21
 * @date 2025
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>
#include <getopt.h>
#include <string.h>
#include <sys/select.h>
#include <signal.h>
#include <structs.h>
#include <score.h>
#include <math.h>

// Constantes de configuración del juego
#define MAX_JUGADORES 9
#define MIN_JUGADORES 1
#define DEFAULT_NUM_JUGADORES 2
#define MAX_WIDTH 100
#define MAX_HEIGHT 100
#define MIN_WIDTH 10
#define MIN_HEIGHT 10
#define DEFAULT_WIDTH 10
#define DEFAULT_HEIGHT 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 1

/**
 * @brief Estructura para parámetros de configuración del juego
 */
typedef struct {
    int width;                                    ///< Ancho del tablero
    int height;                                   ///< Alto del tablero
    int delay;                                    ///< Delay entre movimientos en ms
    int timeout;                                  ///< Timeout para movimientos en segundos
    unsigned int seed;                            ///< Semilla para generación aleatoria
    char *view_path;                              ///< Ruta del binario de la vista
    char *player_paths[MAX_JUGADORES];            ///< Rutas de los binarios de jugadores
    int num_players;                              ///< Número de jugadores
} config_t;

// -----------------------

/**
 * @brief Muestra la ayuda del programa
 * 
 * @param program_name Nombre del programa para mostrar en la ayuda
 */
void show_help(const char *program_name) {
    printf("Uso: %s [opciones] -p jugador1 [jugador2] ...\n", program_name);
    printf("Opciones:\n");
    printf("  -w width    Ancho del tablero (default: %d, mínimo: %d)\n", DEFAULT_WIDTH, MIN_WIDTH);
    printf("  -h height   Alto del tablero (default: %d, mínimo: %d)\n", DEFAULT_HEIGHT, MIN_HEIGHT);
    printf("  -d delay    Milisegundos entre impresiones (default: %d)\n", DEFAULT_DELAY);
    printf("  -t timeout  Timeout en segundos para movimientos (default: %d)\n", DEFAULT_TIMEOUT);
    printf("  -s seed     Semilla para generación del tablero (default: time(NULL))\n");
    printf("  -v view     Ruta del binario de la vista (default: sin vista)\n");
    printf("  -p players  Rutas de los binarios de los jugadores (mínimo: %d, máximo: %d)\n", MIN_JUGADORES, MAX_JUGADORES);
    printf("  --help      Mostrar esta ayuda\n");
}

/**
 * @brief Parsea los argumentos de línea de comandos
 * 
 * @param argc Número de argumentos
 * @param argv Array de argumentos
 * @param config Estructura de configuración a llenar
 * @return 0 en éxito, 1 para --help, -1 en error
 */
int parse_arguments(int argc, char *argv[], config_t *config) {
    // Inicializar configuración por defecto
    config->width = DEFAULT_WIDTH;
    config->height = DEFAULT_HEIGHT;
    config->delay = DEFAULT_DELAY;
    config->timeout = DEFAULT_TIMEOUT;
    config->seed = time(NULL);
    config->view_path = NULL;
    config->num_players = 0;
    
    for (int i = 0; i < MAX_JUGADORES; i++) {
        config->player_paths[i] = NULL;
    }
    
    int opt;
    int player_mode = 0;
    
    // Definir opciones largas
    static struct option long_options[] = {
        {"help", no_argument, 0, 0},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "w:h:d:t:s:v:p:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'w':
                config->width = atoi(optarg);
                if (config->width < MIN_WIDTH) {
                    fprintf(stderr, "Error: ancho mínimo es %d\n", MIN_WIDTH);
                    return -1;
                }
                break;
            case 'h':
                config->height = atoi(optarg);
                if (config->height < MIN_HEIGHT) {
                    fprintf(stderr, "Error: alto mínimo es %d\n", MIN_HEIGHT);
                    return -1;
                }
                break;
            case 'd':
                config->delay = atoi(optarg);
                if (config->delay < 0) {
                    fprintf(stderr, "Error: delay debe ser >= 0\n");
                    return -1;
                }
                break;
            case 't':
                config->timeout = atoi(optarg);
                if (config->timeout < 0) {
                    fprintf(stderr, "Error: timeout debe ser >= 0\n");
                    return -1;
                }
                break;
            case 's':
                config->seed = (unsigned int)atoi(optarg);
                break;
            case 'v':
                config->view_path = optarg;
                break;
            case 'p':
                player_mode = 1;
                // El primer jugador está en optarg
                if (config->num_players < MAX_JUGADORES) {
                    config->player_paths[config->num_players++] = optarg;
                } else {
                    fprintf(stderr, "Error: máximo %d jugadores\n", MAX_JUGADORES);
                    return -1;
                }
                break;
            case 0:
                // Opción larga --help
                show_help(argv[0]);
                return 1;
            default:
                show_help(argv[0]);
                return -1;
        }
    }
    
    // Procesar argumentos restantes como jugadores adicionales (solo si estamos en modo -p)
    if (player_mode) {
        while (optind < argc && config->num_players < MAX_JUGADORES) {
            config->player_paths[config->num_players++] = argv[optind++];
        }
    }
    
    // Validar número mínimo de jugadores
    if (config->num_players < MIN_JUGADORES) {
        fprintf(stderr, "Error: se requiere al menos %d jugador\n", MIN_JUGADORES);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Retorna el contenido de un string después de la última aparición de un separador
 * 
 * @param separator Carácter separador a buscar
 * @param str String en el que buscar
 * @return Puntero al contenido después del último separador
 */
char *textAfter(char separator, char *str) {
    char *lastSeparator = NULL;
    while (*str)
    {
        if (*str == separator) {
            lastSeparator = str;
        }
        str++;
    }
    return lastSeparator+1;
}

/**
 * @brief Mueve un jugador a una posición y la ocupa en el tablero
 * 
 * @param state Estado actual del juego
 * @param playerId ID del jugador a mover
 * @param targetX Coordenada X de destino
 * @param targetY Coordenada Y de destino
 * @return 0 si el movimiento es inválido, el puntaje de la posición si es válido
 */
int movePlayer(game_state_t *state, int playerId, unsigned short targetX, unsigned short targetY) {
    if (targetX >= state->width || targetY >= state->height) {
        return 0;
    }
    int score = state->tablero[targetY * state->width + targetX];
    if (score <= 0) {
        return 0;
    }
    state->jugadores[playerId].x = targetX;
    state->jugadores[playerId].y = targetY;
    state->tablero[targetY * state->width + targetX] = -playerId;
    return score;
}

/**
 * @brief Verifica si es posible escapar de una posición
 * 
 * @param state Estado actual del juego
 * @param centerX Coordenada X central
 * @param centerY Coordenada Y central
 * @return 1 si es escapable, 0 si no
 */
int isEscapable(game_state_t *state, unsigned short centerX, unsigned short centerY) {
    for (int offY = -1; offY <= 1; offY++)
    {
        int y = centerY+offY;
        if (y < 0 || y >= state->height) continue;
        for (int offX = -1; offX <= 1; offX++)
        {
            int x = centerX+offX;
            if (offX == 0 && offY == 0) continue;
            if (x < 0 || x >= state->width) continue;

            if (state->tablero[y*state->width+x] > 0) return 1;
        }
    }
    return 0;
}

/**
 * @brief Verifica si un jugador está atascado
 * 
 * @param state Estado actual del juego
 * @param playerId ID del jugador a verificar
 * @return 1 si está atascado, 0 si no
 */
int isStuck(game_state_t *state, int playerId) {
    jugador_t player = state->jugadores[playerId];
    return player.stuck || !isEscapable(state, player.x, player.y);
}

/**
 * @brief Establece las posiciones iniciales de los jugadores en el tablero
 * 
 * Coloca a los jugadores en posiciones distribuidas circularmente alrededor del centro
 * del tablero, evitando que empiecen en la misma posición.
 * 
 * @param state Estado actual del juego
 */
void setStartingPositions(game_state_t *state) {
    double angleStep = 2*M_PI/state->num_jugadores;
    double angle = (rand() / (double) RAND_MAX) * 2*M_PI;

    unsigned short centerX = state->width/2;
    unsigned short centerY = state->height/2;
    double radius = (state->width < state->height ? state->width : state->height)*0.375;

    for (size_t i = 0; i < state->num_jugadores; i++)
    {
        unsigned short x = centerX + cos(angle)*radius;
        unsigned short y = centerY + sin(angle)*radius;
        movePlayer(state, i, x, y);
        angle += angleStep;
    }
}

/**
 * @brief Inicializa la memoria compartida para el estado del juego y sincronización
 * 
 * @param state Puntero al puntero del estado del juego
 * @param sync Puntero al puntero de la estructura de sincronización
 * @param config Configuración del juego
 * @return 0 en éxito, -1 en error
 */
int initialize_shared_memory(game_state_t **state, game_sync_t **sync, const config_t *config) {
    if (!state || !sync || !config) {
        fprintf(stderr, "Error: Parámetros inválidos para initialize_shared_memory\n");
        return -1;
    }
    
    // Crear memoria compartida para el estado del juego
    int shm_state_fd = shm_open("/game_state", O_CREAT | O_RDWR, 0666);
    if (shm_state_fd == -1) {
        perror("shm_open game_state");
        return -1;
    }
    
    if (ftruncate(shm_state_fd, sizeof(game_state_t)) == -1) {
        perror("ftruncate game_state");
        close(shm_state_fd);
        return -1;
    }
    
    *state = mmap(NULL, sizeof(game_state_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_state_fd, 0);
    if (*state == MAP_FAILED) {
        perror("mmap game_state");
        close(shm_state_fd);
        return -1;
    }

    // Inicializar estado del juego
    (*state)->width = config->width;
    (*state)->height = config->height;
    (*state)->num_jugadores = config->num_players;
    (*state)->terminado = 0;

    // Generar tablero inicial
    srand(config->seed);
    for (int y = 0; y < (*state)->height; y++) {
        for (int x = 0; x < (*state)->width; x++) {
            (*state)->tablero[y * (*state)->width + x] = 1 + rand() % 9;
        }
    }
    
    // Establecer posiciones iniciales de jugadores
    setStartingPositions(*state);
    for (int i = 0; i < config->num_players; i++) {
        (*state)->jugadores[i].puntaje = 0;
        (*state)->jugadores[i].stuck = 0;
        (*state)->jugadores[i].validRequests = 0;
        (*state)->jugadores[i].invalidRequests = 0;
    }

    // Crear memoria compartida para sincronización
    int shm_sync_fd = shm_open("/game_sync", O_CREAT | O_RDWR, 0666);
    if (shm_sync_fd == -1) {
        perror("shm_open game_sync");
        munmap(*state, sizeof(game_state_t));
        close(shm_state_fd);
        return -1;
    }
    
    if (ftruncate(shm_sync_fd, sizeof(game_sync_t)) == -1) {
        perror("ftruncate game_sync");
        close(shm_sync_fd);
        munmap(*state, sizeof(game_state_t));
        close(shm_state_fd);
        return -1;
    }
    
    *sync = mmap(NULL, sizeof(game_sync_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_sync_fd, 0);
    if (*sync == MAP_FAILED) {
        perror("mmap game_sync");
        close(shm_sync_fd);
        munmap(*state, sizeof(game_state_t));
        close(shm_state_fd);
        return -1;
    }

    return 0;
}

/**
 * @brief Inicializa todos los semáforos necesarios para la sincronización
 * 
 * @param sync Estructura de sincronización
 * @param num_players Número de jugadores
 * @return 0 en éxito, -1 en error
 */
int initialize_semaphores(game_sync_t *sync, int num_players) {
    if (!sync || num_players <= 0 || num_players > MAX_JUGADORES) {
        fprintf(stderr, "Error: Parámetros inválidos para initialize_semaphores\n");
        return -1;
    }
    
    if (sem_init(&sync->A, 1, 0) != 0) {
        perror("sem_init A");
        return -1;
    }
    if (sem_init(&sync->B, 1, 0) != 0) {
        perror("sem_init B");
        sem_destroy(&sync->A);
        return -1;
    }
    if (sem_init(&sync->C, 1, 1) != 0) {
        perror("sem_init C");
        sem_destroy(&sync->A);
        sem_destroy(&sync->B);
        return -1;
    }
    if (sem_init(&sync->D, 1, 1) != 0) {
        perror("sem_init D");
        sem_destroy(&sync->A);
        sem_destroy(&sync->B);
        sem_destroy(&sync->C);
        return -1;
    }
    if (sem_init(&sync->E, 1, 1) != 0) {
        perror("sem_init E");
        sem_destroy(&sync->A);
        sem_destroy(&sync->B);
        sem_destroy(&sync->C);
        sem_destroy(&sync->D);
        return -1;
    }
    
    sync->F = 0;
    
    for (int i = 0; i < num_players; i++) {
        if (sem_init(&(sync->G[i]), 1, 0) != 0) {
            perror("sem_init G");
            // Limpiar semáforos ya inicializados
            sem_destroy(&sync->A);
            sem_destroy(&sync->B);
            sem_destroy(&sync->C);
            sem_destroy(&sync->D);
            sem_destroy(&sync->E);
            for (int j = 0; j < i; j++) {
                sem_destroy(&(sync->G[j]));
            }
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief Lanza el proceso de vista
 * 
 * @param vista Puntero al PID del proceso de vista
 * @param view_path Ruta del binario de la vista
 * @return 0 en éxito, -1 en error
 */
int launch_view_process(pid_t *vista, const char *view_path) {
    if (!vista || !view_path) {
        fprintf(stderr, "Error: Parámetros inválidos para launch_view_process\n");
        return -1;
    }
    
    *vista = fork();
    if (*vista == 0) {
        // Proceso hijo = vista
        execl(view_path, "vista", NULL);
        perror("execl vista");
        exit(1);
    } else if (*vista < 0) {
        perror("fork vista");
        return -1;
    }
    return 0;
}

/**
 * @brief Lanza los procesos de jugadores
 * 
 * @param jugadores Array de PIDs de jugadores
 * @param pipes Array de pipes para comunicación con jugadores
 * @param config Configuración del juego
 * @param state Estado del juego
 * @return 0 en éxito, -1 en error
 */
int launch_player_processes(pid_t *jugadores, int pipes[MAX_JUGADORES][2], const config_t *config, game_state_t *state) {
    if (!jugadores || !pipes || !config || !state) {
        fprintf(stderr, "Error: Parámetros inválidos para launch_player_processes\n");
        return -1;
    }
    
    for (int i = 0; i < config->num_players; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return -1;
        }
        
        pid_t pid = fork();
        if (pid == 0) {
            // Proceso hijo = jugador
            
            // CRÍTICO: Cerrar TODOS los pipes heredados de otros jugadores
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]); // cerrar lectura de pipes anteriores
            }
            
            // Cerrar extremo de lectura del pipe propio
            close(pipes[i][0]);
            
            // Redirigir stdout al extremo de escritura del pipe
            dup2(pipes[i][1], STDOUT_FILENO);
            
            // Cerrar el descriptor original después del dup2
            close(pipes[i][1]);

            // Usar la ruta del jugador especificada en la configuración
            char player_name[64];
            snprintf(player_name, sizeof(player_name), "jugador%d", i);
            execl(config->player_paths[i], player_name, NULL);
            perror("execl jugador");
            exit(1); // solo llega si execl falla
        } else if (pid > 0) {
            jugadores[i] = pid;
            state->jugadores[i].pid = pid;
            close(pipes[i][1]); // Master no escribe en este pipe
            snprintf(state->jugadores[i].nombre, PLAYER_NAME_LENGTH, "%s", textAfter('/', config->player_paths[i]));
        } else {
            perror("fork jugador");
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Procesa los movimientos de los jugadores en una iteración
 * 
 * @param state Estado del juego
 * @param sync Estructura de sincronización
 * @param pipes Pipes de comunicación con jugadores
 * @param active_players Array de jugadores activos (puede ser NULL)
 * @param config Configuración del juego
 * @param last_movement_time Tiempo del último movimiento
 * @return 0 en éxito, -1 en error
 */
int process_player_moves(game_state_t *state, game_sync_t *sync, int pipes[MAX_JUGADORES][2], 
                        int active_players[], const config_t *config, time_t *last_movement_time) {
    if (!state || !sync || !pipes || !config || !last_movement_time) {
        fprintf(stderr, "Error: Parámetros inválidos para process_player_moves\n");
        return -1;
    }
    
    // Configurar select para leer de todos los jugadores
    fd_set readfds;
    int max_fd = -1;
    
    FD_ZERO(&readfds);
    int active_count = 0;
    for (int i = 0; i < config->num_players; i++) {
        if (active_players == NULL || active_players[i]) {
            FD_SET(pipes[i][0], &readfds);
            if (pipes[i][0] > max_fd) max_fd = pipes[i][0];
            active_count++;
        }
    }
    
    struct timeval timeout = {0, 0};
    int ready = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
    if (ready < 0) {
        perror("select error");
        return -1;
    }
    
    // Procesar movimientos de jugadores listos
    for (int i = 0; i < config->num_players; i++) {
        if ((active_players != NULL && !active_players[i]) || !FD_ISSET(pipes[i][0], &readfds)) {
            continue;
        }
        
        unsigned char move;
        ssize_t n = read(pipes[i][0], &move, sizeof(move));
        if (n <= 0) {
            // Jugador terminó o error de lectura
            printf("[Master] Jugador %d terminó o se desconectó\n", i);
            if (active_players != NULL) {
                active_players[i] = 0;
            }
            continue;
        }

        // Sincronización para acceso al estado
        sem_wait(&sync->C);
        sem_wait(&sync->D);
        sem_post(&sync->C);
        
        unsigned short x = state->jugadores[i].x;
        unsigned short y = state->jugadores[i].y;
        unsigned short nx = x, ny = y;

        // Interpretar movimiento
        switch (move) {
            case 0: ny--; break;                    // arriba
            case 1: { ny--; nx++; } break;          // arriba-derecha
            case 2: nx++; break;                    // derecha
            case 3: { ny++; nx++; } break;          // abajo-derecha
            case 4: ny++; break;                    // abajo
            case 5: { ny++; nx--; } break;          // abajo-izquierda
            case 6: nx--; break;                    // izquierda
            case 7: { ny--; nx--; } break;          // arriba-izquierda
            default: break;                         // movimiento inválido
        }

        // Verificar y ejecutar movimiento
        int moveResult = movePlayer(state, i, nx, ny);
        
        if (moveResult > 0) {
            state->jugadores[i].validRequests++;
            state->jugadores[i].puntaje += moveResult;
            *last_movement_time = time(NULL);
        } else {
            state->jugadores[i].invalidRequests++;
        }

        // Verificar si el jugador está atascado
        if (isStuck(state, i)) {
            state->jugadores[i].stuck = 1;
            if (active_players != NULL) {
                active_players[i] = 0;
            }
        } else {
            sem_post(&(sync->G[i]));
        }

        sem_post(&sync->D);
    }

    // Verificar si el juego debe terminar
    if (active_players != NULL) {
        int remaining_players = 0;
        for (int i = 0; i < config->num_players; i++) {
            if (active_players[i]) remaining_players++;
        }
        if (remaining_players == 0) {
            state->terminado = 1;
        }
    }
    
    // Verificar timeout global
    time_t current_time = time(NULL);
    if (config->timeout > 0 && (current_time - *last_movement_time) >= config->timeout) {
        state->terminado = 1;
    }

    return 0;
}

void printScores(game_state_t *state) {

    jugador_t *players = state->jugadores;
    printf("Final scores: \n");
    printf("|------------------|------|\n");
    for (size_t i = 0; i < state->num_jugadores; i++) {
        printf("| %-16s | %4u |\n", players[i].nombre, players[i].puntaje);
        printf("|------------------|------|\n");
    }
}

/**
 * @brief Limpia todos los recursos utilizados por el juego
 * 
 * @param state Estado del juego
 * @param sync Estructura de sincronización
 * @param jugadores Array de PIDs de jugadores
 * @param vista PID del proceso de vista
 * @param pipes Array de pipes
 * @param num_players Número de jugadores
 */
void cleanup_resources(game_state_t *state, game_sync_t *sync, pid_t *jugadores, 
                      pid_t vista, int pipes[MAX_JUGADORES][2], int num_players) {
    // Terminar procesos de jugadores
    for (int i = 0; i < num_players; i++) {
        if (jugadores[i] > 0) {
            kill(jugadores[i], SIGTERM);
            waitpid(jugadores[i], NULL, 0);
        }
    }
    
    // Terminar proceso de vista
    if (vista > 0) {
        kill(vista, SIGTERM);
        waitpid(vista, NULL, 0);
    }

    // Cerrar pipes
    for (int i = 0; i < num_players; i++) {
        close(pipes[i][0]);
    }

    // Destruir semáforos
    if (sync != NULL) {
        sem_destroy(&sync->A);
        sem_destroy(&sync->B);
        sem_destroy(&sync->C);
        sem_destroy(&sync->D);
        sem_destroy(&sync->E);
        for (int i = 0; i < num_players; i++) {
            sem_destroy(&(sync->G[i]));
        }
    }

    // Desmapear memoria compartida
    if (state != NULL) {
        munmap(state, sizeof(game_state_t));
    }
    if (sync != NULL) {
        munmap(sync, sizeof(game_sync_t));
    }
    
    // Desvincular memoria compartida
    shm_unlink("/game_state");
    shm_unlink("/game_sync");
}

/**
 * @brief Función principal del maestro del juego
 * 
 * Coordina la ejecución del juego, inicializa recursos compartidos,
 * lanza procesos de jugadores y vista, y maneja la lógica principal del juego.
 * 
 * @param argc Número de argumentos de línea de comandos
 * @param argv Array de argumentos de línea de comandos
 * @return 0 en éxito, 1 en error
 */
int main(int argc, char *argv[]) {
    config_t config;
    
    // Parsear argumentos
    int parse_result = parse_arguments(argc, argv, &config);
    if (parse_result != 0) {
        return parse_result == 1 ? 0 : 1; // 1 es para --help (salida exitosa)
    }
    
    int pipes[MAX_JUGADORES][2];
    pid_t jugadores[MAX_JUGADORES];
    pid_t vista = -1;
    game_state_t *state = NULL;
    game_sync_t *sync = NULL;

    // Inicializar memoria compartida
    if (initialize_shared_memory(&state, &sync, &config) != 0) {
        fprintf(stderr, "Error: No se pudo inicializar la memoria compartida\n");
        return 1;
    }

    // Inicializar semáforos
    if (initialize_semaphores(sync, config.num_players) != 0) {
        fprintf(stderr, "Error: No se pudieron inicializar los semáforos\n");
        cleanup_resources(state, sync, jugadores, vista, pipes, config.num_players);
        return 1;
    }

    // Lanzar proceso de vista si se envió por argumentos
    if (config.view_path != NULL) {
        if (launch_view_process(&vista, config.view_path) != 0) {
            fprintf(stderr, "Error: No se pudo lanzar el proceso de vista\n");
            cleanup_resources(state, sync, jugadores, vista, pipes, config.num_players);
            return 1;
        }
    }

    // Lanzar procesos de jugadores
    if (launch_player_processes(jugadores, pipes, &config, state) != 0) {
        fprintf(stderr, "Error: No se pudieron lanzar los procesos de jugadores\n");
        cleanup_resources(state, sync, jugadores, vista, pipes, config.num_players);
        return 1;
    }

    // Enviar estado inicial a la vista si existe
    if (config.view_path != NULL) {
        sem_post(&sync->A);
        sem_wait(&sync->B);
    }

    // Inicializar jugadores como activos
    int active_players[MAX_JUGADORES];
    for (int i = 0; i < config.num_players; i++) {
        sem_post(&(sync->G[i]));
        active_players[i] = 1;
    }

    if (config.delay > 0) {
        usleep(config.delay * 1000);
    }
    
    // Bucle principal del juego
    time_t last_movement_time = time(NULL);
    while (!state->terminado) {
        if (process_player_moves(state, sync, pipes, active_players, &config, &last_movement_time) != 0) {
            break;
        }
        
        // Avisar a vista si existe
        if (config.view_path != NULL) {
            sem_post(&sync->A);
            sem_wait(&sync->B);
        }
        
        // Delay entre movimientos
        if (config.delay > 0) {
            usleep(config.delay * 1000);
        }
    }

    // Si no hay vista, el master se encarga de imprimir los resultados
    if (config.view_path == NULL) {
        printScores(state);
    }
    
    // Limpiar recursos
    cleanup_resources(state, sync, jugadores, vista, pipes, config.num_players);
    return 0;
}