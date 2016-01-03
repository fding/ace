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

void sort_deltaset(struct board* board, char who, struct deltaset* set) {
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
    int score;
    int i;
    branches += 1;
    char buffer[8];

    char type = ALPHA_CUTOFF;
    int nmoves;
    if (mvs.nmoves == 0)
        nmoves = board_nmoves_accurate(board, who);

    generate_moves(&mvs, board, who);
    if (depth == 0 || nmoves == 0) {
        score = board_score(board, who, &mvs, nmoves);
        if (who) score = -score;
        if (nmoves == 0 || nullmode) return score;

        if (extension >= 2)
            return score;
        // If either us or opponent has few moves,
        // it is cheaper to search deeper, and there is a mating chance
        else if (extension < 2 && (nmoves < 6 || mvs.check)) {
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
            return score;
        }
    }

    if (capturemode) {
        score = board_score(board, who, &mvs, nmoves);
        if (who) score = -score;
        if (score >= beta) return score;
        if (score > alpha)
            alpha = score;
    }

    if (transposition_table_read(board->hash, &stored) == 0) {
        if (stored.depth >= depth) {
            if (stored.type & EXACT) {
                return stored.score;
            }
            if ((stored.type & ALPHA_CUTOFF) &&
                    stored.score <= alpha) {
                return stored.score;
            }
            if ((stored.type & BETA_CUTOFF) &&
                    stored.score >= beta) {
                short_circuit_count += 1;
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
                goto CLEANUP;
            }
        }
    }

    // Null pruning:
    // If we skip a move, and the move is still bad for the oponent,
    // then our move must have been great
    // Currently buggy (probably en passant), and not sure the time savings
    // require tuning
    uint64_t old_enpassant;
    if (depth > 4 && nullmode == 0 && !capturemode) {
        old_enpassant = board->enpassant;
        board->enpassant = 1;
        board_flip_side(board);
        score = -alpha_beta_search(board, &temp, depth - 3, -beta, -alpha,
                0, extension, 1, 1 - who);
        board_flip_side(board);
        board->enpassant = old_enpassant;
        if (score >= beta) return score;
    }

    moveset_to_deltaset(board, &mvs, &out);
    sort_deltaset(board, who, &out);

    for (i = 0; i < out.nmoves; i++) {
        if (capturemode && out.moves[i].captured == -1 && out.moves[i].promotion == out.moves[i].piece) {
            break;
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
            break;
        }
    }
    free(out.moves);
    if (capturemode) return alpha;
CLEANUP:
    if (!nullmode) {
        transposition.hash = board->hash;
        transposition.score = alpha;
        transposition.depth = depth;
        transposition.type = type;
        transposition.age = board->nmoves;
        memcpy(&move, &transposition.move, 8);
        transposition_table_update(&transposition);
    }
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

    move_t best;
    branches = 0;
    alpha = -INFINITY;
    beta = INFINITY;
    score = -alpha_beta_search(board, &best, *depth, alpha, beta,
        0 /* Capture mode */, 0 /* Extension */, 0 /* null-mode */, who);

    clock_t end = clock();
    if (flags & FLAGS_DYNAMIC_DEPTH) {
        if (end - start < CLOCKS_PER_SEC) *depth += 1;
        if (end - start < CLOCKS_PER_SEC * 2) *depth += 1;
        if (end - start > CLOCKS_PER_SEC * 20) *depth -= 1;
        if (end - start > CLOCKS_PER_SEC * 40) *depth -= 1;
        if (*depth <= 3) *depth = 3;
    }

    char buffer[8];
    move_to_algebraic(board, buffer, &best);
    printf("Best scoring move is %s: %.2f, %lu\n", buffer, score/100.0, (end-start)*1000/CLOCKS_PER_SEC);
    printf("Searched %d moves, #alpha: %d, #beta: %d, shorts: %d\n", branches, alpha_cutoff_count, beta_cutoff_count, short_circuit_count);
    return best;
}
