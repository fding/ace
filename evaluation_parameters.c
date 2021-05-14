#include "evaluation_parameters.h"

// All tables are from white's perspective
// We give a bonus for all pieces the closer they move to the opposite side


int MIDGAME_PAWN_VALUE = 100;
int ENDGAME_PAWN_VALUE = 100;
int MIDGAME_KNIGHT_VALUE = 322;
int MIDGAME_BISHOP_VALUE = 337;
int MIDGAME_ROOK_VALUE = 488;
int MIDGAME_QUEEN_VALUE = 996;
int MIDGAME_BISHOP_PAIR = 46;
int MIDGAME_ROOK_PAIR = -23;

int material_table_mg[6] = {100, 287, 300, 479, 888, 30000};
int knight_material_adj_table[9] = {-91, -23, -9, 2, 8, 15, 21, 30, 46};
int rook_material_adj_table[9] = {14, 14, 19, 18, 13, 8, -3, -20, -37};
int queen_material_adj_table[7] = {-64, -27, 15, 43, 49, 27, -43};

int HANGING_PIECE_PENALTY = -18;
int MIDGAME_BACKWARD_PAWN = -11;
int ENDGAME_BACKWARD_PAWN = -30;
int MIDGAME_SUPPORTED_PAWN = 14;
int ENDGAME_SUPPORTED_PAWN = 15;

int doubled_pawn_penalty[8] = {
    -15, -7, 0, -16, -16, 0, -7, -15
};
int doubled_pawn_penalty_endgame[8] = {-30, -35, -40, -40, -40, -40, -35, -30};

int passed_pawn_table[8] = {
    0, -13, -12, -17, 3, 16, 129, 0
};
int passed_pawn_table_endgame[8] = {
    0, 5, 9, 43, 81, 138, 239, 0
};

int passed_pawn_blockade_table[8] = {0, 0, 36, 36, 30, 28, 35, 58};
int passed_pawn_blockade_table_endgame[8] = {0, 0, 30, 30, 40, 50, 80, 120};

int isolated_pawn_penalty[8] = {-4, 0, -1, -21, -21, -1, 0, -4};
int isolated_pawn_penalty_endgame[8] = {-30, -30, -35, -35, -35, -35, -30, -30};

int KNIGHT_OUTPOST_BONUS = 30;
int KNIGHT_ALMOST_OUTPOST_BONUS = 25;
int BISHOP_OUTPOST_BONUS = 20;
int BISHOP_ALMOST_OUTPOST_BONUS = 6;
int ROOK_OPENFILE = 10;
int ROOK_SEMIOPENFILE = -10;
int ROOK_BLOCKEDFILE = -27;
int QUEEN_XRAYED = -20;
int KING_XRAYED = -56;

int CFDE_PAWN_BLOCK = 8;
int KING_SEMIOPEN_FILE_PENALTY = -23;
int KING_OPEN_FILE_PENALTY = 0;
int CASTLE_OBSTRUCTION_PENALTY = -7;
int CAN_CASTLE_BONUS = 31;
int TEMPO_BONUS = 20;


int pawn_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, -1, -22, -21, -21, -22, -1, -1,
    8, 15, 28, 21, 21, 28, 15, 8,
    -8, 2, 3, 3, 3, 3, 2, -8,
    -11, -10, 1, 6, 6, 1, -10, -11,
    -16, -8, 4, -4, -4, 4, -8, -16,
    -11, -1, -3, -13, -13, -3, -1, -11,
    0, 0, 0, 0, 0, 0, 0, 0
};

int ROOK_OUTPOST_BONUS = 8;
int ROOK_TARASCH_BONUS = 22;
int PINNED_PENALTY = -31;

int pawn_table_endgame[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    18, 21, 0, -23, -23, 0, 21, 18,
    33, 24, -4, -12, -12, -4, 24, 33,
    -27, -43, -46, -54, -54, -46, -43, -27,
    -42, -60, -59, -66, -66, -59, -60, -42,
    -50, -67, -55, -52, -52, -55, -67, -50,
    -50, -68, -47, -43, -43, -47, -68, -50,
    0, 0, 0, 0, 0, 0, 0, 0
};

int knight_table[64] = {
        -83, -48, -32, -21, -21, -32, -48, -83,
        -49, -32, -5, -4, -4, -5, -32, -49,
        -43, 0, 9, 10, 10, 9, 0, -43,
        -11, 9, 18, 15, 15, 18, 9, -11,
        4, 11, 23, 21, 21, 23, 11, 4,
        -18, 0, 8, 18, 18, 8, 0, -18,
        -19, -14, -3, 5, 5, -3, -14, -19,
        -34, -15, -14, 0, 0, -14, -15, -34
};

/*
int bishop_table[64] = {
    -15, -10, -12, -14, -14, -12, -10, -15,
    -13,  5,  -5,  -5,  -5,  -5,  5, -13,
    -5,  10,  8,  5,  5,  8,  10, -5,
     0,  15,  13,  10,  10,  13,  15, 0,
     8,  15,  13,  10,  10,  13,  15, 8,
    -10,  5,  5,  5,  5,  5,  5, -10,
    -5,  12,  8,  5,  5,  8,  12, -5,
    -15, -10, -12, -14, -14, -12, -10, -15,
};
*/

int bishop_table[64] = {
        -24, -28, -37, -25, -25, -37, -28, -24,
        -23, -8, -8, -5, -5, -8, -8, -23,
        -1, -2, 0, 0, 0, 0, -2, -1,
        -8, -12, 4, 10, 10, 4, -12, -8,
        -9, -1, 4, 13, 13, 4, -1, -9,
        -4, 6, 9, 8, 8, 9, 6, -4,
        -21, 6, 0, 4, 4, 0, 6, -21,
        -28, -9, -11, -9, -9, -11, -9, -28
};

int rook_table[64] = {
        -7, -9, -12, 0, 0, -12, -9, -7,
        -10, -6, -1, -12, -12, -1, -6, -10,
        -26, -12, -21, -16, -16, -21, -12, -26,
        -22, -24, -19, -26, -26, -19, -24, -22,
        -28, -17, -28, -24, -24, -28, -17, -28,
        -35, -21, -25, -19, -19, -25, -21, -25,
        -29, -22, -14, -20, -20, -14, -22, -29,
        -16, -14, -7, -9, -9, -27, -14, -16
};

int queen_table[64] = {
        13, 1, 12, 6, 6, 12, 1, 13,
        -1, -32, 4, -3, -3, 4, -32, -1,
        19, 11, 14, 14, 14, 14, 11, 19,
        5, -15, 5, -15, -15, 5, -15, 5,
        11, -4, -3, -10, -10, -3, -4, 11,
        2, -6, -1, -1, -1, -1, -6, 2,
        -20, -12, 7, 6, 6, 7, -12, -20,
        -31, -24, -18, -1, -1, -18, -24, -31
};

/*
int king_table[64] = {
    -17, -16, -20, -29, -29, -20, -16, -17,
    -17, -16, -20, -29, -29, -20, -16, -17,
    -17, -16, -20, -29, -29, -20, -16, -17,
    -15, -13, -17, -26, -26, -17, -13, -15,
    -5, -5, -10, -20, -20, -10, -5, -5,
    -2, -3, -7, -18, -18, -7, -3, -2,
    0, -2, -5, -15, -15, -5, -2, 0,
    7, 10, 4, -2, -2, 4, 10, 7,
};
*/

int king_table[64] = {
        -8, 23, 31, -22, -22, 31, 23, -8,
        -4, -2, 26, 44, 44, 26, -2, -4,
        -1, 64, 85, 62, 62, 85, 64, -1,
        -34, 16, 32, 40, 40, 32, 16, -34,
        -54, 3, 6, 11, 11, 6, 3, -54,
        -31, 13, 12, 8, 8, 12, 13, -31,
        -8, 13, 0, -10, -10, 0, 13, -8,
        0, 25, -6, -25, -25, -6, 25, 0
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
    0, 0, 0, 1, 2, 26, 19, 36, 42, 54, 81, 54, 90,
    100, 113, 115, 125, 130, 130, 130, 130, 130, 130, 130,
    130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
    130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
    130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
    130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
    130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
    130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
    130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
    130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
    130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
};

int attack_count_table_queen[29] = {
        -24, -28, -25, -22, -18, -17, -13, -11,
        -9, -7, 0, 3, 6, 7, 15, 20, 33,
        39, 42, 69, 67, 71, 72, 72, 72, 72, 72, 72, 72
};

int attack_count_table_rook[16] = {
    -35, -27, -24, -17, -16, -14, -5, 1, 7, 8, 18, 28, 31, 40, 40, 40
};

int attack_count_table_bishop[15] = {
    -25, -19, -5, 0, 3, 6, 8, 5, 11, 13, 18, 28, 29, 29, 29
};

int attack_count_table_knight[9] = {
    -39, -21, -7, -5, 1, 10, 13, 24, 30
};

int bishop_obstruction_table[9] = {
    6, 0, -6, -11, -13, -19, -20, -20, -20
};
int bishop_own_obstruction_table[9] = {
    16, 8, -5, -10, -12, -13, -14, -15, -16
};

int pawn_shield_table[100] = {
    0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 5, 5, 5, 5, 5,
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

int space_table[32] = {
    0, 5, 10, 15, 22, 30, 39, 46,
    50, 54, 60, 65, 70, 73, 75, 77,
    80, 80, 80, 80, 80, 80, 80, 80,
    80, 80, 80, 80, 80, 80, 80, 80
};
