#ifndef EVALUATION_PARAMETERS_H
#define EVALUATION_PARAMETERS_H

extern int MIDGAME_PAWN_VALUE;
extern int ENDGAME_PAWN_VALUE;
extern int MIDGAME_KNIGHT_VALUE;
extern int MIDGAME_BISHOP_VALUE;
extern int MIDGAME_ROOK_VALUE;
extern int MIDGAME_QUEEN_VALUE;
extern int MIDGAME_BISHOP_PAIR;
extern int MIDGAME_ROOK_PAIR;

extern int material_table_mg[6];

extern int MIDGAME_BACKWARD_PAWN;
extern int ENDGAME_BACKWARD_PAWN;
extern int MIDGAME_SUPPORTED_PAWN;
extern int ENDGAME_SUPPORTED_PAWN;

extern int CFDE_PAWN_BLOCK;

extern int KNIGHT_OUTPOST_BONUS;
extern int KNIGHT_ALMOST_OUTPOST_BONUS;
extern int BISHOP_OUTPOST_BONUS;
extern int BISHOP_ALMOST_OUTPOST_BONUS;
extern int ROOK_OUTPOST_BONUS;
extern int ROOK_TARASCH_BONUS;
extern int ROOK_OPENFILE;
extern int ROOK_SEMIOPENFILE;
extern int ROOK_BLOCKEDFILE;
extern int QUEEN_XRAYED;
extern int KING_XRAYED;
extern int PINNED_PENALTY;

extern int KING_SEMIOPEN_FILE_PENALTY;
extern int KING_OPEN_FILE_PENALTY;
extern int CASTLE_OBSTRUCTION_PENALTY;

extern int HANGING_PIECE_PENALTY;
extern int TEMPO_BONUS;
extern int CAN_CASTLE_BONUS;

extern int doubled_pawn_penalty[8];
extern int doubled_pawn_penalty_endgame[8];
extern int passed_pawn_table[8];
extern int passed_pawn_table_endgame[8];
extern int passed_pawn_blockade_table[8];
extern int passed_pawn_blockade_table_endgame[8];
extern int isolated_pawn_penalty[8];
extern int isolated_pawn_penalty_endgame[8];

extern int knight_material_adj_table[9];
extern int rook_material_adj_table[9];
extern int queen_material_adj_table[7];

extern int pawn_table[64];
extern int pawn_table_endgame[64];
extern int knight_table[64];
extern int bishop_table[64];
extern int rook_table[64];
extern int queen_table[64];
extern int king_table[64];
extern int king_table_endgame[64];
extern int king_attacker_table[100];
extern int attack_count_table_queen[29];
extern int attack_count_table_rook[16];
extern int attack_count_table_bishop[15];
extern int attack_count_table_knight[9];
extern int bishop_obstruction_table[9];
extern int bishop_own_obstruction_table[9];
extern int pawn_shield_table[100];
extern int pawn_storm_table[100];

extern int space_table[32];
#endif
