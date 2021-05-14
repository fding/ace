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
    uint64_t rear_span[2];
    advance_mask[0] = 0ull;
    advance_mask[1] = 0ull;
    rear_span[0] = 0ull;
    rear_span[1] = 0ull;
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
            uint64_t rear_span_mask = (AFILE << file);
            if (who) {
                rear_span_mask = rear_span_mask << (8 * rank + 8);
            } else {
                rear_span_mask = rear_span_mask >> (8 * (8 - rank));
            }
            rear_span[who] |= rear_span_mask;
            int loc = (who == 0) ? (63 - square) : square;
            subscore += pawn_table[loc];
            DPRINTF("Pawn score on %c%c: %d\n", 'a'+file, '1'+rank, pawn_table[loc]);
            subscore_eg += pawn_table_endgame[loc];
            DPRINTF("Pawn endgame score on %c%c: %d\n", 'a'+file, '1'+rank, pawn_table_endgame[loc]);

            uint64_t ahead = (AFILE << file);
            uint64_t behind = (AFILE << file);
            if (who) {
                ahead = ahead >> (8 * (8 - rank));
                behind = behind << (8 * (rank));
            }
            else {
                ahead = ahead << (8 * (rank + 1));
                behind = behind >> (8 * (7 - rank));
            }

            uint64_t passed_pawn_mask = ahead;
            uint64_t backward_mask = behind;
            if (file != 0) {
                passed_pawn_mask |= LEFT(ahead);
                backward_mask |= LEFT(behind);
            }
            if (file != 7) {
                passed_pawn_mask |= RIGHT(ahead);
                backward_mask |= RIGHT(behind);
            }
            backward_mask ^= (1ull << square);

            int rank7pawn = ((who == 0 && rank == 6) || (who == 1 && rank == 1));
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
                    attackers |= LEFT(sq1ahead);
                    blockers |= LEFT(sq2ahead);
                }
                if (file != 7) {
                    attackers |= RIGHT(sq1ahead);
                    blockers |= RIGHT(sq2ahead);
                }
                if (!(attackers & board->pieces[1-who][PAWN]) && (blockers & board->pieces[1-who][PAWN])) {
                    penalized = 1;
                    subscore += MIDGAME_BACKWARD_PAWN;;
                    subscore_eg += ENDGAME_BACKWARD_PAWN;
                    DPRINTF("Backward pawn penalty on %c%c: %d\n", 'a'+file, '1'+rank, MIDGAME_BACKWARD_PAWN);
                    DPRINTF("Backward pawn eg penalty on %c%c: %d\n", 'a'+file, '1'+rank, ENDGAME_BACKWARD_PAWN);
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
                    supporters |= LEFT(sq1behind);
                }
                if (file != 7) {
                    supporters |= RIGHT(sq1behind);
                }
                if (supporters & board->pieces[who][PAWN]) {
                    subscore += MIDGAME_SUPPORTED_PAWN;
                    subscore_eg += ENDGAME_SUPPORTED_PAWN;
                    DPRINTF("Supported pawn bonus on %c%c: %d\n", 'a'+file, '1'+rank, MIDGAME_SUPPORTED_PAWN);
                    DPRINTF("Supported pawn eg bonus on %c%c: %d\n", 'a'+file, '1'+rank, 3);
                }
            }

            // doubled pawns are bad
            if (ahead & (board->pieces[who][PAWN])) {
                int other = LSBINDEX(ahead & board->pieces[who][PAWN]);
                subscore += doubled_pawn_penalty[file] / abs(other/8 - rank);
                subscore_eg += doubled_pawn_penalty_endgame[file] / abs(other/8 - rank);
                DPRINTF("Double pawn penalty on %c%c: %d\n", 'a'+file, '1'+rank,
                        doubled_pawn_penalty[file] / abs(other/8 - rank));
                DPRINTF("Double pawn eg penalty on %c%c: %d\n", 'a'+file, '1'+rank,
                        doubled_pawn_penalty_endgame[file] / abs(other/8 - rank));
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

            mask = 0ull;
            // isolated pawns are bad
            if (file != 0) mask |= (AFILE << (file - 1));
            if (file != 7) mask |= (AFILE << (file + 1));
            if (!penalized && !(mask & board->pieces[who][PAWN])) {
                subscore += isolated_pawn_penalty[file];
                subscore_eg += isolated_pawn_penalty_endgame[file];
                DPRINTF("Isolated pawn penalty on %c%c: %d\n", 'a'+file, '1'+rank,
                        isolated_pawn_penalty[file]);
            }
        }

        stored->holes[who] = ~(attack_set_pawn_multiple_capture[who])(advance_mask[who], 0, ~0ull);
        stored->rear_span[who] = rear_span[who];
        stored->passed_pawn_advance_span[who] = 0;
        bmloop(stored->passed_pawns[who], square, temp) {
            int file = square & 0x7;
            stored->passed_pawn_advance_span[who] = advance_mask[who] & (AFILE << file);
        }

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
