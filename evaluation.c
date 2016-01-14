#include "board.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pieces.h"
#include "moves.h"

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
int pawn_table[64] = {
    800, 800, 800, 800, 800, 800, 800, 800,
    30, 40, 50, 50, 50, 50, 40, 30, 
    20, 30, 30, 35, 35, 30, 30, 20,
    10, 15, 20, 25, 25, 20, 15, 10,
    5, 10, 10, 15, 15, 10, 10, 5,
    0, 0, 10, 10, 10, 10, 0, 0,
    0, 0, 0, -5, -5, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0.0, 
};

int pawn_table_endgame[64] = {
    800, 800, 800, 800, 800, 800, 800, 800,
    120, 200, 300, 300, 300, 300, 200, 120, 
    70, 80, 85, 90, 90, 85, 80, 70,
    10, 30, 35, 50, 50, 35, 30, 10,
    10, 20, 20, 40, 40, 20, 20, 10,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0.0, 
};

int knight_table[64] = {
    -20, -15, -15, -15, -15, -15, -15, -20,
     0,  10,  10,  10,  10,  10,  10,  0,
     0,  10,  20,  20,  20,  20,  10,  0,
    -10, 0,  20,  30,  30,  20, 0, -10,
    -10, 0,  20,  30,  30,  20, 0, -10,
    -15, 0,  15,  15,  15,  15, 0, -15,
    -20, -10, -10, -10, -10, -10, -10, -20,
    -30, -15, -15, -15, -15, -15, -15, -30,

};

int bishop_table[64] = {
    -20, -20, -20, -20, -20, -20, -20, -20,
      0,  20, 30,  30,  30,  30,  10,   0,
     10,  10,  10,  10,  10,  10,  10,  10,
    0,  0,  10,  20,  20,  10,  0, 0,
    0,  0,  10,  20,  20,  10,  0, 0,
    0,  5,  20,  10,  10,  10,  5, 0,
    -10,  10,  0,  0,  0,  0,  10, -10,
    -20, -20, -20, -20, -20, -20, -20, -20,
};

int rook_table[64] = {
    50, 50, 50, 50, 50, 50, 50, 50,
    70, 70, 70, 70, 70, 70, 70, 70,
    0, 0, 10, 15, 15, 10, 0, 0,
    0, 0, 10, 15, 15, 10, 0, 0,
    0, 0, 10, 15, 15, 10, 0, 0,
    0, 0, 10, 15, 15, 10, 0, 0,
    0, 0, 10, 15, 15, 10, 0, 0,
    -5, 0, 10, 15, 15, 10, 0, -5,
};

int queen_table[64] = {
    30, 30, 30, 30, 30, 30, 30, 30,
    40, 40, 50, 50, 50, 50, 40, 40,
    30, 40, 40, 50, 50, 40, 40, 40,
    0, 0, 20, 30, 30, 20, 0, 0,
    0, 0, 20, 30, 30, 20, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
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

int passed_pawn_table[8] = {0, 0, 0, 10, 15, 20, 50, 800};
int passed_pawn_table_endgame[8] = {0, 20, 30, 40, 77, 154, 256, 800};

#define BLACK_CENTRAL_SQUARES 0x0000281428140000ull
#define WHITE_CENTRAL_SQUARES 0x0000142814280000ull

int material_for_player(struct board* board, unsigned char who) {
    return 100 * bitmap_count_ones(board->pieces[who][PAWN]) +
        320 * bitmap_count_ones(board->pieces[who][KNIGHT]) +
        333 * bitmap_count_ones(board->pieces[who][BISHOP]) +
        510 * bitmap_count_ones(board->pieces[who][ROOK]) +
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
#define UNDEFENDED
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

    // Positional scores don't have THAT huge of an effect, so if the situation is already hopeless, give up
    if (score + 300 < alpha) {
        return score;
    }
    if (score > beta + 300)
        return score;

    uint64_t pawns[2], minors[2], majors[2], pieces[2], kings[2], outposts[2];

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

    uint64_t pawn_attacks[2];

    pawn_attacks[0] = attack_set_pawn_multiple_capture[0](pawns[0], 0, 0xffffffffffffffffull);
    pawn_attacks[1] = attack_set_pawn_multiple_capture[1](pawns[1], 0, 0xffffffffffffffffull);

    // Outposts are squares attacked by your own pawns but not by opponents
    outposts[0] = pawn_attacks[0] & (~pawn_attacks[1]) & 0x00ffffff00000000ull;
    outposts[1] = pawn_attacks[1] & (~pawn_attacks[0]) & 0x00000000ffffff00ull;

    pieces[0] = pawns[0] | minors[0] | majors[0] | kings[0];
    pieces[1] = pawns[1] | minors[1] | majors[1] | kings[1];

    uint64_t attacks[2], undefended[2];
    attacks[0] = attacked_squares(board, 0, pieces[0] | pieces[1]);
    attacks[1] = attacked_squares(board, 1, pieces[0] | pieces[1]);

    undefended[0] = attacks[1] ^ (attacks[0] & attacks[1]);
    undefended[1] = attacks[0] ^ (attacks[0] & attacks[1]);

    // undeveloped pieces penalty
    if (RANK1 & minors[0])
        score -= 40;
    if (RANK8 & minors[1])
        score += 40;

    uint64_t mask;

    for (int w = 0; w < 2; w++) {
        int subscore = 0;
        count = 0;
        bmloop(P2BM(board, 6 * w + PAWN), square, temp) {
            count += 1;
            rank = square / 8;
            file = square & 0x7;
            int loc = (w == 0) ? (56 - square + file + file) : square;
            subscore += pawn_table[loc];
#ifdef UNDEFENDED
            // undefended pieces are likely taken
            if ((1ull << square) & undefended[w]) {
                if (who == w)
                    subscore -= 10;
                else
                    subscore -= 50;
            }
#endif
#ifdef PINNED
            // Pinned pieces aren't great
            if ((1ull << square) & mvs->pinned)
                subscore -= 10;
#endif
            // doubled pawns are bad
            if ((AFILE << file)  & (pawns[w] ^ (1ull << square)))
                subscore -= 15;
            // passed pawns are good
            if (!((AFILE << square) & pawns[1 - w])) {
                if (w)
                    subscore += passed_pawn_table[7 - rank];
                else
                    subscore += passed_pawn_table[rank];
            }

            mask = 0;
            // isolated pawns are bad
            if (file != 0) mask |= (AFILE << (file - 1));
            if (file != 7) mask |= (AFILE << (file + 1));
            if (!(mask & pawns[w])) subscore -= 20;
        }
        // If you have no pawns, endgames will be hard
        if (!count) subscore -= 120;

        bmloop(P2BM(board, 6 * w + KNIGHT), square, temp) {
            file = square & 0x7;
            int loc = (w == 0) ? (56 - square + file + file) : square;
            subscore += knight_table[loc];
            // Outposts are good
            if ((1ull << square) & outposts[w]) subscore += 35;
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
            if ((1ull << square) & outposts[w]) subscore += 20;
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
                subscore -= bitmap_count_ones((pawns[0] | pawns[1]) & BLACK_CENTRAL_SQUARES) * 15;
            } else {
                subscore -= bitmap_count_ones((pawns[0] | pawns[1]) & WHITE_CENTRAL_SQUARES) * 15;
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
            int loc = (w == 0) ? (56 - square + file + file) : square;
            subscore += rook_table[loc];
            if ((1ull << square) & outposts[w]) subscore += 20;
            // Rooks on open files are great
            if (!((AFILE << file) & (pawns[0] | pawns[1])))
                subscore += 20;
            // Rooks on semiopen files are good
            else if ((AFILE << file) & pawns[1-w])
                subscore += 10;

            // Doubled rooks are very powerful.
            // We add 80 (40 on this, 40 on other)
            if ((AFILE << file) & (majors[w] ^ (1ull << square)))
                subscore += 25;

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
        bmloop(P2BM(board, WHITEQUEEN), square, temp) {
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
            mask1 = mask >> 40;
            mask2 = mask >> 32;
        } else {
            mask1 = mask << 40;
            mask2 = mask << 32;
        }
        // we want a pawn shield
        if ((board->castled & 1) && !(mask1 & pawns[w]))
            subscore -= 30;
        // Uh-oh -- maybe a pawn storm!
        if (mask2 & pawns[1 - w])
            subscore -= 30;

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

        if (w) score -= subscore;
        else score += subscore;
    }

    // Trading down is good for the side with more material
    if (whitematerial - blackmaterial > 0)
        score += (4006 - blackmaterial) / 8;
    else if (whitematerial - blackmaterial < 0)
        score -= (4006 - whitematerial) / 8;

    // castling is almost always awesome
    score += (board->castled & 1) * 75 - ((board->castled & 2) >> 1) * 75;
    score += ((board->cancastle & 12) != 0) * 25 - ((board->cancastle & 3) != 0 ) * 25;

    // the side with more options is better
    score += (bitmap_count_ones(attacks[0]) - bitmap_count_ones(attacks[1])) * 8;

    return score;
}

static int dist(int sq1, int sq2) {
    int rank1, file1, rank2, file2;
    rank1 = sq1 / 8;
    file1 = sq1 % 8;
    rank2 = sq1 / 8;
    file2 = sq2 % 8;
    return abs(rank1-rank2) + abs(file1-file2);
}

// Endgame behames very differently, so we have a separate scoring function
static int board_score_endgame(struct board* board, unsigned char who, struct deltaset* mvs) {
    int rank, file;

    int score = 0;
    uint64_t temp;
    int square, count;

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

    if (bitmap_count_ones(pieces[0]) == 1 && bitmap_count_ones(pieces[1]) == 1) {
        // This is a draw
        return 0;
    }

    uint64_t mask;
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
        score -= (7 - dist(wkingsquare, bkingsquare)) * 30;
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
        score += (7 - dist(wkingsquare, bkingsquare)) * 30;
    }

    int whitematerial, blackmaterial;
    whitematerial = 0;
    blackmaterial = 0;


    count = 0;
    bmloop(P2BM(board, WHITEPAWN), square, temp) {
        count += 1;
        rank = square / 8;
        file = square & 0x7;
        whitematerial += 135;
        score += pawn_table_endgame[56 - square + file + file];
        // undefended pieces are likely taken
        if ((1ull << square) & undefended[0])
            score -= 50;
        // doubled pawns are bad
        if ((AFILE << file)  & (pawns[0] ^ (1ull << square)))
            score -= 20;
        
        // Encourage kings to come and protect these pawns
        int dist_from_wking = dist(square, wkingsquare);
        int dist_from_bking = dist(square, bkingsquare);
        score += (dist_from_bking - dist_from_wking) * 20;
        // passed pawns are good
        if (!((AFILE << square) & pawns[1])) {
            score += passed_pawn_table_endgame[rank];
            score += (dist_from_bking - dist_from_wking) * 40;
        }

        mask = 0;
        // isolated pawns are bad
        if (file != 0) mask |= (AFILE << (file - 1));
        if (file != 7) mask |= (AFILE << (file + 1));
        if (!(mask & pawns[0])) score -= 20;
    }
    // If you have no pawns, endgames will be hard
    if (!count) score -= 120;

    count = 0;
    bmloop(P2BM(board, BLACKPAWN), square, temp) {
        count += 1;
        file = square & 0x7;
        rank = square / 8;
        blackmaterial += 135;
        score -= pawn_table_endgame[square];
        if ((1ull << square) & undefended[1])
            score += 50;
        if ((AFILE << file) & (pawns[1] ^ (1ull << square)))
            score += 20;

        int dist_from_wking = dist(square, wkingsquare);
        int dist_from_bking = dist(square, bkingsquare);
        score += (dist_from_bking - dist_from_wking) * 20;

        if (!(((AFILE << file) >> (56 - 8 * rank)) & pawns[0])) {
            score -= passed_pawn_table_endgame[8-rank];
            score += (dist_from_bking - dist_from_wking) * 40;
        }

        mask = 0;
        if (file != 0) mask |= (AFILE << (file - 1));
        if (file != 7) mask |= (AFILE << (file + 1));
        if (!(mask & pawns[1])) score += 20;
    }
    if (!count) score += 120;

    bmloop(P2BM(board, WHITEKNIGHT), square, temp) {
        file = square & 0x7;
        whitematerial += 275;
        score += knight_table[56 - square + file + file];
        if ((1ull << square) & undefended[0])
            score -= 150;
    }
    bmloop(P2BM(board, BLACKKNIGHT), square, temp) {
        blackmaterial += 275;
        score -= knight_table[square];
        if ((1ull << square) & undefended[1])
            score += 150;
    }

    count = 0;
    bmloop(P2BM(board, WHITEBISHOP), square, temp) {
        file = square & 0x7;
        whitematerial += 300;
        score += bishop_table[56 - square + file + file];
        count += 1;
        if ((1ull << square) & undefended[0])
            score -= 150;
    }
    // Bishop pairs are very valuable
    // In the endgame, 2 bishops can checkmate a king,
    // whereas 2 knights can't
    score += (count == 2) * 100;

    count = 0;
    bmloop(P2BM(board, BLACKBISHOP), square, temp) {
        blackmaterial += 300;
        score -= bishop_table[square];
        count += 1;
        if ((1ull << square) & undefended[1])
            score += 150;
    }

    score -= (count == 2) * 100;

    bmloop(P2BM(board, WHITEROOK), square, temp) {
        file = square & 0x7;
        whitematerial += 550;
        // Rooks on open files are great
        if (!((AFILE << file) & (pawns[0] | pawns[1])))
            score += 20;
        // Rooks on semiopen files are good
        else if ((AFILE << file) & pawns[1])
            score += 10;

        // Doubled rooks are very powerful.
        // We add 80 (40 on this, 40 on other)
        if ((AFILE << file) & (majors[0] ^ (1ull << square)))
            score += 30;

        if ((1ull << square) & undefended[0])
            score -= 320;
    }
    bmloop(P2BM(board, BLACKROOK), square, temp) {
        file = square & 0x7;
        blackmaterial += 550;
        if (!((AFILE << file) & (pawns[0] | pawns[1])))
            score -= 20;
        else if ((AFILE << file) & pawns[0])
            score -= 10;

        if ((AFILE << file) & (majors[1] ^ (1ull << square)))
            score -= 30;

        if ((1ull << square) & undefended[1])
            score += 320;
    }
    bmloop(P2BM(board, WHITEQUEEN), square, temp) {
        file = square & 0x7;
        whitematerial += 880;
        // A queen counts as a rook
        if (!((AFILE << file) & (pawns[0] | pawns[1])))
            score += 20;
        else if ((AFILE << file) & pawns[1])
            score += 10;

        if ((AFILE << file) & (majors[0] ^ (1ull << square)))
            score += 30;

        if ((1ull << square) & undefended[0])
            score -= 620;
    }
    bmloop(P2BM(board, BLACKQUEEN), square, temp) {
        blackmaterial += 880;
        if (!((AFILE << file) & (pawns[0] | pawns[1])))
            score -= 20;
        else if ((AFILE << file) & pawns[0])
            score -= 10;

        if ((AFILE << file) & (majors[1] ^ (1ull << square)))
            score -= 30;

        if ((1ull << square) & undefended[1])
            score += 620;
    }

    score += (whitematerial - blackmaterial);

    // Trading down is good for the side with more material
    // TODO: Be careful when trading down to avoid draws!
    if (whitematerial - blackmaterial > 0)
        score += (4006 - blackmaterial) / 16;
    else if (whitematerial - blackmaterial < 0)
        score -= (4006 - whitematerial) / 16;

    int winning_side = 1;
    if (whitematerial > blackmaterial)
        winning_side = 1;
    else if (whitematerial < blackmaterial)
        winning_side = -1;


    // Encourage the winning side to move kings closer together
    score += winning_side * (14 - dist(wkingsquare, bkingsquare));

    // King safety

    file = wkingsquare & 0x7;

    score += king_table_endgame[56 - wkingsquare + file + file];

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
    score -= king_table_endgame[bkingsquare];

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

    return score;
}
