#ifndef SEARCH_H
#define SEARCH_H
#include "board.h"

extern uint32_t history[2][64][64];

move_t find_best_move(struct board* board, struct timer* timer, char who, char flags, char infinite);
void search_stop();

#endif
