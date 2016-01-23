#include "pawns.h"
#include "board.h"
#include "pieces.h"
#include "moves.h"
#include <stdlib.h>

struct pawn_structure pawn_hashmap[8192];

uint64_t hash(uint64_t wpawns, uint64_t bpawns) {
    return ((wpawns ^ bpawns) * 0x2480041000800801ull) & 0xfff;
}

int pawn_table[64] = {
    800, 800, 800, 800, 800, 800, 800, 800,
    5, 10, 10, 20, 20, 10, 10, 5,
    5, 10, 10, 20, 20, 10, 10, 5,
    5, 10, 10, 20, 20, 10, 10, 5,
    0, 0, 5, 10, 10, 5, 0, 0,
    -2, -2, 0, 5, 5, 0, -2, -2,
    -2, -2, -2, -2, -2, -2, -2, -2,
    0, 0, 0, 0, 0, 0, 0, 0, 
};

int passed_pawn_table[8] = {0, 0, 0, 10, 15, 20, 50, 800};
int isolated_pawn_penalty[8] = {20, 25, 28, 32, 32, 28, 25, 20};
int doubled_pawn_penalty[8] = {10, 12, 15, 18, 18, 15, 12, 10};
int totcalls;
int hits;

int pawn_table_endgame[64] = {
    800, 800, 800, 800, 800, 800, 800, 800,
    120, 200, 300, 300, 300, 300, 200, 120, 
    70, 80, 85, 90, 90, 85, 80, 70,
    10, 30, 35, 50, 50, 35, 30, 10,
    10, 20, 20, 40, 40, 20, 20, 10,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 
};
int passed_pawn_table_endgame[8] = {0, 20, 30, 40, 77, 154, 256, 800};
int isolated_pawn_penalty_endgame[8] = {20, 25, 25, 25, 25, 25, 25, 20};
int doubled_pawn_penalty_endgame[8] = {20, 25, 25, 30, 30, 25, 25, 20};


struct pawn_structure * evaluate_pawns(struct board* board) {
    struct pawn_structure * stored;
    stored = &pawn_hashmap[board->pawn_hash & 0x1fff];
    if (stored->pawn_hash == board->pawn_hash) {
        return stored;
    }

    int square, rank, file;
    uint64_t temp;
    uint64_t mask;
    int score = 0;
    int score_eg = 0;
    int count = 0;
    stored->passed_pawns[0] = 0;
    stored->passed_pawns[1] = 0;

    uint64_t advance_mask[2];
    advance_mask[0] = 0ull;
    advance_mask[1] = 0ull;
    uint64_t all_pawns = board->pieces[0][PAWN] | board->pieces[1][PAWN];

    for (int who = 0; who < 2; who++) {
        int subscore = 0;
        int subscore_eg = 0;
        stored->holes[who] = 0ull;
        bmloop(P2BM(board, 6 * who + PAWN), square, temp) {
            advance_mask[who] |= (1ull << square);
            int tsq = square + 8 * (1 - 2*who);
            while (0 <= tsq && tsq < 64 && !(all_pawns & (1ull << tsq))) {
                advance_mask[who] |= (1ull << tsq);
                tsq = tsq + 8 * (1 - 2*who);
            }
            count += 1;
            rank = square / 8;
            file = square & 0x7;
            int loc = (who == 0) ? (56 - square + file + file) : square;
            subscore += pawn_table[loc];
            subscore_eg += pawn_table_endgame[loc];

            uint64_t ahead = (AFILE << file);
            uint64_t behind = (AFILE << file);
            if (who) {
                ahead = ahead >> (8 * (7 - rank));
                behind = behind << (8 * rank);
            }
            else {
                ahead = ahead << (8 * rank);
                behind = behind >> (8 * (7 - rank));
            }

            ahead ^= (1ull << square);
            behind ^= (1ull << square);
            uint64_t passed_pawn_mask = ahead;
            uint64_t backward_mask = 0;
            if (file != 0) {
                passed_pawn_mask |= (ahead << 1);
                backward_mask |= (behind << 1);
            }
            if (file != 7) {
                passed_pawn_mask |= (ahead >> 1);
                backward_mask |= (behind >> 1);
            }

            int rank7pawn = (((who == 0) && (rank == 6)) || ((who == 1) && (rank == 1)));
            int penalized = 0;

            // backward pawns are bad
            if (!(backward_mask & board->pieces[who][PAWN]) && !rank7pawn) {
                uint64_t sq1ahead, sq2ahead, blockers, attackers;
                attackers = 0;
                blockers = 0;
                if (who) {
                    sq1ahead = (1ull << (square - 8));
                    sq2ahead = (1ull << (square - 16));
                } else {
                    sq1ahead = (1ull << (square + 8));
                    sq2ahead = (1ull << (square + 16));
                }
                if (file != 0) {
                    attackers |= (sq1ahead << 1);
                    blockers |= (sq2ahead << 1);
                }
                if (file != 7) {
                    attackers |= (sq1ahead >> 1);
                    blockers |= (sq2ahead >> 1);
                }
                if (!(attackers & board->pieces[1-who][PAWN]) && (blockers & board->pieces[1-who][PAWN])) {
                    penalized = 1;
                    subscore -= 30;
                    subscore_eg -= 30;
                }
            } else {
                // Supported pawns are good
                uint64_t sq1behind, supporters;
                supporters = 0;
                if (who) {
                    sq1behind = (1ull << (square + 8));
                } else {
                    sq1behind = (1ull << (square - 8));
                }
                if (file != 0) {
                    supporters |= (sq1behind << 1);
                }
                if (file != 7) {
                    supporters |= (sq1behind >> 1);
                }
                if (supporters & board->pieces[who][PAWN]) {
                    subscore += 15;
                    subscore_eg += 15;
                }
            }

            // doubled pawns are bad
            if (!penalized && (ahead & (board->pieces[who][PAWN]))) {
                int other = LSBINDEX(ahead & board->pieces[who][PAWN]);
                subscore -= doubled_pawn_penalty[file] / abs(other/8 - rank);
                subscore_eg -= doubled_pawn_penalty_endgame[file] / abs(other/8 - rank);
            }

            // passed pawns are good
            if (!(passed_pawn_mask & board->pieces[1-who][PAWN])) {
                if (who) {
                    subscore += passed_pawn_table[7 - rank];
                    subscore_eg += passed_pawn_table[7 - rank];
                }
                else {
                    subscore += passed_pawn_table[rank];
                    subscore_eg += passed_pawn_table[7 - rank];
                }
                stored->passed_pawns[who] |= (1ull << square);
            }

            mask = 0;
            // isolated pawns are bad
            if (file != 0) mask |= (AFILE << (file - 1));
            if (file != 7) mask |= (AFILE << (file + 1));
            if (!penalized && !(mask & board->pieces[who][PAWN])) {
                subscore -= isolated_pawn_penalty[file];
                subscore_eg -= isolated_pawn_penalty_endgame[file];
            }
        }

        stored->holes[who] = ~(attack_set_pawn_multiple_capture[who])(advance_mask[who], 0, ~0ull);

        // If you have no pawns, endgames will be hard
        if (!count) {
            subscore -= 120;
            subscore_eg -= 300;
        }
        if (who) {
            score -= subscore;
            score_eg -= subscore_eg;
        }
        else {
            score += subscore;
            score_eg += subscore_eg;
        }
    }

    stored->score = score;
    stored->score_eg = score_eg;
    stored->pawn_hash = board->pawn_hash;
    return stored;
}
