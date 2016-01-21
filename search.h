#ifndef SEARCH_H
#define SEARCH_H
#include "board.h"

move_t find_best_move(struct board* board, char who, int maxt, char flags);
void search_stop();

#endif
