#include "board.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pieces.h"
#include "moves.h"
#include "pawns.h"
#include "evaluation_parameters.h"

#include "pfkpk/kpk.h"

int distance_to_score[9] = {21, 21, 18, 15, 12, 9, 6, 3, 0};

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
    return 135 * popcnt(board->pieces[who][PAWN]) +
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
    int file = 0, rank = 0;

    int score = 0;
    uint64_t temp;
    int square, count;

#define EG_NONE 0
#define EG_KPKP 1
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

    pieces[0] = pawns[0] | minors[0] | majors[0] | kings[0];
    pieces[1] = pawns[1] | minors[1] | majors[1] | kings[1];

    // Draws from insufficient material

#define ENDGAME_KNOWLEDGE
#ifdef ENDGAME_KNOWLEDGE
    // Only kings
    if (popcnt(pieces[0]) == 1 && popcnt(pieces[1]) == 1) {
        return 0;
    }

    // No pawns
    if (popcnt(pawns[0]) == 0 && popcnt(pawns[1]) == 0) {
        int hval = material_hash(board);
        if (hval < 6561) {
            int val = insufficient_material_table[hval];
            DPRINTF("hash=%d, val=%d\n", hval, val);
            if (val != ENDGAME_TABLE_JUNK) {
                if (val == 0)
                    return 0;
                score += val;
            }
            DPRINTF("score=%d\n", score);
        }
    }

    // Pawn + king vs king
    if (popcnt(majors[0]) == 0 && popcnt(majors[1]) == 0 &&
            popcnt(minors[0]) == 0 && popcnt(minors[1]) == 0) {
        endgame_type = EG_KPKP;
        if (popcnt(pawns[0]) > 1 && popcnt(pawns[1]) == 0) {
            score += 400;
        }
        if (popcnt(pawns[1]) > 1 && popcnt(pawns[0]) == 0) {
            score -= 400;
        }
        if (popcnt(pawns[0]) == 1 && popcnt(pawns[1]) == 0) {
            int val = kpkProbe(who, wkingsquare, LSBINDEX(pawns[0]), bkingsquare);
            if (val == 0) {
                DPRINTF("Bitbase draw\n");
                return 0;
            }
            int psquare = LSBINDEX(pawns[0]);
            int prank = psquare / 8;
            int pfile = psquare % 8;
            int qsquare = 56 + pfile;
            if (dist(qsquare, bkingsquare) + (board->who == 1) < (7 - prank))
                score += 400;
        }
        if (popcnt(pawns[1]) == 1 && popcnt(pawns[0]) == 0) {
            int val = kpkProbe(who, wkingsquare, LSBINDEX(pawns[1]), bkingsquare);
            if (val == 0) {
                DPRINTF("Bitbase draw\n");
                return 0;
            }
            int psquare = LSBINDEX(pawns[0]);
            int prank = psquare / 8;
            int pfile = psquare % 8;
            int qsquare = pfile;
            if (dist(qsquare, wkingsquare) + (board->who == 0) < prank)
                score -= 400;
        }
    }

    /*
    if (popcnt(pieces[0]) == 1) {
        if (majors[1]) {
            int rank = wkingsquare / 8;
            int file = wkingsquare % 8;
            int dr, df;
            dr = rank;
            df = file;
            if (rank >= 4) dr = 7-dr;
            if (file >= 4) df = 7-df;

            mask = AFILE << file;
            if (df == file)
                mask |= AFILE << (file+1);
            else
                mask |= AFILE << (file-1);
            mask |= RANK1 << (8*rank);
            if (dr == rank)
                mask |= RANK1 << (8*rank+8);
            else
                mask |= RANK1 << (8*rank-8);
            if (majors[1] & mask) score -= 100;

        }
    } else if (popcnt(pieces[1]) == 1) {
        if (majors[0]) {
            int rank = bkingsquare / 8;
            int file = bkingsquare % 8;
            int dr, df;
            dr = rank;
            df = file;
            if (rank >= 4) dr = 7-dr;
            if (file >= 4) df = 7-df;
                mask = AFILE << file;
                if (df == file)
                    mask |= AFILE << (file+1);
                else
                    mask |= AFILE << (file-1);
                mask |= RANK1 << (8*rank);
                if (dr == rank)
                    mask |= RANK1 << (8*rank+8);
                else
                    mask |= RANK1 << (8*rank-8);
            if (majors[0] & mask) score += 100;
        }
    }
    */

#endif

    int whitematerial = material_for_player_endgame(board, 0);
    int blackmaterial = material_for_player_endgame(board, 1);
    struct pawn_structure* pstruct;
    pstruct = evaluate_pawns(board);
    float factor = 1;
    if (blackmaterial + whitematerial < 400)
        factor = 3;
    if (blackmaterial + whitematerial < 1000)
        factor = 2;
    if (blackmaterial + whitematerial < 1600)
        factor = 1.5;
    score += (int) ((whitematerial - blackmaterial) * factor);
    DPRINTF("Material score: %d\n", whitematerial - blackmaterial);
    score += pstruct->score_eg;
    DPRINTF("Pawn score: %d\n", pstruct->score_eg);

    bmloop(P2BM(board, WHITEPAWN), square, temp) {
        // Encourage kings to come and protect these pawns
        int dist_from_wking = dist(square, wkingsquare);
        int dist_from_bking = dist(square, bkingsquare);
        int dist_score = 0;
        dist_score += (dist_from_bking - dist_from_wking) * 5;
        // passed pawns are good
        if (pstruct->passed_pawns[0] & (1ull << square)) {
            dist_score += (dist_from_bking - dist_from_wking) * 5;
        }
        if (endgame_type == EG_KPKP)
            dist_score *= 4;

        score += dist_score;
    }

    bmloop(P2BM(board, BLACKPAWN), square, temp) {
        int dist_from_wking = dist(square, wkingsquare);
        int dist_from_bking = dist(square, bkingsquare);
        int dist_score = 0;
        dist_score += (dist_from_bking - dist_from_wking) * 5;

        if (pstruct->passed_pawns[0] & (1ull << square)) {
            dist_score += (dist_from_bking - dist_from_wking) * 5;
        }

        if (endgame_type == EG_KPKP) {
            dist_score *= 4;
        }

        score += dist_score;
    }

    bmloop(P2BM(board, WHITEKNIGHT), square, temp) {
        score += knight_table[63 - square];
    }
    bmloop(P2BM(board, BLACKKNIGHT), square, temp) {
        score -= knight_table[square];
    }

    count = 0;
    bmloop(P2BM(board, WHITEBISHOP), square, temp) {
        score += bishop_table[63 - square];
        count += 1;
    }
    // Bishop pairs are very valuable
    // In the endgame, 2 bishops can checkmate a king,
    // whereas 2 knights can't
    score += (count == 2) * 60;

    count = 0;
    bmloop(P2BM(board, BLACKBISHOP), square, temp) {
        score -= bishop_table[square];
        count += 1;
    }
    score -= (count == 2) * 60;

    bmloop(P2BM(board, WHITEROOK), square, temp) {
        file = square & 0x7;
        rank = square / 8;

        // Doubled rooks are very powerful.
        if ((AFILE << file) & (majors[0] ^ (1ull << square)))
            score += 5;
        // Rook should be behind own pawns
        if ((AFILE << file) & (pawns[0])) {
          int pawn_sq = (AFILE << file) & pawns[0];
          if (pawn_sq / 8 > rank) {
            score += 20;
          } else {
            score -= 30;
          }
        }
    }

    bmloop(P2BM(board, BLACKROOK), square, temp) {
        file = square & 0x7;
        rank = square / 8;

        if ((AFILE << file) & (majors[1] ^ (1ull << square)))
            score -= 5;
        // Rook should be behind own pawns
        if ((AFILE << file) & (pawns[1])) {
          int pawn_sq = (AFILE << file) & pawns[1];
          if (pawn_sq / 8 < rank) {
            score -= 20;
          } else {
            score += 30;
          }
        }
    }
    bmloop(P2BM(board, WHITEQUEEN), square, temp) {
        file = square & 0x7;

        if ((AFILE << file) & (majors[0] ^ (1ull << square)))
            score += 5;
    }
    bmloop(P2BM(board, BLACKQUEEN), square, temp) {

        if ((AFILE << file) & (majors[1] ^ (1ull << square)))
            score -= 5;
    }

    int winning_side = 0;
    if (whitematerial > blackmaterial)
        winning_side = 1;
    else if (whitematerial < blackmaterial)
        winning_side = -1;

    // Encourage the winning side to move kings closer together
    score += winning_side * distance_to_score[dist(wkingsquare, bkingsquare)];

    // King safety

    score += king_table_endgame[63 - wkingsquare];
    score -= king_table_endgame[bkingsquare];

    return score;
}
