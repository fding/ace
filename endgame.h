#ifndef ENDGAME_H
#define ENDGAME_H
#include "pawns.h"

int board_score_endgame(struct board* board, unsigned char who, struct deltaset* mvs);
int board_score_eg_material_pst(struct board* board, unsigned char who, struct deltaset* mvs, struct pawn_structure* pstruct);
int board_score_eg_positional(struct board* board, unsigned char who, struct deltaset* mvs, int endgame_type, struct pawn_structure* pstruct, int winning_side);

#endif
