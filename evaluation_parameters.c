#include "evaluation_parameters.h"

// All tables are from white's perspective
// We give a bonus for all pieces the closer they move to the opposite side

int knight_table[64] = {
    -30, -15, -15, -15, -15, -15, -15, -30,
     -10,  -5,  0,  0,  0, 0,  -5,  -10,
     0,  15,  25,  35,  35,  25,  15,  0,
    -10, 10,  20,  30,  30,  20, 10, -10,
    -10, 5,  15,  20,  20,  15, 5, -10,
    -15, 0,  10,  15,  15,  10, 0, -15,
    -20, -15, -10, -10, -10, -10, -15, -20,
    -40, -20, -15, -15, -15, -15, -20, -40,
};

int bishop_table[64] = {
    -15, -10, -12, -14, -14, -12, -10, -15,
    -13,  5,  -5,  -5,  -5,  -5,  5, -13,
    -5,  10,  8,  5,  5,  8,  10, -5,
     0,  15,  13,  10,  10,  13,  15, 0,
     2,  15,  13,  10,  10,  13,  15, 2,
    -10,  5,  0,  -2,  -2,  0,  5, -10,
    -5,  10,  8,  5,  5,  8,  10, -5,
    -15, -10, -12, -14, -14, -12, -10, -15,
};

int rook_table[64] = {
    -10, -5, 0, 5, 5, 0, -5, -10,
    -10, -1, 4, 10, 10, 4, -1, -10,
    -10, -1, 4, 10, 10, 4, -1, -10,
    -10, -2, 2, 8, 8, 3, -2, -10,
    -10, -3, 2, 7, 7, 2, -3, -10,
    -10, -4, 1, 6, 6, 1, -4, -10,
    -10, -5, 0, 5, 5, 0, -5, -10,
    -10, -5, 0, 5, 5, 0, -5, -10,
};

int queen_table[64] = {
    -15, -10, -5, 5, 5, -5, -10, -15,
    -5,  10,  8,  9,  9,  8,  10, -5,
    -5,  10,  8,  7,  7,  8,  10, -5,
     0,  15,  13,  14,  14,  13,  15, 0,
     2,  15,  13,  14,  14,  13,  15, 2,
    -10,  5,  2,  7,  7,  2,  5, -10,
    -5,  10,  8,  9,  9,  8,  10, -5,
    -15, -10, -5, 5, 5, -5, -10, -15,
};

int king_table[64] = {
    -17, -16, -20, -29, -29, -20, -16, -17,
    -17, -16, -20, -29, -29, -20, -16, -17,
    -17, -16, -20, -29, -29, -20, -16, -17,
    -15, -13, -17, -26, -26, -17, -13, -15,
    -5, -5, -10, -20, -20, -10, -5, -5,
    -2, -3, -7, -18, -18, -7, -3, -2,
    0, -2, -5, -15, -15, -5, -2, 0,
    5, 6, 5, 0, 0, 5, 6, 5,

};

int king_table_endgame[64] = {
    -30, -15, -15, -15, -15, -15, -15, -30,
     -10,  -5,  0,  0,  0, 0,  -5,  -10,
     0,  15,  25,  35,  35,  25,  15,  0,
    -10, 10,  20,  30,  30,  20, 10, -10,
    -10, 5,  15,  20,  20,  15, 0, -10,
    -15, 0,  10,  15,  15,  10, 0, -15,
    -20, -15, -10, -10, -10, -10, -15, -20,
    -40, -20, -15, -15, -15, -15, -20, -40,
};

int king_attacker_table[100] = {
    0,  0,   4,   6,   9,   13,   18,  22,  29,  36,
  43,  50,  58,  68,  78,  88,  99,  110,  132, 144,
  156,  169,  182,  196,  210, 230, 250, 270, 300, 350,
  450, 500, 500, 500, 500, 500, 500, 500, 500, 500,
  500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
  500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
  500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500
};

int attack_count_table[40] = {
    -5, -2, -1, 0, 1, 2, 2, 3, 5, 8, 11, 14, 17, 20, 22, 22, 22, 22, 22, 22,
    22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,
};

int attack_count_table_bishop[20] = {
    -10, -7, -6, -5, -4, 0, 2, 5, 9, 15, 18, 20, 21, 21, 21, 21, 21, 21, 21, 21
};

int attack_count_table_knight[9] = {-5, -2, -1, 1, 4, 7, 12, 13, 14};
int bishop_obstruction_table[9] = {5, 4, 0, -5, -13, -20, -25, -25, -25};
int bishop_own_obstruction_table[9] = {5, -1, -10, -20, -25, -25, -25, -25, -25};

int pawn_shield_table[100] = {
    0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
};

int pawn_storm_table[100] = {
    3,  3,   5,   6,   7,   8,   9,   11, 12, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13,
};