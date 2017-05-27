#include "board.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pieces.h"
#include "moves.h"
#include "pawns.h"
#include "evaluation_parameters.h"
#include "endgame.h"

#define AFILE 0x0101010101010101ull
#define HFILE 0x8080808080808080ull
#define RANK1 0x00000000000000ffull
#define RANK2 0x000000000000ff00ull
#define RANK3 0x0000000000ff0000ull
#define RANK6 0x0000ff0000000000ull
#define RANK7 0x00ff000000000000ull
#define RANK8 0xff00000000000000ull

#define PINNED

struct evaluation_hash_entry {
    uint32_t hash;
    int32_t score;
};

#define EVALUATION_HASH_SIZE (1024 * 256)
struct evaluation_hash_entry evaluation_hash[EVALUATION_HASH_SIZE];

int evaluation_cache_calls;
int evaluation_cache_hits;

int read_evaluation_cache(struct board* board, int* val) {
    int loc = board->hash & (EVALUATION_HASH_SIZE - 1);
    int sig = board->hash >> 32;
    evaluation_cache_calls++;
    if (evaluation_hash[loc].hash == sig) {
        *val = evaluation_hash[loc].score;
        evaluation_cache_hits++;
        return 0;
    }
    return 1;
}

void store_evaluation_cache(struct board* board, int val) {
    int loc = board->hash & (EVALUATION_HASH_SIZE - 1);
    int sig = board->hash >> 32;
    evaluation_hash[loc].hash = sig;
    evaluation_hash[loc].score = val;
}


static int board_score_middlegame(struct board* board, unsigned char who, struct deltaset* mvs, int alpha, int beta);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Evaluation CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Evaluates relative score of white and black. Pawn = 1. who = whose turn. Positive is good for white.
// is_in_check = if current side is in check, nmoves = number of moves

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
    int score;
    if (read_evaluation_cache(board, &score) == 0) {
        return score;
    }

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
        score = board_score_middlegame(board, who, mvs, alpha, beta);
    } else if (phase <= 3) {
        score = board_score_endgame(board, who, mvs);
    } else {
        int score_mg = board_score_middlegame(board, who, mvs, alpha, beta);
        int score_eg = board_score_endgame(board, who, mvs);

        score = (phase * score_mg  + (8 - phase) * score_eg) / 8;
    }
    store_evaluation_cache(board, score);
    return score;
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
            int loc = (w == 0) ? 63 - square : square;
            int oldsubscore = subscore;
            subscore += knight_table[loc];
            SQPRINTF("Sided knight position score for %c%c: %d\n", square, knight_table[loc]);
            // Outposts are good
            if ((1ull << square) & outposts[w]) {
                subscore += 16;
                SQPRINTF("Sided knight outpost score for %c%c: %d\n", square, 31);
            } else if ((1ull << square) & holes[w]) {
                subscore += 8;
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
                subscore -= 15;
                SQPRINTF("Sided knight pinned penalty for %c%c: %d\n", square, -15);
            }
#endif
            SQPRINTF("Total value of knight on %c%c: %d\n", square, subscore - oldsubscore);
        }

        count = 0;
        bmloop(P2BM(board, 6 * w + BISHOP), square, temp) {
            int loc = (w == 0) ? 63 - square : square;
            int oldsubscore = subscore;
            subscore += bishop_table[loc];
            SQPRINTF("Sided bishop score for %c%c: %d\n", square, bishop_table[loc]);
            count += 1;
            if ((1ull << square) & outposts[w]) {
                SQPRINTF("Sided bishop outpost for %c%c: %d\n", square, 14);
                subscore += 12;
            }
            else if ((1ull << square) & holes[w]) {
                SQPRINTF("Sided bishop outpost for %c%c: %d\n", square, 4);
                subscore += 6;
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
                SQPRINTF("Sided bishop pinned for %c%c: %d\n", square, -10);
                subscore -= 10;
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
            int loc = (w == 0) ? 63 - square : square;
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
                SQPRINTF("Sided rook pinned penalty for %c%c: %d\n", square, -10);
                subscore -= 10;
            }
#endif
            SQPRINTF("Total value of rook on %c%c: %d\n", square, subscore - oldsubscore);
        }


        bmloop(P2BM(board, 6 * w + QUEEN), square, temp) {
            file = square & 0x7;
            int loc = (w == 0) ? 63 - square : square;
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
                SQPRINTF("Queen hanging piece penalty for %c%c: %d\n", square, -30);
                subscore -= 30;
            }
#endif
            SQPRINTF("Total value of queen on %c%c: %d\n", square, subscore - oldsubscore);
        }

        square = board->kingsq[w];
        file = square & 0x7;
        int loc = (w == 0) ? 63 - square : square;
        subscore += king_table[loc];
        SQPRINTF("King score for %c%c: %d\n", square, king_table[loc]);

        uint64_t opponent_king_mask;
        int opponent_king_file = board->kingsq[1 - w] % 8;
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
