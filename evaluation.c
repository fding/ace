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
static int board_score_middlegame(struct board* board, unsigned char who, struct deltaset* mvs, int alpha, int beta);

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
    -13,  5,  -5,  -5,  -5,  -5,  5, -13,
    -5,  10,  8,  5,  5,  8,  10, -5,
     0,  15,  13,  10,  10,  13,  15, 0,
     2,  15,  13,  10,  10,  13,  15, 2,
    -10,  5,  0,  -2,  -2,  0,  5, -10,
    -5,  10,  8,  5,  5,  8,  10, -5,
    -15, -10, -12, -14, -14, -12, -10, -15,
};

int rook_table[64] = {
    -10, -5, 0, 5, 5, 0, -5, -10,
    -10, -1, 4, 10, 10, 4, -1, -10,
    -10, -1, 4, 10, 10, 4, -1, -10,
    -10, -2, 2, 8, 8, 3, -2, -10,
    -10, -3, 2, 7, 7, 2, -3, -10,
    -10, -4, 1, 6, 6, 1, -4, -10,
    -10, -5, 0, 5, 5, 0, -5, -10,
    -10, -5, 0, 5, 5, 0, -5, -10,
};

int queen_table[64] = {
    -15, -10, -5, 5, 5, -5, -10, -15,
    -5,  10,  8,  9,  9,  8,  10, -5,
    -5,  10,  8,  7,  7,  8,  10, -5,
     0,  15,  13,  14,  14,  13,  15, 0,
     2,  15,  13,  14,  14,  13,  15, 2,
    -10,  5,  2,  7,  7,  2,  5, -10,
    -5,  10,  8,  9,  9,  8,  10, -5,
    -15, -10, -5, 5, 5, -5, -10, -15,
};

int king_table[64] = {
    -17, -16, -20, -29, -29, -20, -16, -17,
    -17, -16, -20, -29, -29, -20, -16, -17,
    -17, -16, -20, -29, -29, -20, -16, -17,
    -15, -13, -17, -26, -26, -17, -13, -15,
    -5, -5, -10, -20, -20, -10, -5, -5,
    -2, -3, -7, -18, -18, -7, -3, -2,
    0, -2, -5, -15, -15, -5, -2, 0,
    5, 6, 5, 0, 0, 5, 6, 5,

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
    0,  0,   4,   6,   9,   13,   18,  22,  29,  36,
  43,  50,  58,  68,  78,  88,  99,  110,  132, 144,
  156,  169,  182,  196,  210, 230, 250, 270, 300, 350,
  450, 500, 500, 500, 500, 500, 500, 500, 500, 500,
  500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
  500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
  500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500
};

int attack_count_table[40] = {
    -5, -2, -1, 0, 1, 2, 2, 3, 5, 8, 11, 14, 17, 20, 22, 22, 22, 22, 22, 22,
    22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,
};

int attack_count_table_bishop[20] = {
    -10, -7, -6, -5, -4, 0, 2, 5, 9, 15, 18, 20, 21, 21, 21, 21, 21, 21, 21, 21
};

int attack_count_table_knight[9] = {-5, -2, -1, 1, 4, 7, 12, 13, 14};
int bishop_obstruction_table[9] = {5, 4, 0, -5, -13, -20, -25, -25, -25};
int bishop_own_obstruction_table[9] = {5, -1, -10, -20, -25, -25, -25, -25, -25};

int pawn_shield_table[100] = {
    0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 5, 5, 5, 5, 5, 5, 5, 5,
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

#define BLACK_CENTRAL_SQUARES 0x0000281428140000ull
#define BLACK_SQUARES         0xaa55aa55aa55aa55ull
#define WHITE_CENTRAL_SQUARES 0x0000142814280000ull
#define WHITE_SQUARES         0x55aa55aa55aa55aaull

int simplified_material_for_player(struct board* board, side_t who) {
    return popcnt(board->pieces[who][PAWN]) +
        3 * popcnt(board->pieces[who][KNIGHT]) +
        3 * popcnt(board->pieces[who][BISHOP]) +
        5 * popcnt(board->pieces[who][ROOK]) +
        9 * popcnt(board->pieces[who][QUEEN]);
}

int material_for_player(struct board* board, side_t who) {
    return 100 * popcnt(board->pieces[who][PAWN]) +
        300 * popcnt(board->pieces[who][KNIGHT]) +
        315 * popcnt(board->pieces[who][BISHOP]) +
        490 * popcnt(board->pieces[who][ROOK]) +
        880 * popcnt(board->pieces[who][QUEEN]);
}

int material_for_player_endgame(struct board* board, side_t who) {
    return 135 * popcnt(board->pieces[who][PAWN]) +
        275 * popcnt(board->pieces[who][KNIGHT]) +
        300 * popcnt(board->pieces[who][BISHOP]) +
        550 * popcnt(board->pieces[who][ROOK]) +
        900 * popcnt(board->pieces[who][QUEEN]);
}

/* Scoring the board:
 * We score the board in units of centipawns, taking the following
 * into consideration:
 *  1. Material
 *  2. Piece location (differs for king between endgame and midgame)
 *  3. Mobility
 *  4. Presence of bishop pair (half a pawn)
 *  5. Pawn structure
 *      a. passed pawn (bonus)
 *      b. isolated pawn (penalty)
 *      c. doubled pawns (penalty)
 *      d. backward pawns (penalty)
 *      e. connected pawns (bonus)
 *  6. Doubled rooks (bonus)
 *  7. Rooks on open files (bonus)
 *  8. Rooks on semiopen files (bonus)
 *  9. Central pawns on same color for bishop (penalty)
 *  10. Number of available attacks, disregarding king pins (bonus)
 *  11. Castling rights (penalty if you can't castle)
 *  12. King safety
 *      a. Open file (penalty)
 *      b. Lack of pawn shield for castled king (penalty)
 *      c. Pawn storm (bonus for attacker)
 *      d. enemy pieces attacking king zone (penalty)
 *
 * TODO list:
 *  1. Endgame table
 *  2. Finer material nuances, like material hash table
 */
int board_score(struct board* board, unsigned char who, struct deltaset* mvs, int alpha, int beta) {
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

    int phase = MIN(8, popcnt(board_occupancy(board, 0) ^ P2BM(board, WHITEPAWN)) +
            popcnt(board_occupancy(board, 1) ^ P2BM(board, BLACKPAWN)) - 2);

    if (phase >= 8) {
        return board_score_middlegame(board, who, mvs, alpha, beta);
    }

    if (phase <= 3) {
        return board_score_endgame(board, who, mvs);
    }

    int score_mg = board_score_middlegame(board, who, mvs, alpha, beta);
    int score_eg = board_score_endgame(board, who, mvs);

    return (phase * score_mg  + (8 - phase) * score_eg) / 8;
}

static int board_score_middlegame(struct board* board, unsigned char who, struct deltaset* mvs, int alpha, int beta) {
    DPRINTF("Scoring board\n");
    int score = 0;
    uint64_t temp;
    int square, count;
    int rank, file;

    int whitematerial, blackmaterial;
    whitematerial = material_for_player(board, 0);
    blackmaterial = material_for_player(board, 1);
    score = whitematerial - blackmaterial;

    DPRINTF("Material score: %d\n", score);

    // Positional scores don't have THAT huge of an effect, so if the situation is already hopeless, give up
    if (score + 200 < alpha) {
        return score;
    }
    if (score > beta + 200)
        return score;

    uint64_t pawns[2], bishops[2], rooks[2], queens[2],
             minors[2], majors[2], pieces[2], kings[2], outposts[2], holes[2];

    pawns[0] = P2BM(board, WHITEPAWN);
    pawns[1] = P2BM(board, BLACKPAWN);
    bishops[0] = P2BM(board, WHITEBISHOP);
    bishops[1] = P2BM(board, BLACKBISHOP);
    rooks[0] = P2BM(board, WHITEROOK);
    rooks[1] = P2BM(board, BLACKROOK);
    queens[0] = P2BM(board, WHITEQUEEN);
    queens[1] = P2BM(board, BLACKQUEEN);

    minors[0] = P2BM(board, WHITEKNIGHT) | P2BM(board, WHITEBISHOP);
    minors[1] = P2BM(board, BLACKKNIGHT) | P2BM(board, BLACKBISHOP);
    majors[0] = P2BM(board, WHITEROOK) | P2BM(board, WHITEQUEEN);
    majors[1] = P2BM(board, BLACKROOK) | P2BM(board, BLACKQUEEN);
    kings[0] = P2BM(board, WHITEKING);
    kings[1] = P2BM(board, BLACKKING);


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
    DPRINTF("Pawn score: %d\n", pstruct->score);

    // Outposts are squares attacked by your own pawns but not by opponents
    outposts[0] = pstruct->holes[1] & pawn_attacks[0] & 0x00ffffff00000000ull;
    outposts[1] = pstruct->holes[0] & pawn_attacks[1] & 0x00000000ffffff00ull;
    holes[0] = pstruct->holes[1] & 0x00ffffff00000000ull;
    holes[1] = pstruct->holes[0] & 0x00000000ffffff00ull;

    for (int w = 0; w < 2; w++) {
        int subscore = 0;
        int file_occupied[8];
        memset(file_occupied, 0, 8 * sizeof(int));

        int king_attackers = 0;
        uint64_t king_zone = kings[1 - w];
        king_zone |= (kings[1 - w] & (~HFILE)) << 1;
        king_zone |= (kings[1 - w] & (~AFILE)) >> 1;
        uint64_t king_row = king_zone;
        king_zone |= (king_row & ~(RANK1)) >> 8;
        king_zone |= (king_row & ~(RANK8)) << 8;
        if (1 - w) {
            king_zone |= (king_zone & (~RANK1)) >> 8;
            // king_zone |= (king_zone & (~RANK1)) >> 8;
        } else {
            king_zone |= (king_zone & (~RANK8)) << 8;
            // king_zone |= (king_zone & (~RANK8)) << 8;
        }

        bmloop(P2BM(board, 6 * w + KNIGHT), square, temp) {
            file = square & 0x7;
            int loc = (w == 0) ? (56 - square + file + file) : square;
            int oldsubscore = subscore;
            subscore += knight_table[loc];
            SQPRINTF("Sided knight position score for %c%c: %d\n", square, knight_table[loc]);
            // Outposts are good
            if ((1ull << square) & outposts[w]) {
                subscore += 31;
                SQPRINTF("Sided knight outpost score for %c%c: %d\n", square, 31);
            } else if ((1ull << square) & holes[w]) {
                subscore += 21;
                SQPRINTF("Sided knight outpost score for %c%c: %d\n", square, 21);
            }
            uint64_t attack_mask = attack_set_knight(square, pieces[w], pieces[1 - w]);
            subscore += attack_count_table_knight[popcnt(attack_mask)];
            SQPRINTF("Sided knight attack score for %c%c: %d\n", square, attack_count_table_knight[popcnt(attack_mask)]);
            if (attack_mask & king_zone) {
                king_attackers += 2;
                SQPRINTF("Sided knight king attacker for %c%c: %d\n", square, 2);
            }

            // Hanging piece penalty
            if (!(attacks[w] & (1ull << square))) {
                subscore -= 2;
                SQPRINTF("Sided knight hanging piece penalty for %c%c: %d\n", square, -2);
            }
            if ((1ull << square) & undefended[w]) {
                subscore -= 5;
                SQPRINTF("Sided knight undefended piece penalty for %c%c: %d\n", square, -5);
            }
            if ((1ull << square) & pawn_attacks[1 - w]) {
                subscore -= 5;
                SQPRINTF("Sided knight pawn penalty for %c%c: %d\n", square, -5);
            }

#ifdef PINNED
            // Pinned pieces aren't great, especially knights
            if ((1ull << square) & mvs->pinned) {
                subscore -= 30;
                SQPRINTF("Sided knight pinned penalty for %c%c: %d\n", square, -30);
            }
#endif
            SQPRINTF("Total value of knight on %c%c: %d\n", square, subscore - oldsubscore);
        }

        count = 0;
        bmloop(P2BM(board, 6 * w + BISHOP), square, temp) {
            file = square & 0x7;
            int loc = (w == 0) ? (56 - square + file + file) : square;
            int oldsubscore = subscore;
            subscore += bishop_table[loc];
            SQPRINTF("Sided bishop score for %c%c: %d\n", square, bishop_table[loc]);
            count += 1;
            if ((1ull << square) & outposts[w]) {
                SQPRINTF("Sided bishop outpost for %c%c: %d\n", square, 14);
                subscore += 14;
            }
            else if ((1ull << square) & holes[w]) {
                SQPRINTF("Sided bishop outpost for %c%c: %d\n", square, 4);
                subscore += 4;
            }
            uint64_t attack_mask = attack_set_bishop(square, pieces[w], pieces[1 - w]);
            uint64_t attack_mask_only_pawn = attack_set_bishop(square, pawns[w], pieces[1 - w]);
            subscore += attack_count_table_bishop[popcnt(attack_mask)];
            SQPRINTF("Sided bishop attack count for %c%c: %d\n", square, attack_count_table_bishop[popcnt(attack_mask)]);
            if (attack_mask & king_zone) {
                king_attackers += 2;
                SQPRINTF("Sided bishop king attacker for %c%c: %d\n", square, 2);
            } else if (attack_mask_only_pawn & king_zone) {
                king_attackers += 1;
                SQPRINTF("Sided bishop king attacker for %c%c: %d\n", square, 1);
            }

            // Hanging piece penalty
            if (!(attacks[w] & (1ull << square))) {
                subscore -= 2;
                SQPRINTF("Sided bishop hanging penalty for %c%c: %d\n", square, -2);
            }
            if ((1ull << square) & undefended[w]) {
                subscore -= 5;
                SQPRINTF("Sided bishop hanging penalty for %c%c: %d\n", square, -5);
            }
            if ((1ull << square) & pawn_attacks[1 - w]) {
                subscore -= 5;
                SQPRINTF("Sided bishop hanging penalty for %c%c: %d\n", square, -5);
            }

            // At least before end-game, central pawns on same
            // colored squares are bad for bishops
            if ((1ull << square) & BLACK_SQUARES) {
                int penalty = 0;
                penalty += bishop_obstruction_table[popcnt(pawns[1-w] & BLACK_CENTRAL_SQUARES)];
                penalty += bishop_own_obstruction_table[popcnt(pawns[w] & BLACK_CENTRAL_SQUARES)];
                subscore += penalty;
                SQPRINTF("Sided bishop obstruction penalty for %c%c: %d\n", square, penalty);
            } else {
                int penalty = 0;
                penalty += bishop_obstruction_table[popcnt(pawns[1-w] & WHITE_CENTRAL_SQUARES)];
                penalty += bishop_own_obstruction_table[popcnt(pawns[w] & WHITE_CENTRAL_SQUARES)];
                subscore += penalty;
                SQPRINTF("Sided bishop obstruction penalty for %c%c: %d\n", square, penalty);
            }
#ifdef PINNED
            if ((1ull << square) & mvs->pinned) {
                SQPRINTF("Sided bishop pinned for %c%c: %d\n", square, -15);
                subscore -= 15;
            }
#endif
            SQPRINTF("Total value of bishop on %c%c: %d\n", square, subscore - oldsubscore);
        }
        // Bishop pairs are very valuable
        // In the endgame, 2 bishops can checkmate a king,
        // whereas 2 knights can't
        subscore += (count == 2) * 46;
        DPRINTF("Sided double bishop: %d\n", (count==2) * 46);

        bmloop(P2BM(board, 6 * w + ROOK), square, temp) {
            file = square & 0x7;
            rank = square / 8;
            int loc = (w == 0) ? (56 - square + file + file) : square;
            int oldsubscore = subscore;
            subscore += rook_table[loc];
            SQPRINTF("Sided rook for %c%c: %d\n", square, rook_table[loc]);
            if ((w && rank == 1) || (!w && rank == 6)) {
                // TODO: only do this if king is on rank 8
                // or if there are pawns on rank 7
                SQPRINTF("Sided rook on seventh rank for %c%c: %d\n", square, 20);
                subscore += 20;
            }
            if ((1ull << square) & outposts[w]) subscore += 15;
            // Rooks on open files are great
            if (!file_occupied[file]) {
                if (!((AFILE << file) & (pawns[0] | pawns[1]))) {
                    SQPRINTF("Sided rook on open file for %c%c: %d\n", square, 10);
                    subscore += 10;
                // Rooks on semiopen files are good
                } else if (!((AFILE << file) & pawns[w])) {
                    SQPRINTF("Sided rook on open file for %c%c: %d\n", square, 5);
                    subscore += 5;
                }
            }
            else {
                SQPRINTF("Sided rook on blocked file for %c%c: %d\n", square, -5);
                subscore -= 5;
            }
            file_occupied[file] = 1;

            uint64_t attack_mask = attack_set_rook(square, pieces[w], pieces[1 - w]);
            uint64_t attack_mask_only_pawn = attack_set_rook(square, pawns[w], pieces[1 - w]);
            subscore += attack_count_table[popcnt(attack_mask)];
            SQPRINTF("Sided rook attack count for %c%c: %d\n", square, attack_count_table[popcnt(attack_mask)]);
            if (attack_mask & king_zone) {
                SQPRINTF("Sided rook king attacker for %c%c: %d\n", square, 3);
                king_attackers += 3;
            } else if (attack_mask_only_pawn & king_zone) {
                SQPRINTF("Sided rook king attacker for %c%c: %d\n", square, 2);
                king_attackers += 2;
            }

            // Hanging piece penalty
            if (!(attacks[w] & (1ull << square))) {
                SQPRINTF("Sided rook hanging penalty for %c%c: %d\n", square, -2);
                subscore -= 2;
            } if ((1ull << square) & undefended[w]) {
                SQPRINTF("Sided rook hanging penalty for %c%c: %d\n", square, -5);
                subscore -= 5;
            }
            if ((1ull << square) & pawn_attacks[1 - w]) {
                SQPRINTF("Sided rook hanging penalty for %c%c: %d\n", square, -5);
                subscore -= 5;
            }

#ifdef PINNED
            if ((1ull << square) & mvs->pinned) {
                SQPRINTF("Sided rook pinned penalty for %c%c: %d\n", square, -5);
                subscore -= 50;
            }
#endif
            SQPRINTF("Total value of rook on %c%c: %d\n", square, subscore - oldsubscore);
        }


        bmloop(P2BM(board, 6 * w + QUEEN), square, temp) {
            file = square & 0x7;
            int loc = (w == 0) ? (56 - square + file + file) : square;
            int oldsubscore = subscore;
            subscore += queen_table[loc];
            SQPRINTF("Queen score for %c%c: %d\n", square, queen_table[loc]);
            // A queen counts as a rook
            if (!file_occupied[file]) {
                if (!((AFILE << file) & (pawns[0] | pawns[1]))) {
                    SQPRINTF("Queen open file for %c%c: %d\n", square, 15);
                    subscore += 15;
                } else if (!((AFILE << file) & pawns[w])) {
                    SQPRINTF("Queen semiopen file for %c%c: %d\n", square, 5);
                    subscore += 5;
                }
            } else {
                SQPRINTF("Queen blocked file for %c%c: %d\n", square, -5);
                subscore -= 5;
            }
            file_occupied[file] = 1;

            uint64_t attack_mask = attack_set_queen(square, pieces[w], pieces[1 - w]);
            uint64_t attack_mask_only_pawn = attack_set_queen(square, pawns[w], pieces[1 - w]);
            subscore += attack_count_table[popcnt(attack_mask)];
            SQPRINTF("Queen attack score for %c%c: %d\n", square, attack_count_table[popcnt(attack_mask)]);
            if (attack_mask & king_zone) {
                SQPRINTF("Queen king attack for %c%c: %d\n", square, 5);
                king_attackers += 5;
            } else if (attack_mask_only_pawn & king_zone) {
                SQPRINTF("Queen king attack for %c%c: %d\n", square, 4);
                king_attackers += 4;
            }

            // Hanging piece penalty
            if (!(attacks[w] & (1ull << square))) {
                SQPRINTF("Queen hanging piece penalty for %c%c: %d\n", square, -2);
                subscore -= 2;
            } if ((1ull << square) & undefended[w]) {
                SQPRINTF("Queen hanging piece penalty for %c%c: %d\n", square, -5);
                subscore -= 5;
            }
            if ((1ull << square) & pawn_attacks[1 - w]) {
                SQPRINTF("Queen hanging piece penalty for %c%c: %d\n", square, -5);
                subscore -= 5;
            }

#ifdef PINNED
            if ((1ull << square) & mvs->pinned) {
                SQPRINTF("Queen hanging piece penalty for %c%c: %d\n", square, -500);
                subscore -= 500;
            }
#endif
            SQPRINTF("Total value of queen on %c%c: %d\n", square, subscore - oldsubscore);
        }

        square = LSBINDEX(kings[w]);
        file = square & 0x7;
        int loc = (w == 0) ? (56 - square + file + file) : square;
        subscore += king_table[loc];
        SQPRINTF("King score for %c%c: %d\n", square, king_table[loc]);

        uint64_t opponent_king_mask;
        int opponent_king_file = LSBINDEX(kings[1 - w]) % 8;
        if (opponent_king_file <= 2)
            opponent_king_mask = AFILE | (AFILE << 1) | (AFILE << 2);
        if (opponent_king_file >= 5)
            opponent_king_mask = HFILE | (HFILE >> 1) | (HFILE >> 2);

        mask = (AFILE << file);
        if (file != 0)
            mask |= AFILE << (file - 1);
        if (file != 7)
            mask |= AFILE << (file + 1);

        // open files are bad news for the king
        subscore -= 5 * (3 - popcnt(pawns[w] & mask));
        SQPRINTF("King open file penalty for %c%c: %d\n", square, -5 * (3 - popcnt(pawns[w] & mask)));
        subscore -= 6 * (3 - popcnt(pawns[1-w] & mask));
        SQPRINTF("King open file penalty for %c%c: %d\n", square, -6 * (3 - popcnt(pawns[1-w] & mask)));

        if (file <= 3)
            mask = AFILE | (AFILE << 1) | (AFILE << 2);
        else if (file >= 4)
            mask = HFILE | (HFILE >> 1) | (HFILE >> 2);

        uint64_t mask1, mask2, mask3, mask4, mask5;
        if (w) {
            mask1 = mask << 40; // rank 2 and 3
            DPRINTF("Mask: %lx\n", mask1);
            mask2 = opponent_king_mask >> 24; // rank 4, 5, 6, 7
            mask3 = opponent_king_mask >> 32;
            mask4 = opponent_king_mask >> 40;
            mask5 = mask1 << 8; // rank 1 and 2
        } else {
            mask1 = mask >> 40;
            DPRINTF("Mask: %lx\n", mask1);
            mask2 = opponent_king_mask << 24;
            mask3 = opponent_king_mask << 32;
            mask4 = opponent_king_mask << 40;
            mask5 = mask1 >> 8;
        }
        // we want a pawn shield
        if (mask5 & kings[w]) {
            subscore += pawn_shield_table[2 * popcnt(bishops[1-w]) + 3 * popcnt(rooks[1-w]) + 5 * popcnt(queens[1-w])]
                * MIN(3, popcnt(mask1 & pawns[w]));
            SQPRINTF("King pawn_shield score for %c%c: %d\n", square,
                     pawn_shield_table[2 * popcnt(bishops[1-w]) + 3 * popcnt(rooks[1-w]) + 5 * popcnt(queens[1-w])]
                                * MIN(3, popcnt(mask1 & pawns[w])));
        }

        if (mask3 & pawns[1-w]) {
            // we also want a pawn storm
            subscore += pawn_storm_table[king_attackers] * popcnt(mask2 & pawns[w]);
            DPRINTF("Pawn storm score for side %d: %d\n", w, pawn_storm_table[king_attackers] * popcnt(mask2 & pawns[w]));
        }

        subscore += king_attacker_table[king_attackers];
        DPRINTF("Total king attackers for side %d: %d (score=%d)\n", w, king_attackers, king_attacker_table[king_attackers]);

        if (w) score -= subscore;
        else score += subscore;
    }

    score += ((board->cancastle & 12) != 0) * 10 - ((board->cancastle & 3) != 0 ) * 10;
    DPRINTF("Can castle score: %d\n", ((board->cancastle & 12) != 0) * 10 - ((board->cancastle & 3) != 0 ) * 10);
    return score;
}

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


int distance_to_score[9] = {21, 21, 18, 15, 12, 9, 6, 3, 0};

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
void initialize_endgame_tables() {
    // -1 is our junk value
    for (int i = 0; i < 6561; i++) {
        insufficient_material_table[i] = ENDGAME_TABLE_JUNK;
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
    int rank = 0, file = 0;

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
    if (popcnt(pieces[0]) == 1 && popcnt(pieces[1]) == 1) {
        return 0;
    }

    // No pawns
    if (popcnt(pawns[0]) == 0 && popcnt(pawns[1]) == 0) {
        /*
        int nknights[2], nbishops[2], nrooks[2], nqueens[2];
        for (int i = 0; i < 2; i++) {
            nknights[i] = popcnt(board->pieces[i][KNIGHT]);
            nbishops[i] = popcnt(board->pieces[i][BISHOP]);
            nrooks[i] = popcnt(board->pieces[i][ROOK]);
            nqueens[i] = popcnt(board->pieces[i][QUEEN]);
        }
        */

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
        /*
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
        */
    }

    // Pawn + king vs king
    if (popcnt(majors[0]) == 0 && popcnt(majors[1]) == 0 &&
            popcnt(minors[0]) == 0 && popcnt(minors[1]) == 0) {
        endgame_type = EG_KPKP;
        if (popcnt(pawns[0]) > 1 && popcnt(pawns[1]) == 0)
            score += 400;
        if (popcnt(pawns[1]) > 1 && popcnt(pawns[0]) == 0)
            score -= 400;
        if (popcnt(pawns[0]) == 1 && popcnt(pawns[1]) == 0) {
            int psquare = LSBINDEX(pawns[0]);
            int prank = psquare / 8;
            int pfile = psquare % 8;
            int qsquare = 56 + pfile;
            if (dist(qsquare, bkingsquare) + (board->who == 1) < (7 - prank))
                score += 400;
        }
        if (popcnt(pawns[1]) == 1 && popcnt(pawns[0]) == 0) {
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
    score += whitematerial - blackmaterial;
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
    score += (count == 2) * 60;

    count = 0;
    bmloop(P2BM(board, BLACKBISHOP), square, temp) {
        score -= bishop_table[square];
        count += 1;
    }
    score -= (count == 2) * 60;

    bmloop(P2BM(board, WHITEROOK), square, temp) {
        file = square & 0x7;

        // Doubled rooks are very powerful.
        if ((AFILE << file) & (majors[0] ^ (1ull << square)))
            score += 5;
    }
    bmloop(P2BM(board, BLACKROOK), square, temp) {
        file = square & 0x7;

        if ((AFILE << file) & (majors[1] ^ (1ull << square)))
            score -= 5;
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

    file = wkingsquare & 0x7;

    score += king_table_endgame[56 - wkingsquare + file + file];
    score -= king_table_endgame[bkingsquare];

    return score;
}
