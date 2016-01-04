#include "search.h"
#include "pieces.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>

int alpha_cutoff_count = 0;
int beta_cutoff_count = 0;
int short_circuit_count = 0;
int branches = 0;

int material_table[5] = {100, 510, 325, 333, 880};

int sort_deltaset(struct board* board, char who, struct deltaset* set) {
    // We sort deltaset to consider checks and captures first
    int i, j, k;
    move_t temp;
    static int ordering[5] = {QUEEN, ROOK, BISHOP, KNIGHT, PAWN};
    k = 0;
    for (i = k; i < set->nmoves; i++) {
        temp = set->moves[i];
        apply_move(board, who, &temp);
        if (is_in_check(board, 1-who, board_friendly_occupancy(board, 1-who), board_enemy_occupancy(board, 1-who))) {
            set->moves[i] = set->moves[k];
            set->moves[k++] = temp;

        }
        reverse_move(board, who, &temp);
    }
    for (j = 0; j < 5; j++) {
        for (i = k; i < set->nmoves; i++) {
            if (set->moves[i].captured == ordering[j] || 
                    (set->moves[i].piece == PAWN && set->moves[i].promotion == ordering[j] && set->moves[i].promotion != PAWN)) {
                temp = set->moves[k];
                set->moves[k++] = set->moves[i];
                set->moves[i] = temp;
            }
        }
    }
    return k;
}

#define ALPHA_CUTOFF 1
#define BETA_CUTOFF 2
#define EXACT 4
#define MOVESTORED 8

int alpha_beta_search(struct board* board, move_t* best, int depth, int alpha, int beta, int capturemode, int extension, int nullmode, char who)
{
    // engine_print();
    struct moveset mvs;
    struct deltaset out;
    struct transposition transposition, stored;
    move_t move;
    move_t temp;
    mvs.nmoves = 0;
    mvs.npieces = 0;
    int score = 0;
    int i = 0;
    branches += 1;
    char buffer[8];

    char type = ALPHA_CUTOFF;
    int nmoves = 0;

    generate_moves(&mvs, board, who);
    moveset_to_deltaset(board, &mvs, &out);

    nmoves = out.nmoves;

    if (depth == 0 || nmoves == 0) {
        score = board_score(board, who, &mvs, nmoves);
        if (who) score = -score;
        if (nmoves == 0 || nullmode) {
            free(out.moves);
            return score;
        }

        if (extension >= 4) {
            free(out.moves);
            return score;
        }
        // If either us or opponent has few moves,
        // it is cheaper to search deeper, and there is a mating chance
        else if (nmoves < 6 || mvs.check) {
            extension += 1;
            capturemode = 0;
            depth += 2;
        }
        // Quiescent search:
        // Play out a few captures to see if there is imminent danger
        else if (!capturemode && extension == 0) {
            capturemode = 1;
            extension += 1;
            depth += 2;
        } else {
            free(out.moves);
            return score;
        }
    }

    int initial_score = 0;
    if (capturemode) {
        score = board_score(board, who, &mvs, nmoves);
        if (who) score = -score;
        if (score >= beta) {
            free(out.moves);
            return score;
        }
        initial_score = score;
        if (score > alpha)
            alpha = score;
    } else if (depth == 1) {
        initial_score = board_score(board, who, &mvs, nmoves);
        if (who) initial_score = -initial_score;
    }

    if (transposition_table_read(board->hash, &stored) == 0) {
        if (stored.depth >= depth) {
            if (stored.type & EXACT) {
                assert(stored.type & MOVESTORED);
                *best = *(move_t *) (&stored.move);
                free(out.moves);
                return stored.score;
            }
            if ((stored.type & ALPHA_CUTOFF) &&
                    stored.score <= alpha) {
                free(out.moves);
                return stored.score;
            }
            if ((stored.type & BETA_CUTOFF) &&
                    stored.score >= beta) {
                short_circuit_count += 1;
                free(out.moves);
                return stored.score;
            }
        } 
        memcpy(&move, &stored.move, 8);
        // This is a proven good move, so try it first, to improve
        // alpha and beta cutoffs
        if (stored.type & MOVESTORED) {
            apply_move(board, who, &move);

            score = -alpha_beta_search(board, &temp, depth - 1, -beta, -alpha, capturemode, extension, nullmode, 1 - who);
            reverse_move(board, who, &move);
            if (alpha <= score) {
                alpha = score;
                *best = move;
                memcpy(&transposition.move, &move, 8);
                type = EXACT | MOVESTORED;
            }
            if (beta <= alpha) {
                *best = move;
                alpha = beta;
                alpha_cutoff_count += 1;
                type = BETA_CUTOFF | MOVESTORED;
                goto CLEANUP1;
            }
        }
    }

    // Null pruning:
    // If we skip a move, and the move is still bad for the oponent,
    // then our move must have been great
    // Currently buggy (probably en passant), and not sure the time savings
    // require tuning
    if (depth > 4 && nullmode == 0 && !capturemode) {
        uint64_t old_enpassant = board->enpassant;
        board->enpassant = 1;
        board_flip_side(board);
        score = -alpha_beta_search(board, &temp, depth - 3, -beta, -alpha,
                0, extension, 1, 1 - who);
        board_flip_side(board);
        board->enpassant = old_enpassant;
        if (score >= beta) {
            free(out.moves);
            return score;
        }
    }

    int ncaptures = sort_deltaset(board, who, &out);

    int value = 0;
    int delta_cutoff = 230;
    for (i = 0; i < ncaptures; i++) {
        // Delta-pruning for quiescent search
        if (capturemode) {
            if (out.moves[i].captured != -1 ||
                    out.moves[i].promotion != out.moves[i].piece) {
                if (out.moves[i].captured != -1)
                    value = initial_score + material_table[out.moves[i].captured];
                else if (out.moves[i].promotion != out.moves[i].piece) {
                    if (out.moves[i].promotion != QUEEN) break;
                    value = initial_score + 800;
                }
                if (value >= beta) {
                    alpha = value;
                    goto CLEANUP1;
                }
                if (value + delta_cutoff < alpha) {
                    goto CLEANUP1;
                }
                if (alpha < value) {
                    alpha = value;
                }
            }
        }
        apply_move(board, who, &out.moves[i]);
        score = -alpha_beta_search(board, &temp, depth - 1, -beta, -alpha, capturemode, extension, nullmode, 1 - who);
        reverse_move(board, who, &out.moves[i]);
        if (alpha < score) {
            *best = out.moves[i];
            alpha = score;
            transposition.move = *(struct delta_compressed *) (&out.moves[i]);
            type = EXACT | MOVESTORED;
        }
        if (beta <= alpha) {
            alpha_cutoff_count += 1;
            type = BETA_CUTOFF | MOVESTORED;
            goto CLEANUP1;
        }
    }
    if (capturemode) {
        goto CLEANUP1;
    }

    for (i = ncaptures; i < out.nmoves; i++) {
        int value = 0;
        int delta_cutoff = 230;
        if (depth <= 2 && !mvs.check && alpha > -CHECKMATE/2 &&
                beta < CHECKMATE/2 && nmoves > 5) {
            // Futility pruning
            delta_cutoff = 300;
            if (depth == 2) delta_cutoff = 520;
            apply_move(board, who, &out.moves[i]);
            if (initial_score + delta_cutoff < alpha) {
                reverse_move(board, who, &out.moves[i]);
                goto CLEANUP1;
            }
            reverse_move(board, who, &out.moves[i]);
        }
        apply_move(board, who, &out.moves[i]);
        score = -alpha_beta_search(board, &temp, depth - 1, -beta, -alpha, capturemode, extension, nullmode, 1 - who);
        reverse_move(board, who, &out.moves[i]);
        if (alpha < score) {
            *best = out.moves[i];
            alpha = score;
            transposition.move = *(struct delta_compressed *) (&out.moves[i]);
            type = EXACT | MOVESTORED;
        }
        if (beta <= alpha) {
            alpha_cutoff_count += 1;
            type = BETA_CUTOFF | MOVESTORED;
            goto CLEANUP1;
        }
    }
CLEANUP1:
    free(out.moves);
    if (capturemode || nullmode) return alpha;
CLEANUP:
    transposition.hash = board->hash;
    transposition.score = alpha;
    transposition.depth = depth;
    transposition.type = type;
    transposition.age = board->nmoves;
    memcpy(&move, &transposition.move, 8);
    transposition_table_update(&transposition);
    return alpha;
}

move_t generate_move(struct board* board, char who, int* depth, char flags) {
    int alpha, beta;
    struct transposition transposition, stored;
    int i;
    int score = -INFINITY;

    clock_t start = clock();

    alpha_cutoff_count = 0;
    beta_cutoff_count = 0;
    short_circuit_count = 0;

    move_t best, temp;
    branches = 0;
    alpha = -INFINITY;
    beta = INFINITY;
    int used_table = 0;
    if ((flags & FLAGS_USE_OPENING_TABLE) &&
            opening_table_read(board->hash, &best) == 0
            ) {
        used_table = 1;
        fprintf(stderr, "Applying opening...\n");
        apply_move(board, who, &best);
        score = alpha_beta_search(board, &temp, 4, alpha, beta,
            0 /* Capture mode */, 0 /* Extension */, 0 /* null-mode */, 1 - who);
        reverse_move(board, who, &best);
    } else {
        score = alpha_beta_search(board, &best, *depth, alpha, beta,
            0 /* Capture mode */, 0 /* Extension */, 0 /* null-mode */, who);
    }

    clock_t end = clock();
    if (!used_table && (flags & FLAGS_DYNAMIC_DEPTH)) {
        if (end - start < CLOCKS_PER_SEC / 6) *depth += 1;
        if (end - start > CLOCKS_PER_SEC * 20) *depth -= 1;
        if (end - start > CLOCKS_PER_SEC * 40) *depth -= 1;
        if (*depth <= 5) *depth = 5;
    }

    char buffer[8];
    move_to_algebraic(board, buffer, &best);
    fprintf(stderr, "Best scoring move is %s: %.2f, %lu\n", buffer, score/100.0, (end-start)*1000/CLOCKS_PER_SEC);
    fprintf(stderr, "Searched %d moves, #alpha: %d, #beta: %d, shorts: %d\n", branches, alpha_cutoff_count, beta_cutoff_count, short_circuit_count);
    return best;
}
