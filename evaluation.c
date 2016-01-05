#include "board.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pieces.h"

#define AFILE 0x0101010101010101ull
#define HFILE 0x8080808080808080ull
#define RANK1 0x00000000000000ffull
#define RANK2 0x000000000000ff00ull
#define RANK3 0x0000000000ff0000ull
#define RANK6 0x0000ff0000000000ull
#define RANK7 0x00ff000000000000ull
#define RANK8 0xff00000000000000ull

int abs(int s) {
    if (s > 0) return s;
    return -s;
}


int board_score_endgame(struct board* board, char who, struct deltaset* mvs, int nmoves);

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
    30, 35, 40, 40, 40, 40, 35, 30,
    10, 30, 30, 35, 35, 30, 30, 10,
    10, 20, 20, 30, 30, 20, 20, 10,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 0, -10, -10, 0, 0, 0,
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
    -30, -20, -10, -10, -10, -10, -20, -30,
     20,  20,  30,  30,  30,  30,  20,  20,
     20,  30,  35,  35,  35,  35,  30,  20,
    -30, -10,  25,  30,  30,  25, -10, -30,
    -30, -10,  25,  30,  30,  25, -10, -30,
    -30, -10,  35,  25,  25,  35, -10, -30,
    -40, -10, -10, -10, -10, -10, -10, -40,
    -50, -40, -30, -30, -30, -30, -40, -50,

};

int bishop_table[64] = {
    -20, -20, -20, -20, -20, -20, -20, -20,
      0,  20, 30,  30,  30,  30,  10,   0,
     30,  30,  30,  30,  30,  30,  0,  30,
    -30,  0,  10,  20,  20,  10,  0, -30,
    -30,  0,  10,  20,  20,  10,  0, -30,
    -30,  0,  20,  10,  10,  10,  0, -30,
    -30,  10,  0,  0,  0,  0,  10, -30,
    -40, -40, -40, -40, -40, -40, -40, -40,
};

int rook_table[64] = {
    70, 70, 70, 70, 70, 70, 70, 70,
    80, 80, 80, 80, 80, 80, 80, 80,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 10, 10, 10, 10, 0, 0,
};

int queen_table[64] = {
    70, 70, 70, 70, 70, 70, 70, 70,
    60, 60, 60, 60, 60, 60, 60, 60,
    40, 40, 40, 50, 50, 40, 40, 40,
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

int passed_pawn_table[8] = {0, 0, 0, 26, 77, 154, 256, 800};
int passed_pawn_table_endgame[8] = {0, 20, 30, 40, 77, 154, 256, 800};

#define BLACK_CENTRAL_SQUARES 0x0000281428140000ull
#define WHITE_CENTRAL_SQUARES 0x0000142814280000ull

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
int board_score(struct board* board, char who, struct deltaset* mvs, int nmoves) {
    int i;
    int seen;
    int rank, file, pwho;
    piece_t piece;

    if (nmoves < 0)
        nmoves = board_nmoves_accurate(board, who);

    who = -who * 2 + 1;
    if (nmoves == 0 && mvs->check) {
        return -(CHECKMATE - board->nmoves) * who;
    }
    // Stalemate = draw
    if (nmoves == 0) return 0;
    if (board->nmovesnocapture >= 50) return 0;

    int score = 0;
    seen = 0;
    uint64_t temp;
    int square, count;

    uint64_t white_pawns, white_minor, white_major, black_pawns,
             black_minor, black_major, white, black, white_king, black_king;
    white_pawns = P2BM(board, WHITEPAWN);
    black_pawns = P2BM(board, BLACKPAWN);
    white_minor = P2BM(board, WHITEKNIGHT) | P2BM(board, WHITEBISHOP);
    black_minor = P2BM(board, BLACKKNIGHT) | P2BM(board, BLACKBISHOP);
    white_major = P2BM(board, WHITEROOK) | P2BM(board, WHITEQUEEN);
    black_major = P2BM(board, BLACKROOK) | P2BM(board, BLACKQUEEN);
    white_king = P2BM(board, WHITEKING);
    black_king = P2BM(board, BLACKKING);

    white = white_pawns | white_minor | white_major | white_king;
    black = black_pawns | black_minor | black_major | black_king;

    uint64_t whiteattack, blackattack, whiteundefended, blackundefended;
    whiteattack = attacked_squares(board, 0, white | black);
    blackattack = attacked_squares(board, 1, black | white);

    whiteundefended = blackattack ^ (whiteattack & blackattack);
    blackundefended = whiteattack ^ (whiteattack & blackattack);

    int nwhiteminor, nwhitemajor, nblackmajor, nblackminor, endgame;
    nwhitemajor = bitmap_count_ones(white_major);
    nwhiteminor = bitmap_count_ones(white_minor);
    nblackmajor = bitmap_count_ones(black_major);
    nblackminor = bitmap_count_ones(black_minor);

    endgame = ((nwhiteminor <= 2 && nwhitemajor <= 1) ||
            (nwhiteminor == 0 && nwhitemajor <= 2) ||
            (nblackmajor == 0)) &&
            ((nblackminor <= 2 && nblackmajor <= 1) ||
            (nblackminor <= 2 && nblackmajor <= 1) ||
            (nblackmajor == 0));

    if (endgame)
        return board_score_endgame(board, who, mvs, nmoves);

    // undeveloped pieces penalty
    if (RANK1 & white_minor)
        score -= 40;
    if (RANK8 & black_minor)
        score += 40;

    int whitematerial, blackmaterial;
    whitematerial = 0;
    blackmaterial = 0;

    uint64_t mask;

    count = 0;
    bmloop(P2BM(board, WHITEPAWN), square, temp) {
        count += 1;
        rank = square / 8;
        file = square & 0x7;
        whitematerial += 100;
        score += pawn_table[56 - square + file + file];
        // undefended pieces are likely taken
        if ((1ull << square) & whiteundefended)
            score -= 50;
        // doubled pawns are bad
        if ((AFILE << file)  & (white_pawns ^ (1ull << square)))
            score -= 20;
        // passed pawns are good
        if (!((AFILE << square) & black_pawns))
            score += passed_pawn_table[rank];

        mask = 0;
        // isolated pawns are bad
        if (file != 0) mask |= (AFILE << (file - 1));
        if (file != 7) mask |= (AFILE << (file + 1));
        if (!(mask & white_pawns)) score -= 20;
    }
    // If you have no pawns, endgames will be hard
    if (!count) score -= 120;

    count = 0;
    bmloop(P2BM(board, BLACKPAWN), square, temp) {
        count += 1;
        file = square & 0x7;
        rank = square / 8;
        blackmaterial += 100;
        score -= pawn_table[square];
        if ((1ull << square) & blackundefended)
            score += 50;
        if ((AFILE << file) & (black_pawns ^ (1ull << square)))
            score += 20;
        if (!(((AFILE << file) >> (56 - 8 * rank)) & white_pawns))
            score -= passed_pawn_table[8-rank];

        mask = 0;
        if (file != 0) mask |= (AFILE << (file - 1));
        if (file != 7) mask |= (AFILE << (file + 1));
        if (!(mask & black_pawns)) score += 20;
    }
    if (!count) score += 120;

    bmloop(P2BM(board, WHITEKNIGHT), square, temp) {
        file = square & 0x7;
        whitematerial += 320;
        score += knight_table[56 - square + file + file];
        if ((1ull << square) & whiteundefended)
            score -= 150;
    }
    bmloop(P2BM(board, BLACKKNIGHT), square, temp) {
        blackmaterial += 320;
        score -= knight_table[square];
        if ((1ull << square) & blackundefended)
            score += 150;
    }

    count = 0;
    bmloop(P2BM(board, WHITEBISHOP), square, temp) {
        file = square & 0x7;
        whitematerial += 333;
        score += bishop_table[56 - square + file + file];
        count += 1;
        if ((1ull << square) & whiteundefended)
            score -= 150;
        // At least before end-game, central pawns on same
        // colored squares are bad for bishops
        if ((1ull << square) & BLACK_CENTRAL_SQUARES) {
            score -= bitmap_count_ones((white_pawns | black_pawns) & BLACK_CENTRAL_SQUARES) * 15;
        } else {
            score -= bitmap_count_ones((white_pawns | black_pawns) & WHITE_CENTRAL_SQUARES) * 15;
        }
    }
    // Bishop pairs are very valuable
    // In the endgame, 2 bishops can checkmate a king,
    // whereas 2 knights can't
    score += (count == 2) * 50;

    count = 0;
    bmloop(P2BM(board, BLACKBISHOP), square, temp) {
        blackmaterial += 333;
        score -= bishop_table[square];
        count += 1;
        if ((1ull << square) & blackundefended)
            score += 150;
        if ((1ull << square) & BLACK_CENTRAL_SQUARES) {
            score += bitmap_count_ones((white_pawns | black_pawns) & BLACK_CENTRAL_SQUARES) * 15;
        } else {
            score += bitmap_count_ones((white_pawns | black_pawns) & WHITE_CENTRAL_SQUARES) * 15;
        }
    }

    score -= (count == 2) * 50;

    bmloop(P2BM(board, WHITEROOK), square, temp) {
        file = square & 0x7;
        whitematerial += 510;
        score += rook_table[56 - square + file + file];
        // Rooks on open files are great
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score += 20;
        // Rooks on semiopen files are good
        else if ((AFILE << file) & black_pawns)
            score += 10;

        // Doubled rooks are very powerful.
        // We add 80 (40 on this, 40 on other)
        if ((AFILE << file) & (white_major ^ (1ull << square)))
            score += 30;

        if ((1ull << square) & whiteundefended)
            score -= 320;
    }
    bmloop(P2BM(board, BLACKROOK), square, temp) {
        file = square & 0x7;
        blackmaterial += 510;
        score -= rook_table[square];
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score -= 20;
        else if ((AFILE << file) & white_pawns)
            score -= 10;

        if ((AFILE << file) & (black_major ^ (1ull << square)))
            score -= 30;

        if ((1ull << square) & blackundefended)
            score += 320;
    }
    bmloop(P2BM(board, WHITEQUEEN), square, temp) {
        file = square & 0x7;
        whitematerial += 900;
        score += queen_table[56 - square + file + file];
        // A queen counts as a rook
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score += 20;
        else if ((AFILE << file) & black_pawns)
            score += 10;

        if ((AFILE << file) & (white_major ^ (1ull << square)))
            score += 30;

        if ((1ull << square) & whiteundefended)
            score -= 620;
    }
    bmloop(P2BM(board, BLACKQUEEN), square, temp) {
        blackmaterial += 900;
        score -= queen_table[square];
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score -= 20;
        else if ((AFILE << file) & white_pawns)
            score -= 10;

        if ((AFILE << file) & (black_major ^ (1ull << square)))
            score -= 30;

        if ((1ull << square) & blackundefended)
            score += 620;
    }

    score += (whitematerial - blackmaterial);

    // Trading down is good for the side with more material
    if (whitematerial - blackmaterial > 0)
        score += (4006 - blackmaterial) / 8;
    else if (whitematerial - blackmaterial < 0)
        score -= (4006 - whitematerial) / 8;

    // castling is almost always awesome
    score += (board->castled & 1) * 100 - ((board->castled & 2) >> 1) * 100;
    score += ((board->cancastle & 12) != 0) * 100 - ((board->cancastle & 3) != 0 ) * 100;

    // King safety

    square = LSBINDEX(white_king);
    file = square & 0x7;

    score += king_table[56 - square + file + file];

    mask = AFILE << file;
    if (file != 0)
        mask |= (AFILE << (file - 1));
    if (file != 7)
        mask |= (AFILE << (file + 1));

    // open files are bad news for the king
    if (!((AFILE << file) & white_pawns))
        score -= 15;
    // we want a pawn shield
    if ((board->castled & 1) && !((mask >> 32) & white_pawns))
        score -= 10;
    if ((board->castled & 1) && !((mask >> 40) & white_pawns))
        score -= 10;
    if ((board->castled & 1) && !((mask >> 48) & white_pawns))
        score -= 10;
    // Uh-oh -- maybe a pawn storm!
    if ((mask >> 32) & black_pawns)
        score -= 10;
    if ((mask >> 40) & black_pawns)
        score -= 30;
    if ((mask >> 48) & black_pawns)
        score -= 50;

    uint64_t king_movements;
    king_movements = white_king | attack_set_king(square, white, black);
    count = bitmap_count_ones(king_movements & (~blackattack));
    if (who == 1 && mvs->check) {
        if (count <= 4) score -= (4 - count) * 60;
        if (count == 0) score -= 100;
        score -= 20;
    } else if (count <= 2 && (king_movements & (~blackattack))) {
        score -= (3 - count) *30;
    } else if (count == 0) score -= 20;

    square = LSBINDEX(black_king);
    file = square & 0x7;
    score -= king_table[square];

    mask = AFILE << file;
    if (file != 0)
        mask |= (AFILE << (file - 1));
    if (file != 7)
        mask |= (AFILE << (file + 1));

    if (!((AFILE << file) & black_pawns)) score -= 30;
    if ((board->castled & 1) && !((mask << 32) & black_pawns))
        score += 15;
    if ((board->castled & 1) && !((mask << 40) & black_pawns))
        score += 10;
    if ((board->castled & 1) && !((mask << 48) & black_pawns))
        score += 10;
    if ((mask << 32) & white_pawns)
        score += 10;
    if ((mask << 40) & white_pawns)
        score += 30;
    if ((mask << 48) & white_pawns)
        score += 50;

    king_movements = black_king | attack_set_king(square, black, white);
    count = bitmap_count_ones(king_movements & (~whiteattack));
    if (who == -1 && mvs->check) {
        if (count <= 4) score += (4 - count) * 60;
        if (count == 0) score += 100;
        score += 20;
    } else if (count <= 2 && (king_movements & (~whiteattack))) {
        score += (3 - count) *30;
    } else if (count == 0) score += 20;


    // the side with more options is better
    score += (bitmap_count_ones(whiteattack) - bitmap_count_ones(blackattack)) * 8;

    return score;
}


int dist(int sq1, int sq2) {
    int rank1, file1, rank2, file2;
    rank1 = sq1/8;
    file1 = sq1%8;
    rank2 = sq1/8;
    file2 = sq2%8;
    return abs(rank1-rank2) + abs(file1-file2);
}

// Endgame behames very differently, so we have a separate scoring function
int board_score_endgame(struct board* board, char who, struct deltaset* mvs, int nmoves) {
    int i;
    int seen;
    int rank, file, pwho;
    piece_t piece;

    who = -who * 2 + 1;

    int score = 0;
    seen = 0;
    uint64_t temp;
    int square, count;

    uint64_t white_pawns, white_minor, white_major, black_pawns,
             black_minor, black_major, white, black, white_king, black_king;
    white_pawns = P2BM(board, WHITEPAWN);
    black_pawns = P2BM(board, BLACKPAWN);
    white_minor = P2BM(board, WHITEKNIGHT) | P2BM(board, WHITEBISHOP);
    black_minor = P2BM(board, BLACKKNIGHT) | P2BM(board, BLACKBISHOP);
    white_major = P2BM(board, WHITEROOK) | P2BM(board, WHITEQUEEN);
    black_major = P2BM(board, BLACKROOK) | P2BM(board, BLACKQUEEN);
    white_king = P2BM(board, WHITEKING);
    black_king = P2BM(board, BLACKKING);

    int wkingsquare, bkingsquare;
    wkingsquare = LSBINDEX(white_king);
    bkingsquare = LSBINDEX(black_king);

    white = white_pawns | white_minor | white_major | white_king;
    black = black_pawns | black_minor | black_major | black_king;

    uint64_t whiteattack, blackattack, whiteundefended, blackundefended;
    whiteattack = attacked_squares(board, 0, white | black);
    blackattack = attacked_squares(board, 1, black | white);

    whiteundefended = blackattack ^ (whiteattack & blackattack);
    blackundefended = whiteattack ^ (whiteattack & blackattack);

    if (bitmap_count_ones(white) == 1 && bitmap_count_ones(black) == 1) {
        // This is a draw
        return 0;
    }

    uint64_t mask;
    if (bitmap_count_ones(white) == 1) {
        if (black_major) {
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
            if (black_major & mask) score -= 100;

        }
        score -= (7 - dist(wkingsquare, bkingsquare)) * 30;
    } else if (bitmap_count_ones(black) == 1) {
        if (white_major) {
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
            if (white_major & mask) score += 100;
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
        if ((1ull << square) & whiteundefended)
            score -= 50;
        // doubled pawns are bad
        if ((AFILE << file)  & (white_pawns ^ (1ull << square)))
            score -= 20;
        
        // Encourage kings to come and protect these pawns
        int dist_from_wking = dist(square, wkingsquare);
        int dist_from_bking = dist(square, bkingsquare);
        score += (dist_from_bking - dist_from_wking) * 20;
        // passed pawns are good
        if (!((AFILE << square) & black_pawns)) {
            score += passed_pawn_table_endgame[rank];
            score += (dist_from_bking - dist_from_wking) * 40;
        }

        mask = 0;
        // isolated pawns are bad
        if (file != 0) mask |= (AFILE << (file - 1));
        if (file != 7) mask |= (AFILE << (file + 1));
        if (!(mask & white_pawns)) score -= 20;
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
        if ((1ull << square) & blackundefended)
            score += 50;
        if ((AFILE << file) & (black_pawns ^ (1ull << square)))
            score += 20;

        int dist_from_wking = dist(square, wkingsquare);
        int dist_from_bking = dist(square, bkingsquare);
        score += (dist_from_bking - dist_from_wking) * 20;

        if (!(((AFILE << file) >> (56 - 8 * rank)) & white_pawns)) {
            score -= passed_pawn_table_endgame[8-rank];
            score += (dist_from_bking - dist_from_wking) * 40;
        }

        mask = 0;
        if (file != 0) mask |= (AFILE << (file - 1));
        if (file != 7) mask |= (AFILE << (file + 1));
        if (!(mask & black_pawns)) score += 20;
    }
    if (!count) score += 120;

    bmloop(P2BM(board, WHITEKNIGHT), square, temp) {
        file = square & 0x7;
        whitematerial += 275;
        score += knight_table[56 - square + file + file];
        if ((1ull << square) & whiteundefended)
            score -= 150;
    }
    bmloop(P2BM(board, BLACKKNIGHT), square, temp) {
        blackmaterial += 275;
        score -= knight_table[square];
        if ((1ull << square) & blackundefended)
            score += 150;
    }

    count = 0;
    bmloop(P2BM(board, WHITEBISHOP), square, temp) {
        file = square & 0x7;
        whitematerial += 300;
        score += bishop_table[56 - square + file + file];
        count += 1;
        if ((1ull << square) & whiteundefended)
            score -= 150;
        // At least before end-game, central pawns on same
        // colored squares are bad for bishops
        if ((1ull << square) & BLACK_CENTRAL_SQUARES) {
            score -= bitmap_count_ones((white_pawns | black_pawns) & BLACK_CENTRAL_SQUARES) * 15;
        } else {
            score -= bitmap_count_ones((white_pawns | black_pawns) & WHITE_CENTRAL_SQUARES) * 15;
        }
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
        if ((1ull << square) & blackundefended)
            score += 150;
        if ((1ull << square) & BLACK_CENTRAL_SQUARES) {
            score += bitmap_count_ones((white_pawns | black_pawns) & BLACK_CENTRAL_SQUARES) * 15;
        } else {
            score += bitmap_count_ones((white_pawns | black_pawns) & WHITE_CENTRAL_SQUARES) * 15;
        }
    }

    score -= (count == 2) * 100;

    bmloop(P2BM(board, WHITEROOK), square, temp) {
        file = square & 0x7;
        whitematerial += 550;
        // Rooks on open files are great
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score += 20;
        // Rooks on semiopen files are good
        else if ((AFILE << file) & black_pawns)
            score += 10;

        // Doubled rooks are very powerful.
        // We add 80 (40 on this, 40 on other)
        if ((AFILE << file) & (white_major ^ (1ull << square)))
            score += 30;

        if ((1ull << square) & whiteundefended)
            score -= 320;
    }
    bmloop(P2BM(board, BLACKROOK), square, temp) {
        file = square & 0x7;
        blackmaterial += 550;
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score -= 20;
        else if ((AFILE << file) & white_pawns)
            score -= 10;

        if ((AFILE << file) & (black_major ^ (1ull << square)))
            score -= 30;

        if ((1ull << square) & blackundefended)
            score += 320;
    }
    bmloop(P2BM(board, WHITEQUEEN), square, temp) {
        file = square & 0x7;
        whitematerial += 880;
        // A queen counts as a rook
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score += 20;
        else if ((AFILE << file) & black_pawns)
            score += 10;

        if ((AFILE << file) & (white_major ^ (1ull << square)))
            score += 30;

        if ((1ull << square) & whiteundefended)
            score -= 620;
    }
    bmloop(P2BM(board, BLACKQUEEN), square, temp) {
        blackmaterial += 880;
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score -= 20;
        else if ((AFILE << file) & white_pawns)
            score -= 10;

        if ((AFILE << file) & (black_major ^ (1ull << square)))
            score -= 30;

        if ((1ull << square) & blackundefended)
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
    king_movements = white_king | attack_set_king(wkingsquare, white, black);
    count = bitmap_count_ones(king_movements & (~blackattack));
    if (who == 1 && mvs->check) {
        if (count <= 4) score -= (4 - count) * 60;
        if (count == 0) score -= 100;
        score -= 20;
    } else if (count <= 2 && (king_movements & (~blackattack))) {
        score -= (3 - count) *30;
    } else if (count == 0) score -= 20;

    file = bkingsquare & 0x7;
    score -= king_table_endgame[bkingsquare];

    king_movements = black_king | attack_set_king(bkingsquare, black, white);
    count = bitmap_count_ones(king_movements & (~whiteattack));
    if (who == -1 && mvs->check) {
        if (count <= 4) score += (4 - count) * 60;
        if (count == 0) score += 100;
        score += 20;
    } else if (count <= 2 && (king_movements & (~whiteattack))) {
        score += (3 - count) *30;
    } else if (count == 0) score += 20;

    // the side with more options is better
    score += (bitmap_count_ones(whiteattack) - bitmap_count_ones(blackattack)) * 8;

    return score;
}

