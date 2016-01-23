#include "board.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pieces.h"
#include "moves.h"
#include "pawns.h"

#define AFILE 0x0101010101010101ull
#define HFILE 0x8080808080808080ull
#define RANK1 0x00000000000000ffull
#define RANK2 0x000000000000ff00ull
#define RANK3 0x0000000000ff0000ull
#define RANK6 0x0000ff0000000000ull
#define RANK7 0x00ff000000000000ull
#define RANK8 0xff00000000000000ull


static int board_score_endgame(struct board* board, unsigned char who, struct deltaset* mvs);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Evaluation CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Evaluates relative score of white and black. Pawn = 1. who = whose turn. Positive is good for white.
// is_in_check = if current side is in check, nmoves = number of moves

// All tables are from white's perspective
// We give a bonus for all pieces the closer they move to the opposite side

int knight_table[64] = {
    -30, -15, -15, -15, -15, -15, -15, -30,
     -10,  -5,  0,  0,  0, 0,  -5,  -10,
     0,  15,  25,  35,  35,  25,  15,  0,
    -10, 10,  20,  30,  30,  20, 10, -10,
    -10, 5,  15,  20,  20,  15, 0, -10,
    -15, 0,  10,  15,  15,  10, 0, -15,
    -20, -15, -10, -10, -10, -10, -15, -20,
    -40, -20, -15, -15, -15, -15, -20, -40,

};

int bishop_table[64] = {
    -15, -10, -12, -14, -14, -12, -10, -15,
    -5,  10,  8,  5,  5,  8,  10, -5,
     2,  15,  13,  10,  10,  13,  15, 2,
     0,  15,  13,  10,  10,  13,  15, 0,
    -5,  10,  8,  5,  5,  8,  10, -5,
    -10,  5,  0,  -2,  -2,  0,  5, -10,
    -13,  5,  -5,  -5,  -5,  -5,  5, -13,
    -20, -10, -15, -20, -20, -15, -10, -20,
};

int rook_table[64] = {
    -10, -5, 0, 5, 5, 0, -5, -10,
    -10, -5, 0, 5, 5, 0, -5, -10,
    -10, -5, 0, 5, 5, 0, -5, -10,
    -10, -5, 0, 5, 5, 0, -5, -10,
    -10, -5, 0, 5, 5, 0, -5, -10,
    -10, -5, 0, 5, 5, 0, -5, -10,
    -10, -5, 0, 5, 5, 0, -5, -10,
    -10, -5, 0, 5, 5, 0, -5, -10,
};

int queen_table[64] = {
    0, 2, 5, 10, 10, 5, 2, 0,
    0, 2, 5, 10, 10, 5, 2, 0,
    0, 2, 5, 10, 10, 5, 2, 0,
    0, 0, 5, 10, 10, 5, 0, 0,
    0, 0, 5, 10, 10, 5, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -2, 0, 0, 0, 0, 0, 0, -2,
};

int king_table[64] = {
    -100, -100, -100, -100, -100, -100, -100, -100,
    -100, -100, -100, -100, -100, -100, -100, -100,
    -100, -100, -100, -100, -100, -100, -100, -100,
    -100, -100, -100, -100, -100, -100, -100, -100,
    -50, -50, -50, -60, -60, -50, -50, -50,
    -20, -20, -40, -50, -50, -40, -20, -20,
    -10, -10, -10, -20, -20, -10, -10, -10,
    10, 10, 10, -10, -10, 10, 10, 10,
};

int king_table_endgame[64] = {
    -80, -50, -30, -30, -30, -30, -50, -80,
    -50, -20, -20, -20, -20, -20, -20, -50,
    -30, -20,  30,  30,  30,  30, -20, -30,
    -30, -20,  30,  40,  40,  30, -20, -30,
    -30, -20,  30,  40,  40,  30, -20, -30,
    -30, -20,  30,  30,  30,  30, -20, -30,
    -50, -20, -20, -20, -20, -20, -20, -50,
    -80, -50, -30, -30, -30, -30, -50, -80,
};

#define BLACK_CENTRAL_SQUARES 0x0000281428140000ull
#define WHITE_CENTRAL_SQUARES 0x0000142814280000ull

int material_for_player(struct board* board, unsigned char who) {
    return 100 * bitmap_count_ones(board->pieces[who][PAWN]) +
        300 * bitmap_count_ones(board->pieces[who][KNIGHT]) +
        315 * bitmap_count_ones(board->pieces[who][BISHOP]) +
        490 * bitmap_count_ones(board->pieces[who][ROOK]) +
        880 * bitmap_count_ones(board->pieces[who][QUEEN]);
}

int material_for_player_endgame(struct board* board, unsigned char who) {
    return 135 * bitmap_count_ones(board->pieces[who][PAWN]) +
        275 * bitmap_count_ones(board->pieces[who][KNIGHT]) +
        300 * bitmap_count_ones(board->pieces[who][BISHOP]) +
        550 * bitmap_count_ones(board->pieces[who][ROOK]) +
        900 * bitmap_count_ones(board->pieces[who][QUEEN]);
}

/* Scoring the board:
 * We score the board in units of centipawns, taking the following
 * into consideration:
 *  1. Material
 *  2. Piece location (differs for king between endgame and midgame)
 *  3. Presence of bishop pair (half a pawn)
 *  4. Pawn structure
 *      a. passed pawn (bonus)
 *      b. isolated pawn (penalty)
 *      c. doubled pawns (penalty)
 *  5. Doubled rooks (bonus)
 *  6. Rooks on open files (bonus, 1/5 of a pawn)
 *  7. Rooks on semiopen files (bonus, 1/10 of a pawn)
 *  8. Central pawns on same color for bishop (penalty)
 *  9. Undefended attacked pieces (heavy penalty)
 *  10. Number of available attacks, disregarding king pins (bonus)
 *  11. Castling rights (penalty if you can't castle)
 *  12. King safety
 *      a. Open file (penalty)
 *      b. Lack of pawn shield for castled king (heavy penalty)
 *      c. Pawn storm (bonus for attacker)
 *  13. Trading down bonus
 *
 * TODO list:
 *  1. Endgame table
 *  2. Finer material nuances, like material hash table
 */
int board_score(struct board* board, unsigned char who, struct deltaset* mvs, int alpha, int beta) {
    int rank, file;

    int nmoves = mvs->nmoves;

    if (nmoves == 0 && mvs->check) {
        if (who)
            return (CHECKMATE - board->nmoves);
        else
            return -(CHECKMATE - board->nmoves);
    }
    // Stalemate = draw
    if (nmoves == 0) return 0;
    if (board->nmovesnocapture >= 50) return 0;

    int score = 0;
    uint64_t temp;
    int square, count;

    int whitematerial, blackmaterial;
    whitematerial = material_for_player(board, 0);
    blackmaterial = material_for_player(board, 1);
    score = whitematerial - blackmaterial;

    uint64_t pawns[2], minors[2], majors[2], pieces[2], kings[2], outposts[2], holes[2];

    pawns[0] = P2BM(board, WHITEPAWN);
    pawns[1] = P2BM(board, BLACKPAWN);
    minors[0] = P2BM(board, WHITEKNIGHT) | P2BM(board, WHITEBISHOP);
    minors[1] = P2BM(board, BLACKKNIGHT) | P2BM(board, BLACKBISHOP);
    majors[0] = P2BM(board, WHITEROOK) | P2BM(board, WHITEQUEEN);
    majors[1] = P2BM(board, BLACKROOK) | P2BM(board, BLACKQUEEN);
    kings[0] = P2BM(board, WHITEKING);
    kings[1] = P2BM(board, BLACKKING);

    int nminors[2], nmajors[2], endgame;

    nmajors[0] = bitmap_count_ones(majors[0]);
    nminors[0] = bitmap_count_ones(minors[0]);
    nmajors[1] = bitmap_count_ones(majors[1]);
    nminors[1] = bitmap_count_ones(minors[1]);

    endgame = ((nminors[0] <= 2 && nmajors[0] <= 1) ||
            (nminors[0] == 0 && nmajors[0] <= 2) ||
            (nmajors[0] == 0)) &&
            ((nminors[1] <= 2 && nmajors[1] <= 1) ||
            (nminors[1] <= 2 && nmajors[1] <= 1) ||
            (nmajors[1] == 0));

    if (endgame)
        return board_score_endgame(board, who, mvs);

    // Positional scores don't have THAT huge of an effect, so if the situation is already hopeless, give up
    if (score + 300 < alpha) {
        return score;
    }
    if (score > beta + 300)
        return score;


    uint64_t pawn_attacks[2];

    pawn_attacks[0] = attack_set_pawn_multiple_capture[0](pawns[0], 0, 0xffffffffffffffffull);
    pawn_attacks[1] = attack_set_pawn_multiple_capture[1](pawns[1], 0, 0xffffffffffffffffull);

    pieces[0] = pawns[0] | minors[0] | majors[0] | kings[0];
    pieces[1] = pawns[1] | minors[1] | majors[1] | kings[1];

    uint64_t attacks[2], undefended[2];
    attacks[0] = attacked_squares(board, 0, pieces[0] | pieces[1]);
    attacks[1] = attacked_squares(board, 1, pieces[0] | pieces[1]);

    undefended[0] = attacks[1] ^ (attacks[0] & attacks[1]);
    undefended[1] = attacks[0] ^ (attacks[0] & attacks[1]);

    uint64_t mask;
    struct pawn_structure * pstruct;
    pstruct = evaluate_pawns(board);
    score += pstruct->score;

    // Outposts are squares attacked by your own pawns but not by opponents
    outposts[0] = pstruct->holes[1] & pawn_attacks[0] & 0x00ffffff00000000ull;
    outposts[1] = pstruct->holes[0] & pawn_attacks[1] & 0x00000000ffffff00ull;
    holes[0] = pstruct->holes[1];
    holes[1] = pstruct->holes[0];

    for (int w = 0; w < 2; w++) {
        int subscore = 0;

        bmloop(P2BM(board, 6 * w + KNIGHT), square, temp) {
            file = square & 0x7;
            int loc = (w == 0) ? (56 - square + file + file) : square;
            subscore += knight_table[loc];
            // Outposts are good
            if ((1ull << square) & outposts[w]) subscore += 35;
            else if ((1ull << square) & holes[w]) subscore += 20;
            uint64_t attack_mask = attack_set_knight(square, pieces[w], pieces[1 - w]);
            subscore += 6 * bitmap_count_ones(attack_mask);
#ifdef UNDEFENDED
            if ((1ull << square) & undefended[w]) {
                if (who == w)
                    subscore -= 30;
                else
                    subscore -= 150;
            }
            if (((1ull << square) & pawn_attacks[1 - w]) && who)
                subscore -= 60;
#endif
#ifdef PINNED
            // Pinned pieces aren't great, especially knights
            if ((1ull << square) & mvs->pinned)
                subscore -= 30;
#endif
        }

        count = 0;
        bmloop(P2BM(board, 6 * w + BISHOP), square, temp) {
            file = square & 0x7;
            int loc = (w == 0) ? (56 - square + file + file) : square;
            subscore += bishop_table[loc];
            count += 1;
            if ((1ull << square) & outposts[w]) subscore += 15;
            uint64_t attack_mask = attack_set_bishop(square, pieces[w], pieces[1 - w]);
            subscore += 4 * bitmap_count_ones(attack_mask);
#ifdef UNDEFENDED
            if ((1ull << square) & undefended[w]) {
                if (who == w)
                    subscore -= 30;
                else
                    subscore -= 150;
            }
            if (((1ull << square) & pawn_attacks[1 - w]) && who)
                subscore -= 60;
#endif
            // At least before end-game, central pawns on same
            // colored squares are bad for bishops
            if ((1ull << square) & BLACK_CENTRAL_SQUARES) {
                subscore -= bitmap_count_ones((pawns[0] | pawns[1]) & BLACK_CENTRAL_SQUARES) * 10;
            } else {
                subscore -= bitmap_count_ones((pawns[0] | pawns[1]) & WHITE_CENTRAL_SQUARES) * 10;
            }
#ifdef PINNED
            if ((1ull << square) & mvs->pinned)
                subscore -= 15;
#endif
        }
        // Bishop pairs are very valuable
        // In the endgame, 2 bishops can checkmate a king,
        // whereas 2 knights can't
        subscore += (count == 2) * 50;

        bmloop(P2BM(board, 6 * w + ROOK), square, temp) {
            file = square & 0x7;
            rank = square / 8;
            int loc = (w == 0) ? (56 - square + file + file) : square;
            subscore += rook_table[loc];
            if ((w && rank == 1) || (!w && rank == 6)) {
                // TODO: only do this if king is on rank 8
                // or if there are pawns on rank 7
                subscore += 50;
            }
            if ((1ull << square) & outposts[w]) subscore += 20;
            // Rooks on open files are great
            if (!((AFILE << file) & (pawns[0] | pawns[1])))
                subscore += 30;
            // Rooks on semiopen files are good
            else if (!((AFILE << file) & pawns[w])) {
                subscore += 20;
            }

            // Doubled rooks are very powerful.
            // We add 80 (40 on this, 40 on other)
            if ((AFILE << file) & (majors[w] ^ (1ull << square)))
                subscore += 25;
            uint64_t attack_mask = attack_set_rook(square, pieces[w], pieces[1 - w]);
            subscore += 4 * bitmap_count_ones(attack_mask);

#ifdef UNDEFENDED
            if ((1ull << square) & undefended[w]) {
                if (who == w)
                    subscore -= 30;
                else
                    subscore -= 300;
            }
            if (((1ull << square) & pawn_attacks[1 - w]) && who)
                subscore -= 100;
#endif 

#ifdef PINNED
            if ((1ull << square) & mvs->pinned)
                subscore -= 50;
#endif
        }
        bmloop(P2BM(board, 6 * w + QUEEN), square, temp) {
            file = square & 0x7;
            int loc = (w == 0) ? (56 - square + file + file) : square;
            subscore += queen_table[loc];
            // A queen counts as a rook
            if (!((AFILE << file) & (pawns[0] | pawns[1])))
                subscore += 20;
            else if ((AFILE << file) & pawns[1 - w])
                subscore += 10;

            if ((AFILE << file) & (majors[w] ^ (1ull << square)))
                subscore += 30;

            uint64_t attack_mask = attack_set_queen(square, pieces[w], pieces[1 - w]);
            subscore += bitmap_count_ones(attack_mask & (~attacks[1 - w]));
            subscore += bitmap_count_ones(attack_mask) / 2;

#ifdef UNDEFENDED
            if ((1ull << square) & undefended[w]) {
                if (who == w)
                    subscore -= 40;
                else
                    subscore -= 600;
            }
            if (((1ull << square) & pawn_attacks[1 - w]) && who)
                subscore -= 200;
#endif

#ifdef PINNED
            if ((1ull << square) & mvs->pinned)
                subscore -= 500;
#endif
        }

        square = LSBINDEX(kings[w]);
        file = square & 0x7;
        int loc = (w == 0) ? (56 - square + file + file) : square;
        subscore += king_table[loc];

        mask = AFILE << file;
        if (file != 0)
            mask |= (AFILE << (file - 1));
        if (file != 7)
            mask |= (AFILE << (file + 1));

        // open files are bad news for the king
        if (!((AFILE << file) & pawns[w]))
            subscore -= 15;

        uint64_t mask1, mask2;
        if (w) {
            mask1 = mask << 40;
            mask2 = mask << 32;
        } else {
            mask1 = mask >> 40;
            mask2 = mask >> 32;
        }
        // we want a pawn shield
        if (!(mask1 & pawns[w]))
            subscore -= 30;
        // Uh-oh -- maybe a pawn storm!
        if (mask2 & pawns[1 - w])
            subscore -= 30;

        /*
        uint64_t king_movements;
        king_movements = kings[0] | attack_set_king(square, pieces[0], pieces[1]);
        count = bitmap_count_ones(king_movements & (~attacks[1 - w]));
        if (who == w && mvs->check) {
            if (count <= 3) subscore -= (3 - count) * 30;
            if (count == 0) subscore -= 30;
            subscore -= 20;
        } else if (count <= 1 && (king_movements & attacks[1 - w])) {
            subscore -= (2 - count) *30;
        }
        */

        if (w) score -= subscore;
        else score += subscore;
    }

    /*
    // castling is almost always awesome
    score += (board->castled & 1) * 20 - ((board->castled & 2) >> 1) * 20;
    */
    score += ((board->cancastle & 12) != 0) * 10 - ((board->cancastle & 3) != 0 ) * 10;

    return score;
}

static int kdist(int sq1, int sq2) {
    int rank1, file1, rank2, file2;
    rank1 = sq1 / 8;
    file1 = sq1 % 8;
    rank2 = sq1 / 8;
    file2 = sq2 % 8;
    int dh = abs(rank1-rank2);
    int dv = abs(file1-file2);
    if (dh < dv) return dv;
    else return dh;
}

static int dist(int sq1, int sq2) {
    return kdist(sq1, sq2);
    int rank1, file1, rank2, file2;
    rank1 = sq1 / 8;
    file1 = sq1 % 8;
    rank2 = sq1 / 8;
    file2 = sq2 % 8;
    return abs(rank1-rank2) + abs(file1-file2);
}


int distance_to_score[9] = {40, 40, 40, 30, 20, 15, 10, 5, 0};

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
    return material_hash_wp(bitmap_count_ones(board->pieces[0][QUEEN]), bitmap_count_ones(board->pieces[1][QUEEN]),
        bitmap_count_ones(board->pieces[0][ROOK]), bitmap_count_ones(board->pieces[1][ROOK]),
        bitmap_count_ones(board->pieces[0][BISHOP]), bitmap_count_ones(board->pieces[1][BISHOP]),
        bitmap_count_ones(board->pieces[0][KNIGHT]), bitmap_count_ones(board->pieces[1][KNIGHT]));
}

void initialize_endgame_tables() {
    // -1 is our junk value
    for (int i = 0; i < 6561; i++) {
        insufficient_material_table[i] = -1;
    }
#define PCHECKMATE 4096
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
static int board_score_endgame(struct board* board, unsigned char who, struct deltaset* mvs) {
    int rank, file;

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
    wkingsquare = LSBINDEX(kings[0]);
    bkingsquare = LSBINDEX(kings[1]);

    pieces[0] = pawns[0] | minors[0] | majors[0] | kings[0];
    pieces[1] = pawns[1] | minors[1] | majors[1] | kings[1];

    uint64_t attacks[2], undefended[2];
    attacks[0] = attacked_squares(board, 0, pieces[0] | pieces[1]);
    attacks[1] = attacked_squares(board, 1, pieces[0] | pieces[1]);

    undefended[0] = attacks[1] ^ (attacks[0] & attacks[1]);
    undefended[1] = attacks[0] ^ (attacks[0] & attacks[1]);

    // Draws from insufficient material

    uint64_t mask;

#define ENDGAME_KNOWLEDGE
#ifdef ENDGAME_KNOWLEDGE
    // Only kings
    if (bitmap_count_ones(pieces[0]) == 1 && bitmap_count_ones(pieces[1]) == 1) {
        return 0;
    }

    // No pawns
    if (bitmap_count_ones(pawns[0]) == 0 && bitmap_count_ones(pawns[1]) == 0) {
        int nknights[2], nbishops[2], nrooks[2], nqueens[2];
        for (int i = 0; i < 2; i++) {
            nknights[i] = bitmap_count_ones(board->pieces[i][KNIGHT]);
            nbishops[i] = bitmap_count_ones(board->pieces[i][BISHOP]);
            nrooks[i] = bitmap_count_ones(board->pieces[i][ROOK]);
            nqueens[i] = bitmap_count_ones(board->pieces[i][QUEEN]);
        }
        if (nqueens[0] > 0 && nqueens[1] == 0 && nrooks[1] == 0)
            score += 3000;
        else if (nqueens[1] > 0 && nqueens[0] == 0 && nrooks[0] == 0)
            score -= 3000;
        else if (nqueens[0] > 0 && nqueens[1] == 0 && nrooks[1] == 1 && nbishops[1] == 0 && nknights[1] == 0)
            score += 1000;
        else if (nqueens[1] > 0 && nqueens[0] == 0 && nrooks[0] == 1 && nbishops[0] == 0 && nknights[0] == 0)
            score -= 1000;
        // KNNK is a draw
        else if (nqueens[0] == 0 && nqueens[1] == 0 && nrooks[0] == 0 && nrooks[1] == 0 && nbishops[0] == 0 && nbishops[1] == 0)
            return 0;
        // KBKB is a draw
        else if (nqueens[0] == 0 && nqueens[1] == 0 && nrooks[0] == 0 && nrooks[1] == 0 && nbishops[0] <= 1 && nbishops[1] <= 1 && nknights[0] == 0 && nknights[1] == 0)
            return 0;
    }

    // Pawn + king vs king
    if (bitmap_count_ones(majors[0]) == 0 && bitmap_count_ones(majors[1]) == 0 &&
            bitmap_count_ones(minors[0]) == 0 && bitmap_count_ones(minors[1]) == 0) {
        endgame_type = EG_KPKP;
        if (bitmap_count_ones(pawns[0]) > 1 && bitmap_count_ones(pawns[1]) == 0)
            score += 400;
        if (bitmap_count_ones(pawns[1]) > 1 && bitmap_count_ones(pawns[0]) == 0)
            score -= 400;
        if (bitmap_count_ones(pawns[0]) == 1 && bitmap_count_ones(pawns[1]) == 0) {
            int psquare = LSBINDEX(pawns[0]);
            int prank = psquare / 8;
            int pfile = psquare % 8;
            int qsquare = 56 + pfile;
            if (kdist(qsquare, bkingsquare) + (board->who == 1) < (7 - prank))
                score += 400;
        }
        if (bitmap_count_ones(pawns[1]) == 1 && bitmap_count_ones(pawns[0]) == 0) {
            int psquare = LSBINDEX(pawns[0]);
            int prank = psquare / 8;
            int pfile = psquare % 8;
            int qsquare = pfile;
            if (kdist(qsquare, wkingsquare) + (board->who == 0) < prank)
                score -= 400;
        }
    }


    if (bitmap_count_ones(pieces[0]) == 1) {
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
    } else if (bitmap_count_ones(pieces[1]) == 1) {
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

#endif

    int whitematerial = material_for_player_endgame(board, 0);
    int blackmaterial = material_for_player_endgame(board, 1);
    struct pawn_structure* pstruct;
    pstruct = evaluate_pawns(board);
    score = whitematerial - blackmaterial;
    score += pstruct->score_eg;

    bmloop(P2BM(board, WHITEPAWN), square, temp) {
        // Encourage kings to come and protect these pawns
        int dist_from_wking = dist(square, wkingsquare);
        int dist_from_bking = dist(square, bkingsquare);
        int dist_score = 0;
        dist_score += (dist_from_bking - dist_from_wking) * 15;
        // passed pawns are good
        if (pstruct->passed_pawns[0] & (1ull << square)) {
            dist_score += (dist_from_bking - dist_from_wking) * 30;
        }
        if (endgame_type == EG_KPKP)
            dist_score *= 2;

        score += dist_score;
    }

    bmloop(P2BM(board, BLACKPAWN), square, temp) {
        int dist_from_wking = dist(square, wkingsquare);
        int dist_from_bking = dist(square, bkingsquare);
        int dist_score = 0;
        dist_score += (dist_from_bking - dist_from_wking) * 15;

        if (pstruct->passed_pawns[0] & (1ull << square)) {
            dist_score += (dist_from_bking - dist_from_wking) * 30;
        }

        if (endgame_type == EG_KPKP) {
            dist_score *= 2;
        }

        score -= dist_score;
    }

    bmloop(P2BM(board, WHITEKNIGHT), square, temp) {
        file = square & 0x7;
        score += knight_table[56 - square + file + file];
    }
    bmloop(P2BM(board, BLACKKNIGHT), square, temp) {
        score -= knight_table[square];
    }

    count = 0;
    bmloop(P2BM(board, WHITEBISHOP), square, temp) {
        file = square & 0x7;
        score += bishop_table[56 - square + file + file];
        count += 1;
    }
    // Bishop pairs are very valuable
    // In the endgame, 2 bishops can checkmate a king,
    // whereas 2 knights can't
    score += (count == 2) * 100;

    count = 0;
    bmloop(P2BM(board, BLACKBISHOP), square, temp) {
        score -= bishop_table[square];
        count += 1;
    }

    score -= (count == 2) * 100;

    bmloop(P2BM(board, WHITEROOK), square, temp) {
        file = square & 0x7;
        // Rooks on open files are great
        if (!((AFILE << file) & (pawns[0] | pawns[1])))
            score += 30;
        // Rooks on semiopen files are good
        else if ((AFILE << file) & pawns[1])
            score += 20;

        // Doubled rooks are very powerful.
        // We add 80 (40 on this, 40 on other)
        if ((AFILE << file) & (majors[0] ^ (1ull << square)))
            score += 30;
    }
    bmloop(P2BM(board, BLACKROOK), square, temp) {
        file = square & 0x7;
        if (!((AFILE << file) & (pawns[0] | pawns[1])))
            score -= 30;
        else if ((AFILE << file) & pawns[0])
            score -= 20;

        if ((AFILE << file) & (majors[1] ^ (1ull << square)))
            score -= 30;
    }
    bmloop(P2BM(board, WHITEQUEEN), square, temp) {
        file = square & 0x7;
        // A queen counts as a rook
        if (!((AFILE << file) & (pawns[0] | pawns[1])))
            score += 20;
        else if ((AFILE << file) & pawns[1])
            score += 10;

        if ((AFILE << file) & (majors[0] ^ (1ull << square)))
            score += 30;
    }
    bmloop(P2BM(board, BLACKQUEEN), square, temp) {
        if (!((AFILE << file) & (pawns[0] | pawns[1])))
            score -= 20;
        else if ((AFILE << file) & pawns[0])
            score -= 10;

        if ((AFILE << file) & (majors[1] ^ (1ull << square)))
            score -= 30;
    }

    int winning_side = 0;
    if (whitematerial > blackmaterial)
        winning_side = 1;
    else if (whitematerial < blackmaterial)
        winning_side = -1;


    // Encourage the winning side to move kings closer together
    score += winning_side * distance_to_score[dist(wkingsquare, bkingsquare)];

    // King safety

    file = wkingsquare & 0x7;

    score += king_table_endgame[56 - wkingsquare + file + file];
    score -= king_table_endgame[bkingsquare];

    /*
    uint64_t king_movements;
    king_movements = kings[0] | attack_set_king(wkingsquare, pieces[0], pieces[1]);
    count = bitmap_count_ones(king_movements & (~attacks[1]));
    if (who == 0 && mvs->check) {
        if (count <= 4) score -= (4 - count) * 60;
        if (count == 0) score -= 100;
        score -= 20;
    } else if (count <= 2 && (king_movements & (~attacks[1]))) {
        score -= (3 - count) *30;
    } else if (count == 0) score -= 20;

    file = bkingsquare & 0x7;

    king_movements = kings[1] | attack_set_king(bkingsquare, pieces[1], pieces[0]);
    count = bitmap_count_ones(king_movements & (~attacks[0]));
    if (who == 1 && mvs->check) {
        if (count <= 4) score += (4 - count) * 60;
        if (count == 0) score += 100;
        score += 20;
    } else if (count <= 2 && (king_movements & (~attacks[0]))) {
        score += (3 - count) *30;
    } else if (count == 0) score += 20;

    // the side with more options is better
    score += (bitmap_count_ones(attacks[0]) - bitmap_count_ones(attacks[1])) * 8;
    */

    return score;
}
