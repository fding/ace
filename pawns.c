#include "pawns.h"
#include "board.h"
#include "pieces.h"
#include "moves.h"
#include "evaluation_parameters.h"
#include <stdlib.h>

#define PAWN_HASH_SIZE (1024 * 8)
struct pawn_structure pawn_hashmap[PAWN_HASH_SIZE];

uint64_t hash(uint64_t wpawns, uint64_t bpawns) {
    return ((wpawns ^ bpawns) * 0x2480041000800801ull) & (PAWN_HASH_SIZE - 1);
}

int pawn_table[64] = {
    800, 800, 800, 800, 800, 800, 800, 800,
    -6, 7, -4, -2, -2, -4, 7, -6,
    -7, -6, -3, -1, -1, -3, -6, -7,
    -7, 0, 5, 13, 13, 5, 0, -7,
    -12, -7, 10, 17, 17, 10, -7, -12,
    -13, -3, 10, 12, 12, 10, -3, -13,
    -10, 0, 2, 4, 4, 2, 0, -10,
    0, 0, 0, 0, 0, 0, 0, 0
};

int pawn_table_endgame[64] = {
    800, 800, 800, 800, 800, 800, 800, 800,
     15,  17,  14,  14,  14,  14,  17, 15,
     15,  17,  19,  19,  19,  19,  17, 15,
     10,  12,  14,  14,  14,  14,  12, 10,
     5,  7,  8,  8,  8,  8,  7, 5,
     0,  2,  3,  3,  3,  3,  2, 0,
    -5, -3, -2, -2, -2, -2, -3, -5,
    0, 0, 0, 0, 0, 0, 0, 0
};
int passed_pawn_table_endgame[8] = {0, 20, 30, 40, 77, 154, 256, 800};
int isolated_pawn_penalty_endgame[8] = {20, 25, 25, 25, 25, 25, 25, 20};
int doubled_pawn_penalty_endgame[8] = {20, 25, 25, 30, 30, 25, 25, 20};

struct pawn_structure * evaluate_pawns(struct board* board) {
    struct pawn_structure * stored;
    stored = &pawn_hashmap[board->pawn_hash & 0x1fff];
    if (stored->pawn_hash == board->pawn_hash) {
        DPRINTF("Pawn hash collision: %llx\n", board->pawn_hash);
        return stored;
    }

    int square, rank, file;
    uint64_t temp;
    uint64_t mask;
    int score = 0;
    int score_eg = 0;
    stored->passed_pawns[0] = 0;
    stored->passed_pawns[1] = 0;

    uint64_t advance_mask[2];
    advance_mask[0] = 0ull;
    advance_mask[1] = 0ull;
    uint64_t all_pawns = board->pieces[0][PAWN] | board->pieces[1][PAWN];

    for (int who = 0; who < 2; who++) {
        int count = 0;
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
            int loc = (who == 0) ? (63 - square) : square;
            subscore += pawn_table[loc];
            DPRINTF("Pawn score on %c%c: %d\n", 'a'+file, '1'+rank, pawn_table[loc]);
            subscore_eg += pawn_table_endgame[loc];
            DPRINTF("Pawn endgame score on %c%c: %d\n", 'a'+file, '1'+rank, pawn_table_endgame[loc]);

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
            uint64_t passed_pawn_mask = ahead;
            uint64_t backward_mask = behind;
            if (file != 0) {
                passed_pawn_mask |= (ahead >> 1);
                backward_mask |= (behind >> 1);
            }
            if (file != 7) {
                passed_pawn_mask |= (ahead << 1);
                backward_mask |= (behind << 1);
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
                    attackers |= (sq1ahead >> 1);
                    blockers |= (sq2ahead >> 1);
                }
                if (file != 7) {
                    attackers |= (sq1ahead << 1);
                    blockers |= (sq2ahead << 1);
                }
                if (!(attackers & board->pieces[1-who][PAWN]) && (blockers & board->pieces[1-who][PAWN])) {
                    penalized = 1;
                    subscore += MIDGAME_BACKWARD_PAWN;;
                    subscore_eg -= 30;
                    DPRINTF("Backward pawn penalty on %c%c: %d\n", 'a'+file, '1'+rank, MIDGAME_BACKWARD_PAWN);
                    DPRINTF("Backward pawn eg penalty on %c%c: %d\n", 'a'+file, '1'+rank, -30);
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
                    supporters |= (sq1behind >> 1);
                }
                if (file != 7) {
                    supporters |= (sq1behind << 1);
                }
                if (supporters & board->pieces[who][PAWN]) {
                    subscore += MIDGAME_SUPPORTED_PAWN;
                    subscore_eg += 3;
                    DPRINTF("Supported pawn bonus on %c%c: %d\n", 'a'+file, '1'+rank, MIDGAME_SUPPORTED_PAWN);
                    DPRINTF("Supported pawn eg bonus on %c%c: %d\n", 'a'+file, '1'+rank, 3);
                }
            }

            // doubled pawns are bad
            if (!penalized && (ahead & (board->pieces[who][PAWN]))) {
                int other = LSBINDEX(ahead & board->pieces[who][PAWN]);
                subscore += doubled_pawn_penalty[file] / abs(other/8 - rank);
                subscore_eg -= doubled_pawn_penalty_endgame[file] / abs(other/8 - rank);
                DPRINTF("Double pawn penalty on %c%c: %d\n", 'a'+file, '1'+rank,
                        doubled_pawn_penalty[file] / abs(other/8 - rank));
                DPRINTF("Double pawn eg penalty on %c%c: %d\n", 'a'+file, '1'+rank,
                        -doubled_pawn_penalty_endgame[file] / abs(other/8 - rank));
            }

            // passed pawns are good
            if (!(passed_pawn_mask & board->pieces[1-who][PAWN]) && !(ahead & board->pieces[who][PAWN])) {
                if (who) {
                    subscore += passed_pawn_table[7 - rank];
                    subscore_eg += passed_pawn_table_endgame[7 - rank];
                    DPRINTF("Passed pawn bonus on %c%c: %d\n", 'a'+file, '1'+rank,
                            passed_pawn_table[7 - rank]);
                    DPRINTF("Passed eg pawn bonus on %c%c: %d\n", 'a'+file, '1'+rank,
                            passed_pawn_table_endgame[7 - rank]);
                }
                else {
                    subscore += passed_pawn_table[rank];
                    subscore_eg += passed_pawn_table_endgame[rank];
                    DPRINTF("Passed pawn bonus on %c%c: %d\n", 'a'+file, '1'+rank,
                            passed_pawn_table[rank]);
                    DPRINTF("Passed eg pawn bonus on %c%c: %d\n", 'a'+file, '1'+rank,
                            passed_pawn_table_endgame[rank]);
                }
                stored->passed_pawns[who] |= (1ull << square);
            }

            mask = 0;
            // isolated pawns are bad
            if (file != 0) mask |= (AFILE << (file - 1));
            if (file != 7) mask |= (AFILE << (file + 1));
            if (!penalized && !(mask & board->pieces[who][PAWN])) {
                subscore += isolated_pawn_penalty[file];
                subscore_eg -= isolated_pawn_penalty_endgame[file];
                DPRINTF("Isolated pawn penalty on %c%c: %d\n", 'a'+file, '1'+rank,
                        isolated_pawn_penalty[file]);
            }
        }

        stored->holes[who] = ~(attack_set_pawn_multiple_capture[who])(advance_mask[who], 0, ~0ull);

        // If you have no pawns, endgames will be hard
        if (!count) {
            subscore -= 50;
            subscore_eg -= 60;
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
