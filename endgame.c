#include "board.h"
#include "endgame.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pieces.h"
#include "moves.h"
#include "pawns.h"
#include "evaluation_parameters.h"

#include "pfkpk/kpk.h"

#define BLACK_SQUARES         0xaa55aa55aa55aa55ull
#define WHITE_SQUARES         0x55aa55aa55aa55aaull

int distance_to_score[9] = {21, 21, 15, 10, 5, 0, -5, -10, -15};

static int dist(int sq1, int sq2) {
    int rank1, file1, rank2, file2;
    rank1 = sq1 / 8;
    file1 = sq1 % 8;
    rank2 = sq2 / 8;
    file2 = sq2 % 8;
    int dh = abs(rank1-rank2);
    int dv = abs(file1-file2);
    if (dh < dv) return dv;
    else return dh;
}

static int material_for_player_endgame(struct board* board, side_t who) {
    return ENDGAME_PAWN_VALUE * popcnt(board->pieces[who][PAWN]) +
        275 * popcnt(board->pieces[who][KNIGHT]) +
        300 * popcnt(board->pieces[who][BISHOP]) +
        550 * popcnt(board->pieces[who][ROOK]) +
        900 * popcnt(board->pieces[who][QUEEN]);
}

// Table of how easy certain endgames are to win
// This is a hash table, computed via:
// nwqueens + nbqueens*3 + nwrooks * 9 + nbrooks * 27 + nwbishops * 81 + nbbishops * 243 + nwknights * 729 + nbknights * 2187
int insufficient_material_table[6561];

static int material_hash_wp(int nwqueens, int nbqueens, int nwrooks, int nbrooks, int nwbishops, int nbbishops,
        int nwknights, int nbknights) {
    return nwqueens +
        nbqueens * 3 + 
        nwrooks * 9 + 
        nbrooks * 27 + 
        nwbishops * 81 + 
        nbbishops * 243 + 
        nwknights * 729 + 
        nbknights * 2187;
}

static int material_hash(struct board* board) {
    return material_hash_wp(popcnt(board->pieces[0][QUEEN]), popcnt(board->pieces[1][QUEEN]),
        popcnt(board->pieces[0][ROOK]), popcnt(board->pieces[1][ROOK]),
        popcnt(board->pieces[0][BISHOP]), popcnt(board->pieces[1][BISHOP]),
        popcnt(board->pieces[0][KNIGHT]), popcnt(board->pieces[1][KNIGHT]));
}

#define ENDGAME_TABLE_JUNK -1
#define PCHECKMATE 4096
void initialize_endgame_tables() {
    kpkGenerate();
    // -1 is our junk value
    for (int i = 0; i < 6561; i++) {
        insufficient_material_table[i] = ENDGAME_TABLE_JUNK;
    }
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 0, 0, /* Knight */ 0, 0)] = 0;
    /* Absolute draws */
    // KB vs K
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 1, 0, /* Knight */ 0, 0)] = 0;
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 0, 1, /* Knight */ 0, 0)] = 0;
    // KB vs KB
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 1, 1, /* Knight */ 0, 0)] = 0;
    // KN vs K
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 0, 0, /* Knight */ 1, 0)] = 0;
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 0, 0, /* Knight */ 0, 1)] = 0;
    // KN vs KN
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 0, 0, /* Knight */ 1, 1)] = 0;
    // KNN vs KN
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 0, 0, /* Knight */ 2, 1)] = 0;
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 0, 0, /* Knight */ 1, 2)] = 0;
    // KNN vs KNN
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 0, 0, /* Knight */ 2, 2)] = 0;

    /* Basic checkmates */
    // KQX vs K. The easiest. We should be careful not to assign to large checkmate values without search proving that
    // checkmate is possible (since after all the queen might be taken)
    for (int i = 0; i <= 2; i++) {
        for (int j = 0; j <= 2; j++) {
            for (int k = 0; k <= 2; k++) {
                insufficient_material_table[material_hash_wp(/* Queen */ 1, 0, /* Rook */ i, 0, /* Bishop */ j, 0, /* Knight */ k, 0)] = 
                    PCHECKMATE;
                insufficient_material_table[material_hash_wp(/* Queen */ 0, 1, /* Rook */ 0, i, /* Bishop */ 0, j, /* Knight */ 0, k)] =
                    -PCHECKMATE;
            }
        }
    }
    // KR vs K. Should be easy
    for (int i = 1; i < 2; i++) {
        for (int j = 0; j <= 2; j++) {
            for (int k = 0; k <= 2; k++) {
                insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ i, 0, /* Bishop */ j, 0, /* Knight */ k, 0)] =
                    PCHECKMATE/2 * i;
                insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, i, /* Bishop */ 0, j, /* Knight */ 0, k)] =
                    -PCHECKMATE/2 * i;
            }
        }
    }
    // KBB vs K.
    for (int i = 0; i <= 2; i++) {
        insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 2, 0, /* Knight */ i, 0)] = 3*PCHECKMATE/8;
        insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 0, 2, /* Knight */ 0, i)] = -3*PCHECKMATE/8;
    }
    // KBN vs K. This could be tricky though
    for (int i = 1; i <= 2; i++) {
        insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 1, 0, /* Knight */ i, 0)] = PCHECKMATE/4;
        insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 0, /* Bishop */ 0, 1, /* Knight */ 0, i)] = -PCHECKMATE/4;
    }

    // KQ vs KR. Usually a win
    for (int i = 0; i <= 2; i++) {
        for (int j = 0; j <= 2; j++) {
            for (int k = 0; k <= 2; k++) {
                insufficient_material_table[material_hash_wp(/* Queen */ 1, 0, /* Rook */ i, 1, /* Bishop */ j, 0, /* Knight */ k, 0)] =
                    PCHECKMATE/4;
                insufficient_material_table[material_hash_wp(/* Queen */ 0, 1, /* Rook */ 1, i, /* Bishop */ 0, j, /* Knight */ 0, k)] =
                    -PCHECKMATE/4;
            }
        }
    }
    // KQ vs K+minor pieces. Usually a win, but could be difficult
    for (int i = 0; i <= 2; i++) {
        for (int j = 0; j <= 2; j++) {
            for (int k = 0; k <= 2; k++) {
                insufficient_material_table[material_hash_wp(/* Queen */ 1, 0, /* Rook */ i, 0, /* Bishop */ j, 1, /* Knight */ k, 0)] =
                    PCHECKMATE/8;
                insufficient_material_table[material_hash_wp(/* Queen */ 0, 1, /* Rook */ 0, i, /* Bishop */ 1, j, /* Knight */ 0, k)] =
                    -PCHECKMATE/8;
                insufficient_material_table[material_hash_wp(/* Queen */ 1, 0, /* Rook */ i, 0, /* Bishop */ j, 2, /* Knight */ k, 0)] =
                    PCHECKMATE/8;
                insufficient_material_table[material_hash_wp(/* Queen */ 0, 1, /* Rook */ 0, i, /* Bishop */ 2, j, /* Knight */ 0, k)] =
                    -PCHECKMATE/8;
                insufficient_material_table[material_hash_wp(/* Queen */ 1, 0, /* Rook */ i, 0, /* Bishop */ j, 1, /* Knight */ k, 1)] =
                    PCHECKMATE/8;
                insufficient_material_table[material_hash_wp(/* Queen */ 0, 1, /* Rook */ 0, i, /* Bishop */ 1, j, /* Knight */ 1, k)] =
                    -PCHECKMATE/8;
                insufficient_material_table[material_hash_wp(/* Queen */ 1, 0, /* Rook */ i, 0, /* Bishop */ j, 0, /* Knight */ k, 2)] =
                    PCHECKMATE/8;
                insufficient_material_table[material_hash_wp(/* Queen */ 0, 1, /* Rook */ 0, i, /* Bishop */ 0, j, /* Knight */ 2, k)] =
                    -PCHECKMATE/8;
            }
        }
    }
    
    // KR vs K+minor. Drawish
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 1, 0, /* Bishop */ 0, 1, /* Knight */ 0, 0)] = 2;
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 1, /* Bishop */ 1, 0, /* Knight */ 0, 0)] = -2;
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 1, 0, /* Bishop */ 0, 0, /* Knight */ 0, 1)] = 2;
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 0, 1, /* Bishop */ 0, 0, /* Knight */ 1, 0)] = -2;
    // KR+minor vs KR. Drawish
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 1, 1, /* Bishop */ 0, 1, /* Knight */ 0, 0)] = -2;
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 1, 1, /* Bishop */ 1, 0, /* Knight */ 0, 0)] = 2;
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 1, 1, /* Bishop */ 0, 0, /* Knight */ 0, 1)] = -2;
    insufficient_material_table[material_hash_wp(/* Queen */ 0, 0, /* Rook */ 1, 1, /* Bishop */ 0, 0, /* Knight */ 1, 0)] = 2;
}

// Endgame behaves very differently, so we have a separate scoring function
int board_score_endgame(struct board* board, unsigned char who, struct deltaset* mvs) {
    int score = 0;

#define EG_NONE 0
#define EG_KPKP 1
#define EG_KPKB 2
    int endgame_type = EG_NONE;

    uint64_t pawns[2], minors[2], majors[2], kings[2], pieces[2];
    pawns[0] = P2BM(board, WHITEPAWN);
    pawns[1] = P2BM(board, BLACKPAWN);
    minors[0] = P2BM(board, WHITEKNIGHT) | P2BM(board, WHITEBISHOP);
    minors[1] = P2BM(board, BLACKKNIGHT) | P2BM(board, BLACKBISHOP);
    majors[0] = P2BM(board, WHITEROOK) | P2BM(board, WHITEQUEEN);
    majors[1] = P2BM(board, BLACKROOK) | P2BM(board, BLACKQUEEN);
    kings[0] = P2BM(board, WHITEKING);
    kings[1] = P2BM(board, BLACKKING);

    int wkingsquare, bkingsquare;
    wkingsquare = board->kingsq[0];
    bkingsquare = board->kingsq[1];

    pieces[0] = pawns[0] | minors[0] | majors[0];
    pieces[1] = pawns[1] | minors[1] | majors[1];

    // Draws from insufficient material
    // Only kings
    if (!(pieces[0] | pieces[1])) {
        return 0;
    }

    // No pawns
    int endgame_table_value = ENDGAME_TABLE_JUNK;
    if (!(pawns[0] | pawns[1])) {
        int hval = material_hash(board);
        if (hval < 6561) {
            int endgame_table_value = insufficient_material_table[hval];
            DPRINTF("hash=%d, val=%d\n", hval, endgame_table_value);
            if (endgame_table_value != ENDGAME_TABLE_JUNK) {
                if (endgame_table_value == 0)
                    return 0;
                score += endgame_table_value;
            }
            DPRINTF("score=%d\n", score);
        }
    }

    // Pawn + king vs king
    if (!(majors[0] | majors[1] | minors[0] | minors[1])) {
        endgame_type = EG_KPKP;
        if (popcnt(pawns[0]) > 1 && (!pawns[1])) {
            score += 500;
        }
        if (popcnt(pawns[1]) > 1 && (!pawns[0])) {
            score -= 500;
        }
        if (popcnt(pawns[0]) == 1 && (!pawns[1])) {
            int val = kpkProbe(who, wkingsquare, LSBINDEX(pawns[0]), bkingsquare);
            if (val == 0) {
                DPRINTF("Bitbase draw\n");
                return 0;
            }
        }
        if (popcnt(pawns[1]) == 1 && (!pawns[0])) {
            int val = kpkProbe(who, wkingsquare, LSBINDEX(pawns[1]), bkingsquare);
            if (val == 0) {
                DPRINTF("Bitbase draw\n");
                return 0;
            }
        }
    }
    if (!pieces[0] || !pieces[1]) {
        int defending_king_square;
        uint64_t attacks;
        int sign = 0;
        uint64_t accessible_squares;
        if (pieces[1]) {
            defending_king_square = wkingsquare;
            attacks = attacked_squares(board, 1, pieces[1] | kings[1]);
            accessible_squares = kings[0];
            sign = -1;
        } else {
            defending_king_square = bkingsquare;
            attacks = attacked_squares(board, 0, pieces[0] | kings[0]);
            sign = 1;
            accessible_squares = kings[1];
        }
        uint64_t examined_squares = 0;
        while ((examined_squares ^ accessible_squares)) {
            int square = LSBINDEX(examined_squares ^ accessible_squares);
            accessible_squares |= attack_set_king(square, pieces[0] | pieces[1], 0) & (~attacks);
            examined_squares |= (1ull << square);
        }
        DPRINTF("Accessible squares: %d\n", popcnt(accessible_squares));
        score -= sign * popcnt(accessible_squares) * 5;
    }

    struct pawn_structure* pstruct = evaluate_pawns(board);
    int material_score = board_score_eg_material_pst(board, who, mvs, pstruct);
    DPRINTF("Material pieceboard score: %d \n", material_score);
    // For drawish positions, don't over emphasize position
    if (endgame_table_value != ENDGAME_TABLE_JUNK && endgame_table_value > -10
            && endgame_table_value < 10) {
        material_score /= 4;
    }
    DPRINTF("Score prematerial: %d\n", score);
    score += material_score;
    DPRINTF("Score aftermaterial: %d\n", score);
    int winning_side = 0;
    if (material_score >= 100) {
        winning_side = 1;
    } else if (material_score <= -100) {
        winning_side = -1;
    }
    score += board_score_eg_positional(board, who, mvs, endgame_type, pstruct, winning_side);

    DPRINTF("Score afterpositional: %d\n", score);
    return score;
}

// Endgame behaves very differently, so we have a separate scoring function
int board_score_eg_material_pst(struct board* board, unsigned char who, struct deltaset* mvs, struct pawn_structure* pstruct) {
    uint64_t temp;
    int score = 0;
    int square, count;

    int wkingsquare, bkingsquare;
    wkingsquare = board->kingsq[0];
    bkingsquare = board->kingsq[1];

    // Draws from insufficient material

    int whitematerial = material_for_player_endgame(board, 0);
    int blackmaterial = material_for_player_endgame(board, 1);
    float factor = 1;
    if (blackmaterial + whitematerial < 4000) {
        factor = 2 - (blackmaterial + whitematerial) / 4000.0;
    }
    score += (int) ((whitematerial - blackmaterial) * factor);
    DPRINTF("Material score: %d\n", whitematerial - blackmaterial);
    DPRINTF("Material factor: %.2f\n", factor);
    score += pstruct->score_eg;
    DPRINTF("Pawn score: %d\n", pstruct->score_eg);
    DPRINTF("total score: %d\n", score);

    bmloop(P2BM(board, WHITEKNIGHT), square, temp) {
        score += knight_table[63 - square];
    }
    bmloop(P2BM(board, BLACKKNIGHT), square, temp) {
        score -= knight_table[square];
    }

    DPRINTF("total score before bishop: %d\n", score);
    count = 0;
    bmloop(P2BM(board, WHITEBISHOP), square, temp) {
        score += bishop_table[63 - square];
        count += 1;
    }
    // Bishop pairs are very valuable
    // In the endgame, 2 bishops can checkmate a king,
    // whereas 2 knights can't
    score += (count == 2) * 60;
    DPRINTF("total score after bishop: %d\n", score);

    count = 0;
    bmloop(P2BM(board, BLACKBISHOP), square, temp) {
        score -= bishop_table[square];
        count += 1;
    }
    score -= (count == 2) * 60;
    DPRINTF("total score after all bishop: %d\n", score);
    bmloop(P2BM(board, WHITEROOK), square, temp) {
        score += rook_table[63 - square];
    }
    bmloop(P2BM(board, BLACKROOK), square, temp) {
        score -= rook_table[square];
    }

    score += king_table_endgame[63 - wkingsquare];
    score -= king_table_endgame[bkingsquare];

    return score;
}

int board_score_eg_positional(struct board* board, unsigned char who, struct deltaset* mvs, int endgame_type, struct pawn_structure* pstruct, int winning_side) {
    int file = 0, rank = 0;

    int score = 0;
    uint64_t temp;
    int square;

    uint64_t pawns[2], minors[2], majors[2], kings[2];
    pawns[0] = P2BM(board, WHITEPAWN);
    pawns[1] = P2BM(board, BLACKPAWN);
    minors[0] = P2BM(board, WHITEKNIGHT) | P2BM(board, WHITEBISHOP);
    minors[1] = P2BM(board, BLACKKNIGHT) | P2BM(board, BLACKBISHOP);
    majors[0] = P2BM(board, WHITEROOK) | P2BM(board, WHITEQUEEN);
    majors[1] = P2BM(board, BLACKROOK) | P2BM(board, BLACKQUEEN);
    kings[0] = P2BM(board, WHITEKING);
    kings[1] = P2BM(board, BLACKKING);

    int wkingsquare, bkingsquare;
    wkingsquare = board->kingsq[0];
    bkingsquare = board->kingsq[1];
    uint64_t passed_pawn_blockade[2];
    passed_pawn_blockade[0] = pstruct->passed_pawns[0] << 8;
    passed_pawn_blockade[1] = pstruct->passed_pawns[1] >> 8;

    for (int w = 0; w < 2; w++) {
        int file_occupied[8];
        int subscore = 0;
        memset(file_occupied, 0, 8 * sizeof(int));
        bmloop(passed_pawn_blockade[1-w] & (minors[w] | majors[w] | kings[w]), square, temp) {
            int rank;
            if (w)
                rank = square / 8;
            else
                rank = 7 - square / 8;
            subscore += passed_pawn_blockade_table_endgame[rank];
        }
        bmloop(P2BM(board, 6 * w + PAWN), square, temp) {
            // Encourage kings to come and protect these pawns
            int dist_from_wking = dist(square, wkingsquare);
            int dist_from_bking = dist(square, bkingsquare);
            int dist_score = 0;
            dist_score += (dist_from_bking - dist_from_wking) * 5;
            // Prioritize passed pawns
            if (pstruct->passed_pawns[w] & (1ull << square)) {
                dist_score += (dist_from_bking - dist_from_wking) * 10;
            }
            if (endgame_type == EG_KPKP)
                dist_score *= 4;

            DPRINTF("WPAWN Distance score: %d\n", dist_score);
            // Directly add to score since this isn't a sided quantity
            score += dist_score;
        }
        bmloop(P2BM(board, 6 * w + BISHOP), square, temp) {
            uint64_t color_mask = 0;
            if ((1ull << square) & BLACK_SQUARES)
                color_mask = BLACK_SQUARES;
            else
                color_mask = WHITE_SQUARES;
            int bishop_score = popcnt(pawns[1-w] & color_mask) * 6 - popcnt(pawns[w] & color_mask) * 2;
            subscore += bishop_score;
            SQPRINTF("Bishop pawn score for %c%c: %d\n", square, bishop_score);
            uint64_t last_row = RANK1;
            if (w) last_row = RANK8;
            int wrong_bishop_score = 0;
            wrong_bishop_score -= 5 * popcnt((pstruct->passed_pawn_advance_span[1 - w] & last_row) & ~color_mask);
            wrong_bishop_score -= 20 * popcnt((pstruct->passed_pawn_advance_span[1 - w] & last_row) & ~color_mask & (AFILE | HFILE));
            SQPRINTF("Wrong bishop score for %c%c: %d\n", square, wrong_bishop_score);
            subscore += wrong_bishop_score;
        }
        bmloop(P2BM(board, 6 * w + ROOK), square, temp) {
            file = square & 0x7;
            rank = square / 8;

            // Doubled rooks are very powerful.
            if ((AFILE << file) & (majors[w] ^ (1ull << square)))
                subscore += 5;
            // Rook should be behind passed pawns
            for (int pawn_side = 0; pawn_side < 2; pawn_side++) {
                uint64_t same_file_pawn = (AFILE << file) & pstruct->passed_pawns[pawn_side] & ~pstruct->rear_span[pawn_side];
                if (same_file_pawn && !file_occupied[file]) {
                  int lineup_score = 0;
                  if (pawn_side) {
                      if (same_file_pawn < (1ull << square)) {
                          lineup_score = 25;
                      }
                  } else {
                      if (same_file_pawn > (1ull << square)) {
                          lineup_score = 25;
                      }
                  }
                  SQPRINTF("Rook behind pawn score for %c%c: %d\n", square, lineup_score);
                  subscore += lineup_score;
                }
            }
            file_occupied[file] = 1;
        }
        bmloop(P2BM(board, 6 * w + QUEEN), square, temp) {
            file = square & 0x7;

            if ((AFILE << file) & (majors[w] ^ (1ull << square)))
                subscore += 5;
        }
        if (w) {
            score -= subscore;
        } else {
            score += subscore;
        }
    }
    DPRINTF("score: %d\n", score);

    // Encourage the winning side to move kings closer together
    int king_distance_score = winning_side * distance_to_score[dist(wkingsquare, bkingsquare)];
    if (!(pawns[0] | pawns[1]))
        king_distance_score *= 5;

    score += king_distance_score;
    DPRINTF("Winning side king distance: %d, %d\n", king_distance_score, dist(wkingsquare, bkingsquare));
    DPRINTF("score: %d\n", score);
    return score;
}
