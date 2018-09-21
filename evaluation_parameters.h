#ifndef EVALUATION_PARAMETERS_H
#define EVALUATION_PARAMETERS_H

extern int MIDGAME_PAWN_VALUE;
extern int MIDGAME_KNIGHT_VALUE;
extern int MIDGAME_BISHOP_VALUE;
extern int MIDGAME_ROOK_VALUE;
extern int MIDGAME_QUEEN_VALUE;
extern int MIDGAME_BISHOP_PAIR;

extern int MIDGAME_BACKWARD_PAWN;
extern int MIDGAME_SUPPORTED_PAWN;

extern int KNIGHT_OUTPOST_BONUS;
extern int KNIGHT_ALMOST_OUTPOST_BONUS;


extern int doubled_pawn_penalty[8];
extern int passed_pawn_table[8];
extern int isolated_pawn_penalty[8];


extern int knight_table[64];
extern int bishop_table[64];
extern int rook_table[64];
extern int queen_table[64];
extern int king_table[64];
extern int king_table_endgame[64];
extern int king_attacker_table[100];
extern int attack_count_table[40];
extern int attack_count_table_bishop[20];
extern int attack_count_table_knight[9];
extern int bishop_obstruction_table[9];
extern int bishop_own_obstruction_table[9];
extern int pawn_shield_table[100];
extern int pawn_storm_table[100];

#endif
