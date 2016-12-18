#ifndef EVALUATION_PARAMETERS_H
#define EVALUATION_PARAMETERS_H

extern int knight_table[64];
extern int bishop_table[64];
extern int rook_table[64];
extern int queen_table[64];
extern int king_table[64];
extern int king_table_endgame[64];
extern int king_attacker_table[64];
extern int attack_count_table[40];
extern int attack_count_table_bishop[20];
extern int attack_count_table_knight[9];
extern int bishop_obstruction_table[9];
extern int bishop_own_obstruction_table[9];
extern int pawn_shield_table[100];
extern int pawn_storm_table[100];

#endif
