#ifndef SCORE_H
#define SCORE_H

#include <structs.h>

/**
 * @brief Crea un arreglo de índices de jugadores ordenados por puntaje (mayor primero)
 * 
 * Asigna y retorna un nuevo arreglo conteniendo índices de jugadores ordenados por
 * sus puntajes en orden descendente. El llamador es responsable de liberar
 * la memoria retornada.
 * 
 * @param state Puntero al estado actual del juego
 * @return Puntero al arreglo asignado de índices de jugadores, o NULL en caso de fallo
 */
int *getPlayerOrder(game_state_t *state);

#endif