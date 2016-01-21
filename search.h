#ifndef SEARCH_H
#define SEARCH_H
#include "board.h"

move_t find_best_move(struct board* board, struct timer* timer, char who, char flags);
void search_stop();

#endif
