#include "search.h"
#include "pieces.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>

#define ISCHECKMATE(s) ((s) > CHECKMATE - 1000)

int alpha_cutoff_count = 0;
int beta_cutoff_count = 0;
int short_circuit_count = 0;
int branches = 0;
int out_of_time = 0;
clock_t start;
int max_thinking_time = 0;

int material_table[5] = {100, 510, 325, 333, 880};
move_t killer[32];
uint64_t seen[32];
int ply;
int allow_short = 0;

struct absearchparams {
    int capturemode;
    int extension;
    int nullmode;
};

int sort_deltaset(struct board* board, char who, struct deltaset* set);
int alpha_beta_search(struct board* board, move_t* best, int depth, int alpha, int beta, int capturemode, int extension, int nullmode, char who);
int try_move(struct board* board, move_t* move, move_t* best, struct transposition* trans, int depth, int* alpha, int beta, int capturemode, int extension, int nullmode, char who);

int transposition_table_search(struct board* board, struct deltaset* out, int depth, move_t* best, struct transposition* trans, int* alpha, int beta,
        int capturemode, int extension, int nullmode, char who);

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
    // Killer moves
    for (i = k; i < set->nmoves; i++) {
        if (move_equal(set->moves[i], killer[ply])) {
            temp = set->moves[k];
            set->moves[k] = set->moves[i];
            set->moves[i] = temp;
            break;
        }
    }
    return k;
}

#define ALPHA_CUTOFF 1
#define BETA_CUTOFF 2
#define EXACT 4
#define MOVESTORED 8


int try_move(struct board* board, move_t* restrict move, move_t* restrict best, struct transposition* trans, int depth, int* alpha, int beta, int capturemode, int extension, int nullmode, char who) {
    move_t temp;
    apply_move(board, who, move);
    int s= -alpha_beta_search(board, &temp, depth - 1, -beta, -(*alpha), capturemode, extension, nullmode, 1 - who);
    reverse_move(board, who, move);
    if (*alpha < s) {
        *alpha = s;
        *best = *move;
        if (!capturemode && !nullmode && !out_of_time) {
            trans->move = *(struct delta_compressed *) (move);
            trans->type = EXACT | MOVESTORED;
            trans->score = s;
        }
    }
    if (beta <= *alpha) {
        killer[ply] = *move;
        if (!capturemode && !nullmode && !out_of_time) {
            trans->move = *(struct delta_compressed *) (move);
            trans->type = BETA_CUTOFF | MOVESTORED;
            trans->score = s;
        }
        return 0;
    }
    return 1;
}

int transposition_table_search(struct board* board, struct deltaset* out, int depth, move_t* best, struct transposition* trans, int* alpha, int beta,
        int capturemode, int extension, int nullmode, char who) {
    struct transposition stored;
    move_t move;
    move_t temp;
    int s;
    if (transposition_table_read(board->hash, &stored) == 0 && stored.age < board->nmoves - 2) {
        if (stored.depth >= depth) {
            if ((stored.type & EXACT) && allow_short) {
                assert(stored.type & MOVESTORED);
                *best = *(move_t *) (&stored.move);
                *alpha = stored.score;
                return 0;
            } 
            if ((stored.type & ALPHA_CUTOFF) &&
                    stored.score <= *alpha && allow_short) {
                *alpha = stored.score;
                return 0;
            }
            if ((stored.type & BETA_CUTOFF) &&
                    stored.score >= beta && allow_short) {
                *alpha = stored.score;
                return 0;
            }
        } 
        memcpy(&move, &stored.move, 8);
        // This is a proven good move, so try it first, to improve
        // alpha and beta cutoffs
        if (stored.type & MOVESTORED) {
            if (!try_move(board, &move, best, trans, depth, alpha, beta, capturemode, extension, nullmode, who))
                return 0;
        }
    }
    return -1;
}

int alpha_beta_search(struct board* board, move_t* restrict best, int depth, int alpha, int beta, int capturemode, int extension, int nullmode, char who)
{
    // engine_print();
    struct moveset mvs;
    struct deltaset out;
    struct transposition transposition, stored;
    int pruned = 0;
    move_t move;
    move_t temp;
    mvs.nmoves = 0;
    mvs.npieces = 0;
    int score = 0;
    int i = 0;
    branches += 1;
    char buffer[8];

    for (i = 0; i < ply; i++) {
        if (board->hash == seen[i]) {
            return 0;
        }
    }
    seen[ply++] = board->hash;

    int nmoves = 0;
    transposition.type = ALPHA_CUTOFF;
    transposition.hash = board->hash;
    transposition.age = board->nmoves;

    generate_moves(&mvs, board, who);
    moveset_to_deltaset(board, &mvs, &out);

    nmoves = out.nmoves;

    if (clock() - start > max_thinking_time) {
        out_of_time = 1;
        ply--;
        return alpha;
    }

    if (depth == 0 || nmoves == 0) {
        score = board_score(board, who, &mvs, nmoves);
        if (who) score = -score;
        if (nmoves == 0 || nullmode) {
            free(out.moves);
            ply--;
            return score;
        }

        if (extension >= 3) {
            free(out.moves);
            ply--;
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
            ply--;
            return score;
        }
    }
    
    transposition.depth = depth;

    int initial_score = 0;
    if (capturemode) {
        score = board_score(board, who, &mvs, nmoves);
        if (who) score = -score;
        if (score >= beta) {
            free(out.moves);
            ply--;
            return score;
        }
        initial_score = score;
        if (score > alpha)
            alpha = score;
    } else if (depth == 1) {
        initial_score = board_score(board, who, &mvs, nmoves);
        if (who) initial_score = -initial_score;
    }

    if (!transposition_table_search(board, &out, depth, best, &transposition, &alpha, beta, capturemode, extension, nullmode, who)) {
        pruned = 1;
        goto CLEANUP1;
    }
    
    // Null pruning:
    // If we skip a move, and the move is still bad for the oponent,
    // then our move must have been great
    // We check if we have at least 4 pieces. Otherwise, we might encounter zugzwang
    if (depth > 4 && nullmode == 0 && !capturemode && !mvs.check && board_npieces(board, who) > 4) {
        uint64_t old_enpassant = board->enpassant;
        board->enpassant = 1;
        board_flip_side(board);
        score = -alpha_beta_search(board, &temp, depth - 2, -beta, -alpha,
                0, extension, 1, 1 - who);
        board_flip_side(board);
        board->enpassant = old_enpassant;
        if (score >= beta) {
            free(out.moves);
            ply--;
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
                    if (out.moves[i].promotion != QUEEN) continue;
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
        if (!try_move(board, &out.moves[i], best, &transposition, depth, &alpha, beta, capturemode, extension, nullmode, who))
            goto CLEANUP1;
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
                pruned = 1;
                goto CLEANUP1;
            }
            reverse_move(board, who, &out.moves[i]);
        }
        if (!try_move(board, &out.moves[i], best, &transposition, depth, &alpha, beta, capturemode, extension, nullmode, who))
            goto CLEANUP1;
    }
CLEANUP1:
    free(out.moves);
    ply--;
    if (capturemode || nullmode || pruned || out_of_time) return alpha;
    transposition_table_update(&transposition);
    return alpha;
}

move_t generate_move(struct board* board, char who, int maxt, char flags) {
    out_of_time = 0;
    int alpha, beta;
    struct transposition transposition, stored;
    int i, d, s;
    int score = -INFINITY;
    d = 4;
    max_thinking_time = maxt * CLOCKS_PER_SEC;
    if (!(flags & FLAGS_DYNAMIC_DEPTH))
        d = 6;

    start = clock();

    alpha_cutoff_count = 0;
    beta_cutoff_count = 0;
    short_circuit_count = 0;
    allow_short = 0;

    ply = 0;

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
        out_of_time = 0;
        score = alpha_beta_search(board, &temp, 4, alpha, beta,
            0 /* Capture mode */, 0 /* Extension */, 0 /* null-mode */, 1 - who);
        reverse_move(board, who, &best);
    } else {
        move_t temp;
        for (; ; d += 2) {
            s = alpha_beta_search(board, &best, d, alpha, beta,
                0 /* Capture mode */, 1 /* Extension */, 0 /* null-mode */, who);
            if (out_of_time) {
                best = temp;
                break;
            }
            else {
                score = s;
                temp = best;
                allow_short = 1;
                if (s > CHECKMATE - 1000 || s < -CHECKMATE + 1000) {
                    break;
                }
                if (!(flags & FLAGS_DYNAMIC_DEPTH))
                    break;
            }
        }
    }

    char buffer[8];
    move_to_algebraic(board, buffer, &best);
    fprintf(stderr, "Best scoring move is %s: %.2f\n", buffer, score/100.0);
    fprintf(stderr, "Searched %d moves, #alpha: %d, #beta: %d, shorts: %d, depth: %d\n", branches, alpha_cutoff_count, beta_cutoff_count, short_circuit_count, d-2);
    return best;
}
