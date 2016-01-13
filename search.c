#include "search.h"
#include "pieces.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>

#define ISCHECKMATE(s) ((s) > CHECKMATE - 1000)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

int alpha_cutoff_count = 0;
int beta_cutoff_count = 0;
int short_circuit_count = 0;
int branches = 0;
int out_of_time = 0;
clock_t start;
int max_thinking_time = 0;

int material_table[5] = {100, 510, 325, 333, 880};
move_t killer[32];
uint64_t seen[64];
int ply;
int allow_short = 0;

int history[2][64][64];

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

void insertion_sort(move_t* moves, int* scores, int start, int end) {
    int i, j, k, ts;
    move_t tm;
    for (i = start; i < end; i++) {
        for (k = i - 1; k >= start; k--) {
            if (scores[k] >= scores[i])
                break;
        }
        for (j = k + 1; j < i; j++) {
            ts = scores[j];
            tm = moves[j];
            scores[j] = scores[i];
            moves[j] = moves[i];
            scores[i] = ts;
            moves[i] = tm;
        }
    }
}

int sort_deltaset(struct board* board, char who, struct deltaset* set) {
    // We sort deltaset to consider checks and captures first
    int i, k;
    move_t temp;
    struct transposition * stored;
    int scores[256];
    k = 0;
    for (i = 0; i < set->nmoves; i++) {
        apply_move(board, who, &set->moves[i]);
        scores[i] = 0;
        if (transposition_table_read(board->hash, &stored) == 0)
            scores[i] = -stored->score;

        if (is_in_check(board, 1-who, board_friendly_occupancy(board, 1-who), board_enemy_occupancy(board, 1-who))) {
            scores[i] += 15000;
        }
        if (scores[i] >= 10000) k++;
        reverse_move(board, who, &set->moves[i]);
    }
    insertion_sort(set->moves, scores, 0, set->nmoves);

    int start = k;
    for (i = k; i < set->nmoves; i++) {
        if (set->moves[i].captured != -1 || 
                (set->moves[i].piece == PAWN && set->moves[i].promotion != PAWN)) {
            scores[k] = 0;
            if ((1ull << set->moves[i].square2) & set->opponent_attacks)
                scores[k] -= material_table[set->moves[i].piece];
            if (set->moves[i].captured != -1) {
                scores[k] += material_table[set->moves[i].captured];
            }
            if (set->moves[i].promotion != set->moves[i].piece) {
                if (!((1ull << set->moves[i].square2) & set->opponent_attacks))
                    scores[k] += material_table[set->moves[i].promotion];
            }
            temp = set->moves[k];
            set->moves[k++] = set->moves[i];
            set->moves[i] = temp;
        }
    }

    int ncaptures = k;

    insertion_sort(set->moves, scores, start, k);
    // Killer moves
    for (i = k; i < set->nmoves; i++) {
        if (move_equal(set->moves[i], killer[ply])) {
            temp = set->moves[k];
            set->moves[k++] = set->moves[i];
            set->moves[i] = temp;
            break;
        }
    }

    for (i = k; i < set->nmoves; i++) {
        assert(history[who][set->moves[i].square1][set->moves[i].square2] >= 0);
        scores[i] = history[who][set->moves[i].square1][set->moves[i].square2];
        if (((1ull << set->moves[i].square1) & set->opponent_attacks) &&
            !((1ull << set->moves[i].square2) & set->opponent_attacks))
            scores[i] += material_table[set->moves[i].piece] * 2;
    }
    insertion_sort(set->moves, scores, k, set->nmoves);
    return ncaptures;
}

#define ALPHA_CUTOFF 1
#define BETA_CUTOFF 2
#define EXACT 4
#define MOVESTORED 8

static int transform_checkmate(struct board* board, struct transposition* trans) {
    if (trans->score > CHECKMATE - 1200) {
        return trans->score + trans->age - board->nmoves;
    }
    else if (trans->score < -CHECKMATE + 1200) {
        return trans->score - trans->age + board->nmoves;
    }
    return trans->score;
}

static int is_checkmate(int score) {
    return (score > CHECKMATE - 1200) || (score < -CHECKMATE + 1200);
}

int try_move(struct board* board, move_t* restrict move, move_t* restrict best, struct transposition* trans, int depth, int* alpha, int beta, int capturemode, int extension, int nullmode, char who) {
    move_t temp;
    apply_move(board, who, move);
    int s= -alpha_beta_search(board, &temp, depth - 10, -beta, -(*alpha), capturemode, extension, nullmode, 1 - who);
    /*
    if (ply == 1) {
        char buffer[8];
        move_to_algebraic(board, buffer, move);
        fprintf(stderr, "Move: %s (%d)\n", buffer, s);
    }
    */
    reverse_move(board, who, move);
    if (out_of_time) return 0;
    if (*alpha < s) {
        *alpha = s;
        *best = *move;
        if (!capturemode) {
            trans->move = *(struct delta_compressed *) (move);
            trans->type = EXACT | MOVESTORED;
            trans->score = s;
        }
    }
    if (beta <= *alpha) {
        beta_cutoff_count += 1;
        killer[ply] = *move;
        if (!capturemode) {
            trans->move = *(struct delta_compressed *) (move);
            trans->type = BETA_CUTOFF | MOVESTORED;
            trans->score = s;
            if (move->captured == -1 && move->promotion == move->piece)
                history[who][move->square1][move->square2] += 1;
        }
        return 0;
    }
    return 1;
}

int transposition_table_search(struct board* board, struct deltaset* out, int depth, move_t* best, struct transposition* trans, int* alpha, int beta,
        int capturemode, int extension, int nullmode, char who) {
    struct transposition * stored;
    move_t move;
    int score;
    if (transposition_table_read(board->hash, &stored) == 0 && position_count_table_read(board->hash) < 1) {
        score = transform_checkmate(board, stored);
        if (stored->depth >= depth) {
            if ((stored->type & EXACT)) {
                *best = *(move_t *) (&stored->move);
                *alpha = score;
                *trans = *stored;
                return 0;
            } 
            if ((stored->type & ALPHA_CUTOFF) &&
                    score <= *alpha) {
                *alpha = score;
                *trans = *stored;
                return 0;
            }
            if ((stored->type & BETA_CUTOFF) &&
                    score >= beta) {
                *alpha = score;
                *trans = *stored;
                return 0;
            }
        } 
        memcpy(&move, &stored->move, 8);
        // This is a proven good move, so try it first, to improve
        // alpha and beta cutoffs
        if (stored->type & MOVESTORED) {
            if (!try_move(board, &move, best, trans, depth, alpha, beta, capturemode, extension, nullmode, who))
                return 0;
        }
    }
    return -1;
}

int alpha_beta_search(struct board* board, move_t* best, int depth, int alpha, int beta, int capturemode, int extension, int nullmode, char who)
{
    if (ply != 0) {
        alpha = MAX(alpha, -CHECKMATE + board->nmoves);
        beta = MIN(beta, CHECKMATE - board->nmoves - 1);
        if (alpha >= beta) return alpha;
    }

    struct deltaset out;
    struct transposition transposition;
    int pruned = 0;
    move_t temp;
    int score = 0;
    int i = 0;
    branches += 1;
    struct transposition * stored;

    best->piece = -1; // Mark it as invalid
    if (!nullmode) {
        for (i = 0; i < ply; i++) {
            if (board->hash == seen[i]) {
                return 0;
            }
        }
        seen[ply] = board->hash;
    }
    ply++;

    if (clock() - start > max_thinking_time) {
        out_of_time = 1;
        ply--;
        return 0;
    }

    int nmoves = 0;
    transposition.type = ALPHA_CUTOFF;
    transposition.hash = board->hash;
    transposition.age = board->nmoves;
    transposition.valid = 1;
    transposition.score = alpha;

    generate_moves(&out, board, who);

    nmoves = out.nmoves;

    if (depth < 10 || nmoves == 0) {
        if (transposition_table_read(board->hash, &stored) == 0) {
            score = transform_checkmate(board, stored);
            if (!(stored->type & EXACT) && !((stored->type & ALPHA_CUTOFF) && score <= alpha) &&
                 !((stored->type & BETA_CUTOFF) && score >=beta)) {
                score = board_score(board, who, &out, alpha, beta);
                if (who) score = -score;
            }
        } else {
            score = board_score(board, who, &out, alpha, beta);
            if (who) score = -score;
        }

        if (nmoves == 0 || capturemode) {
            ply--;
            return score;
        }

        /*
        if (extension >= 2) {
            ply--;
            return score;
        }
        // If either us or opponent has few moves,
        // it is cheaper to search deeper, and there is a mating chance
        else if (nmoves < 5 || out.check) {
            extension += 1;
            capturemode = 0;
            depth += 30;
        }
        // Quiescent search:
        // Play out a few captures to see if there is imminent danger
        else */
        if (!capturemode) {
            extension += 1;
            depth += 120;
            capturemode = 1;
        } else {
            ply--;
            return score;
        }
    }

    // if (out.check) depth += 7;
    
    transposition.depth = depth;

    int initial_score = 0;
    if (capturemode) {
        score = board_score(board, who, &out, alpha, beta);
        if (who) score = -score;
        if (score >= beta && !out.check) {
            ply--;
            return score;
        }
        initial_score = score;
        if (score > alpha)
            alpha = score;
    }

    if (!transposition_table_search(board, &out, depth, best, &transposition, &alpha, beta, capturemode, extension, nullmode, who)) {
        pruned = 1;
        goto CLEANUP1;
    }
    
    // Null pruning:
    // If we skip a move, and the move is still bad for the oponent,
    // then our move must have been great
    // We check if we have at least 4 pieces. Otherwise, we might encounter zugzwang
    if (depth >= 30 && nullmode == 0 && !capturemode && !out.check && board_npieces(board, who) > 4) {
        uint64_t old_enpassant = board_flip_side(board, 1);
        score = -alpha_beta_search(board, &temp, depth - 20, -beta, -alpha,
                0, extension, 1, 1 - who);
        board_flip_side(board, old_enpassant);
        board->enpassant = old_enpassant;
        if (score >= beta) {
            ply--;
            return score;
        }
    }


    int ncaptures = sort_deltaset(board, who, &out);

    int value = 0;
    int delta_cutoff = 350;
    for (i = 0; i < ncaptures; i++) {
        // Delta-pruning for quiescent search
        if (capturemode && !out.check) {
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
                    continue;
                }
                if (alpha < value) {
                    alpha = value;
                }
            }
        }
        if (!try_move(board, &out.moves[i], best, &transposition, depth, &alpha, beta, capturemode, extension, nullmode, who))
            goto CLEANUP1;
    }
    if (capturemode && !out.check) {
        ply--;
        return alpha;
    }

    // Futility pruning
    if (depth <= 10 && !out.check && alpha > -CHECKMATE/2 && beta < CHECKMATE/2 && nmoves > 6) {
        initial_score = board_score(board, who, &out, alpha, beta);
        if (who) initial_score = -initial_score;
            // Futility pruning
        int delta_cutoff = 300;
        if (initial_score + delta_cutoff < alpha) {
            ply--;
            return initial_score + delta_cutoff;
        }
    }

    for (i = ncaptures; i < out.nmoves; i++) {
        if (!try_move(board, &out.moves[i], best, &transposition, depth, &alpha, beta, capturemode, extension, nullmode, who))
            goto CLEANUP1;
    }
CLEANUP1:
    ply--;
    if (capturemode || pruned || out_of_time) return alpha;
    transposition_table_update(&transposition);
    return alpha;
}

int prev_score[2];

move_t generate_move(struct board* board, char who, int maxt, char flags) {
    out_of_time = 0;
    int alpha, beta;
    int d, s;
    d = 20;
    max_thinking_time = maxt * CLOCKS_PER_SEC;
    if (!(flags & FLAGS_DYNAMIC_DEPTH)) {
        d = 60;
        max_thinking_time = 1000 * CLOCKS_PER_SEC;
    }
    struct deltaset out;
    generate_moves(&out, board, who);

    start = clock();

    alpha_cutoff_count = 0;
    beta_cutoff_count = 0;
    short_circuit_count = 0;
    allow_short = 0;


    move_t best, temp;
    branches = 0;
    alpha = -INFINITY;
    beta = INFINITY;
    int used_table = 0;
    static int leeway_table[32] = {100, 50, 50, 25, 25, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
    };
    if ((flags & FLAGS_USE_OPENING_TABLE) &&
            opening_table_read(board->hash, &best) == 0
            ) {
        used_table = 1;
        fprintf(stderr, "Applying opening...\n");
        apply_move(board, who, &best);
        out_of_time = 0;
        ply = 0;
        s = alpha_beta_search(board, &temp, 60, alpha, beta,
            0 /* Capture mode */, 0 /* Extension */, 0 /* null-mode */, 1 - who);
        reverse_move(board, who, &best);
    } else {
        move_t temp;
        if (flags & FLAGS_DYNAMIC_DEPTH) {
            ply = 0;
            s = alpha_beta_search(board, &best, d, -INFINITY, INFINITY,
                0 /* Capture mode */, 0 /* Extension */, 0 /* null-mode */, who);
            temp = best;
            for ( d = 60; d < 380; d += 20) {
                alpha = prev_score[who] - leeway_table[(d-60)/10];
                beta = prev_score[who] + leeway_table[(d-60)/10];
                int changea = leeway_table[(d-60)/10];
                int changeb = leeway_table[(d-60)/10];
                while (1) {
                    ply = 0;
                    s = alpha_beta_search(board, &best, d, alpha, beta,
                        0 /* Capture mode */, 0 /* Extension */, 0 /* null-mode */, who);
                    if (out_of_time) {
                        break;
                    }
                    fprintf(stderr, "alpha=%d, beta=%d, d=%d, s=%d\n", alpha, beta, d, s);
                    if (s <= alpha) {
                        alpha = alpha - changea;
                        changea *= 4;
                    }
                    else if (s >= beta) {
                        beta = beta + changeb;
                        changeb *= 4;
                    } else {
                        break;
                    }
                }
                if (out_of_time) {
                    if (best.piece == -1)
                        best = temp;
                    break;
                }
                else {
                    prev_score[who] = s;
                    temp = best;
                    allow_short = 1;
                    if (s > CHECKMATE - 1000 || s < -CHECKMATE + 1000) {
                        break;
                    }
                }
            }
        }
        else {
            for (d = 40; d <= 60; d += 20) {
                alpha = prev_score[who] - 50;
                beta = prev_score[who] + 50;
                int changea = 50;
                int changeb = 50;
                while (1) {
                    ply = 0;
                    s = alpha_beta_search(board, &best, d, alpha, beta,
                        0 /* Capture mode */, 0 /* Extension */, 0 /* null-mode */, who);
                    if (s <= alpha) {
                        alpha = alpha - changea;
                        changea *= 4;
                    }
                    else if (s >= beta) {
                        beta = beta + changeb;
                        changeb *= 4;
                    } else {
                        break;
                    }
                }
                fprintf(stderr, "alpha=%d, beta=%d, guess=%d\n", alpha, beta, prev_score[who]);
                prev_score[who] = s;
            }
        }
    }

    char buffer[8];
    move_to_algebraic(board, buffer, &best);
    fprintf(stderr, "Best scoring move is %s: %.2f\n", buffer, s/100.0);
    fprintf(stderr, "Searched %d moves, #alpha: %d, #beta: %d, shorts: %d, depth: %d\n", branches, alpha_cutoff_count, beta_cutoff_count, short_circuit_count, d-20);
    return best;
}

