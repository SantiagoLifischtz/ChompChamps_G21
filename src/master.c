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

// Estructura para parámetros de configuración
typedef struct {
    int width;
    int height;
    int delay;
    int timeout;
    unsigned int seed;
    char *view_path;
    char *player_paths[MAX_JUGADORES];
    int num_players;
} config_t;
// -----------------------

// Función para mostrar ayuda
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

// Función para parsear argumentos
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

    // ----- shm estado -----
    int shm_state_fd = shm_open("/game_state", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_state_fd, sizeof(game_state_t));
    game_state_t *state = mmap(NULL, sizeof(game_state_t),
                               PROT_READ | PROT_WRITE, MAP_SHARED,
                               shm_state_fd, 0);

    state->width = config.width;
    state->height = config.height;
    state->num_jugadores = config.num_players;
    state->terminado = 0;

    // tablero inicial
    srand(config.seed);
    for (int y=0; y<state->height; y++) {
        for (int x=0; x<state->width; x++) {
            state->tablero[y][x] = 1 + rand() % 9;
        }
    }
    


    // jugadores iniciales - distribuirlos en esquinas/bordes del tablero
    for (int i = 0; i < config.num_players; i++) {
        switch (i % 4) {
            case 0: // esquina superior izquierda
                state->jugadores[i].x = 0;
                state->jugadores[i].y = 0;
                break;
            case 1: // esquina inferior derecha
                state->jugadores[i].x = state->width - 1;
                state->jugadores[i].y = state->height - 1;
                break;
            case 2: // esquina superior derecha
                state->jugadores[i].x = state->width - 1;
                state->jugadores[i].y = 0;
                break;
            case 3: // esquina inferior izquierda
                state->jugadores[i].x = 0;
                state->jugadores[i].y = state->height - 1;
                break;
        }
        state->jugadores[i].puntaje = 0;
        state->tablero[state->jugadores[i].y][state->jugadores[i].x] = -(i+1);

    }

    // ----- shm sync -----
    int shm_sync_fd = shm_open("/game_sync", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_sync_fd, sizeof(game_sync_t));
    game_sync_t *sync = mmap(NULL, sizeof(game_sync_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_sync_fd, 0);

    sem_init(&sync->A, 1, 0);
    sem_init(&sync->B, 1, 0);

    sem_init(&sync->C, 1, 1);
    sem_init(&sync->D, 1, 1);
    sem_init(&sync->E, 1, 1);
    sync->F = 0;
    for (int i = 0; i < config.num_players; i++) {
        sem_init(&(sync->G[i]), 1, 0);
    }

    // ----- pipes + jugadores -----
    for (int i = 0; i < config.num_players; i++) {
        pipe(pipes[i]);
        pid_t pid = fork();
        if (pid == 0) {
            // hijo = jugador
            close(pipes[i][0]);                     // cierro lectura
            dup2(pipes[i][1], STDOUT_FILENO);       // stdout → escritura del pipe

            // Usar la ruta del jugador especificada en la configuración
            char player_name[64];
            snprintf(player_name, sizeof(player_name), "jugador%d", i);
            execl(config.player_paths[i], player_name, NULL);
            perror("execl jugador");
            exit(1); // solo llega si execl falla
        } else if (pid > 0) {
            jugadores[i] = pid;
            state->jugadores[i].pid = pid;
            close(pipes[i][1]); // master no escribe en este pipe
            snprintf(state->jugadores[i].nombre, PLAYER_NAME_LENGTH, config.player_paths[i]);
        } else {
            perror("fork jugador");
            exit(1);
        }
    }

    // ----- vista -----
    if (config.view_path != NULL) {
        vista = fork();
        if (vista == 0) {
            execl(config.view_path, "vista", NULL);
            perror("execl vista");
            exit(1);
        }
        
        // Enviar estado inicial a la vista

        sem_post(&sync->A);
        sem_wait(&sync->B);
    }

    // ----- bucle master -----
    int steps = 0;
    int active_players[MAX_JUGADORES]; // Array para rastrear jugadores activos

    // Inicializar todos los jugadores como activos
    for (int i = 0; i < config.num_players; i++) {
        active_players[i] = 1;
        sem_post(&(sync->G[i]));
        printf("\n");
    }

    time_t last_movement_time = time(NULL); // Tiempo del último movimiento de cualquier jugador    
    while (!state->terminado) {

        sem_wait(&sync->C);
        sem_wait(&sync->D);
        
        // Configurar timeout para lectura de todos los jugadores
        fd_set readfds;
        struct timeval timeout;
        int max_fd = -1;
        
        FD_ZERO(&readfds);
        int active_count = 0;
        for (int i = 0; i < config.num_players; i++) {
            if (active_players[i]) {
                FD_SET(pipes[i][0], &readfds);
                if (pipes[i][0] > max_fd) max_fd = pipes[i][0];
                active_count++;
            }
        }
        
        // Si no hay jugadores activos, terminar
        // if (active_count == 0) {
        //     printf("[Master] Todos los jugadores han terminado\n");
        //     break;
        // }
        
        // Calcular cuánto tiempo queda del timeout global
        time_t current_time = time(NULL);
        long remaining_timeout = config.timeout+1 - (current_time - last_movement_time);
        
        // Si no queda tiempo, usar timeout de 0 para que select retorne inmediatamente
        if (remaining_timeout <= 0) {
            printf("[Debug] No time left, forcing immediate timeout\n");
            remaining_timeout = 0;
            state->terminado = 1;
        }
        
        // Usar el tiempo restante como timeout para select
        timeout.tv_sec = remaining_timeout;
        timeout.tv_usec = 0;
        
        int ready = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if (ready < 0) {
            perror("select error");
            break;
        } else if (ready == 0) {
            // Timeout - no hay movimientos en el tiempo esperado
            printf("[Master] Timeout global alcanzado (%ld segundos sin movimientos, límite: %d)\n", 
                   current_time - last_movement_time, config.timeout);
            break;
        }
        
        // Procesar movimientos de jugadores que están listos
        for (int i = 0; i < config.num_players; i++) {
            if (!active_players[i] || !FD_ISSET(pipes[i][0], &readfds)) {
                continue; // Este jugador no está activo o no tiene movimiento listo
            }
            
            unsigned char move;
            ssize_t n = read(pipes[i][0], &move, sizeof(move));
            if (n <= 0) {
                // Jugador terminó o error de lectura - marcar como inactivo
                printf("[Master] Jugador %d terminó o se desconectó\n", i);
                active_players[i] = 0;
                continue;
            }
            
            unsigned short x = state->jugadores[i].x;
            unsigned short y = state->jugadores[i].y;
            unsigned short nx = x, ny = y;

            // interpretar movimiento (0 arriba, 2 derecha, 4 abajo, 6 izquierda, 1 arriba-derecha, 3 abajo-derecha, 5 abajo-izquierda, 7 arriba-izquierda)
            switch (move) {
                case 0: // arriba
                    if (y > 0) ny--;
                    break;
                case 1: // arriba-derecha
                    if (y > 0 && x + 1 < state->width) { ny--; nx++; }
                    break;
                case 2: // derecha
                    if (x + 1 < state->width) nx++;
                    break;
                case 3: // abajo-derecha
                    if (y + 1 < state->height && x + 1 < state->width) { ny++; nx++; }
                    break;
                case 4: // abajo
                    if (y + 1 < state->height) ny++;
                    break;
                case 5: // abajo-izquierda
                    if (y + 1 < state->height && x > 0) { ny++; nx--; }
                    break;
                case 6: // izquierda
                    if (x > 0) nx--;
                    break;
                case 7: // arriba-izquierda
                    if (y > 0 && x > 0) { ny--; nx--; }
                    break;
                default:
                    // movimiento inválido, no hacer nada
                    break;
            }

            // Verificar si la posición de destino es válida (no ocupada)
            int destino_valor = state->tablero[ny][nx];
            
            // Solo permitir el movimiento si:
            // 1. La posición tiene un valor positivo (comida disponible)
            // 2. O la posición es 0 (vacía)
            // NO permitir si es negativo (ocupada por jugador actual o visitada)
            if (destino_valor > 0 || destino_valor == 0) {
                // Movimiento válido - actualizar puntaje y tablero
                int reward = destino_valor;
                if (reward > 0) state->jugadores[i].puntaje += reward;
                state->jugadores[i].validRequests++;
                
                // Marcar antigua posición como visitada (si era posición actual)
                if (state->tablero[y][x] == -(i+1)) {
                    state->tablero[y][x] = -(i+11);  // Visitada: -11 para jugador 0, -12 para jugador 1
                }
                
                // Marcar nueva posición como posición actual del jugador
                state->tablero[ny][nx] = -(i+1);  // Actual: -1 para jugador 0, -2 para jugador 1
                state->jugadores[i].x = nx;
                state->jugadores[i].y = ny;

                last_movement_time = time(NULL);
            } else {
                // Movimiento inválido - mantener posición actual
                state->jugadores[i].invalidRequests++;
            }

            sem_post(&(sync->G[i]));
        }
        // avisar a vista (solo si hay vista activa)
        if (config.view_path != NULL) {
            sem_post(&sync->A);
            sem_wait(&sync->B);
        }
        steps++;

        sem_post(&sync->D);
        sem_post(&sync->C);
        
        // El delay se ejecuta en cada ciclo del bucle principal, para pausar entre movimientos.
        if (config.delay > 0) {
            // sleep acepta segundos, usleep acepta microsegundos (1ms = 1000us)
            usleep(config.delay * 1000);
        }
    }
    
    // Terminar procesos activamente para evitar esperas innecesarias
    for (int i = 0; i < config.num_players; i++) {
        kill(jugadores[i], SIGTERM);
        waitpid(jugadores[i], NULL, 0);
    }
    if (vista > 0) {
        kill(vista, SIGTERM);
        waitpid(vista, NULL, 0);
    }

    shm_unlink("/game_state");
    shm_unlink("/game_sync");

    return 0;
}
