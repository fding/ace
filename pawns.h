#ifndef PAWNS_H
#define PAWNS_H
#include <stdint.h>
#include "board.h"

struct pawn_structure {
    uint64_t pawn_hash;
    uint64_t passed_pawns[2];
    uint64_t holes[2];
    int32_t score;
    int32_t score_eg;
};

struct pawn_structure * evaluate_pawns(struct board* board);

#endif
