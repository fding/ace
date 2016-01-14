#include "search.h"
#include "pieces.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

// Transposition table code
#define HASHMASK1 ((1ull << 26) - 1)

static void transposition_table_update(struct transposition * update) {
    int hash1 = HASHMASK1 & update->hash;
    if (transposition_table[hash1].valid) {
        if (transposition_table[hash1].hash == update->hash) {
            if (update->depth > transposition_table[hash1].depth)
                transposition_table[hash1] = *update;
            transposition_table[hash1].valid = update->valid;
        } else {
            if (transposition_table[hash1].depth < update->depth
                    || transposition_table[hash1].age > update->age + 10)
                transposition_table[hash1] = *update;
        }
    }
    else {
        transposition_table[hash1] = *update;
    }
}

static int transposition_table_read(uint64_t hash, struct transposition** value) {
    int hash1 = HASHMASK1 & hash;
    if (transposition_table[hash1].hash == hash) {
        *value = &transposition_table[hash1];
        return 0;
    }
    return -1;
}

int alpha_cutoff_count = 0;
int beta_cutoff_count = 0;
int short_circuit_count = 0;
int branches = 0;
int out_of_time = 0;
clock_t start;
int max_thinking_time = 0;

int material_table[5] = {100, 510, 325, 333, 900};


struct killer_slot {
    move_t m1;
    move_t m2;
};

struct killer_slot killer[32];

uint64_t seen[64];
int ply;

int history[2][64][64];

static int sort_deltaset(struct board* board, char who, struct deltaset* set);
int search(struct board* board, move_t* best, int depth, int alpha, int beta,
        int capturemode, int nullmode, char who);
static int try_move(struct board* board, move_t* move, move_t* best, struct transposition* trans,
        int depth, int* alpha, int beta, int capturemode, int nullmode, char* pvariation, char who);

static int transposition_table_search(struct board* board, struct deltaset* out, int depth, move_t* best,
        struct transposition* trans, int* alpha, int beta, int capturemode, int nullmode, char* pvariation, char who);

// We only need to copy the first 8 bytes of m2 to m1, and this can be done in one operation
#define move_copy(m1, m2) (*((struct delta_compressed *) (m1)) = *((struct delta_compressed *) (m2)))

static void update_killer(int ply, move_t* m) {
    if (!move_equal(*m, killer[ply].m1)) {
        move_copy(&killer[ply].m2, &killer[ply].m1);
        move_copy(&killer[ply].m1, m);
    }
}

static void insertion_sort(move_t* moves, int* scores, int start, int end) {
    /* A stable sorting algorithm */
    int i, j, k, ts;
    move_t tm;
    for (i = start; i < end; i++) {
        for (k = i - 1; k >= start; k--) {
            if (scores[k] >= scores[i])
                break;
        }
        for (j = k + 1; j < i; j++) {
            ts = scores[j];
            move_copy(&tm, &moves[j]);
            scores[j] = scores[i];
            move_copy(&moves[j], &moves[i]);
            scores[i] = ts;
            move_copy(&moves[i], &tm);
        }
    }
}

static int move_see(struct board* board, struct deltaset* set, move_t* move) {
    int score = 0;
    if ((1ull << move->square2) & set->opponent_attacks)
        score -= material_table[move->piece];
    if (move->captured != -1) {
        score += material_table[move->captured];
    }
    if (move->promotion != move->piece) {
        if (!((1ull << move->square2) & set->opponent_attacks))
            score += material_table[move->promotion];
    }
    return score;
}

static int sort_deltaset(struct board* board, char who, struct deltaset* set) {
    /*
     * Sorts the available moves according to how good it is. The criterion is:
     * 1. Moves that result in a won position in the transposition table
     * 2. Moves that give check
     * 3. Captures and pawn promotions. The score of such a move is the value of the captured piece,
     *    minus the value of the capturing piece if the attacked square is defended,
     *    plus the value of the promotion if the pawn cannot be taken immediately
     * 4. Killer moves (moves that caused beta-cutoffs in sibling nodes)
     * 5. All other moves, sorted by history heuristic
     *
     * Ties are broken by the score of the position after the move as recorded in the transposition table
     */
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

        // This way, checks will be before all other moves except checkmates
        if (is_in_check(board, 1-who, board_occupancy(board, 1-who), board_occupancy(board, who))) {
            scores[i] += 15000;
        }
        if (scores[i] >= 10000) k++;
        reverse_move(board, who, &set->moves[i]);
    }

    // Sort the moves based on score of following position
    // This will move the checkmates to the front of the list,
    // immediately followed by check giving moves
    // All other moves are also sorted,
    // but later on, they will be scrambled.
    // However, this preliminary sort will help break ties
    insertion_sort(set->moves, scores, 0, set->nmoves);

    int start = k;
    for (i = k; i < set->nmoves; i++) {
        if (set->moves[i].captured != -1 || 
                (set->moves[i].piece == PAWN && set->moves[i].promotion != PAWN)) {
            scores[k] = move_see(board, set, &set->moves[i]);
            move_copy(&temp, &set->moves[k]);
            move_copy(&set->moves[k++], &set->moves[i]);
            move_copy(&set->moves[i], &temp);
        }
    }

    int ncaptures = k;

    insertion_sort(set->moves, scores, start, k);
    // Killer moves
    for (i = k; i < set->nmoves; i++) {
        if (move_equal(set->moves[i], killer[ply].m1)) {
            move_copy(&temp, &set->moves[k]);
            move_copy(&set->moves[k++], &set->moves[i]);
            move_copy(&set->moves[i], &temp);
            break;
        }
    }
    for (i = k; i < set->nmoves; i++) {
        if (move_equal(set->moves[i], killer[ply].m2)) {
            move_copy(&temp, &set->moves[k]);
            move_copy(&set->moves[k++], &set->moves[i]);
            move_copy(&set->moves[i], &temp);
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

static int try_move(struct board* board, move_t* restrict move, move_t* restrict best, struct transposition* trans,
        int depth, int* alpha, int beta, int capturemode, int nullmode, char* pvariation, char who) {
    move_t temp;
    apply_move(board, who, move);
    int s;
    if (capturemode || *pvariation || ply == 1) {
        s = -search(board, &temp, depth - 10, -beta, -(*alpha), capturemode, nullmode, 1 - who);
    } else {
        s = -search(board, &temp, depth - 10, -(*alpha) - 1, -(*alpha), capturemode, nullmode, 1 - who);
        if (!out_of_time && s >= *alpha && s < beta)
            s = -search(board, &temp, depth - 10, -beta, -(*alpha), capturemode, nullmode, 1 - who);
    }
    reverse_move(board, who, move);
    if (*alpha < s) {
        *alpha = s;
        *best = *move;
        if (!capturemode) {
            move_copy(&trans->move, move);
            trans->type = EXACT | MOVESTORED;
            trans->score = s;
        }
        if (s < beta)
            *pvariation = 0;
    }
    if (beta <= *alpha) {
        beta_cutoff_count += 1;
        if (move->captured == -1)
            update_killer(ply, move);
        if (!capturemode) {
            move_copy(&trans->move, move);
            trans->type = BETA_CUTOFF | MOVESTORED;
            trans->score = s;
            if (move->captured == -1 && move->promotion == move->piece)
                history[who][move->square1][move->square2] += depth;
        }
        return 0;
    }
    if (out_of_time) return 0;
    return 1;
}

static int transposition_table_search(struct board* board, struct deltaset* out, int depth, move_t* best,
        struct transposition* trans, int* alpha, int beta, int capturemode, int nullmode, char* pvariation, char who) {
    struct transposition * stored;
    move_t move;
    int score;
    if (transposition_table_read(board->hash, &stored) == 0 && position_count_table_read(board->hash) < 1) {
        score = transform_checkmate(board, stored);
        if (stored->depth >= depth) {
            if ((stored->type & EXACT)) {
                move_copy(best, &stored->move);
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
        move_copy(&move, &stored->move);
        // This is a proven good move, so try it first, to improve
        // alpha and beta cutoffs
        if (stored->type & MOVESTORED) {
            if (!try_move(board, &move, best, trans, depth, alpha, beta, capturemode, nullmode, pvariation, who))
                return 0;
        }
    }
    return -1;
}

int search(struct board* board, move_t* best,
        int depth, int alpha, int beta, int capturemode, int nullmode, char who) {
    int ai, bi;
    ai = alpha;
    bi = beta;
    if (ply != 0) {
        alpha = MAX(alpha, -CHECKMATE + board->nmoves);
        beta = MIN(beta, CHECKMATE - board->nmoves - 1);
        if (alpha >= beta) return alpha;
    }

    struct deltaset out;
    struct transposition transposition;
    move_t temp;
    int score = 0;
    int i = 0;
    char pvariation = 1;
    branches += 1;
    struct transposition * stored;

    // Mark it as invalid, in case we return prematurely (due to time limit)
    best->piece = -1; 

    // Detect cycles, which result in a drawn position
    for (i = 0; i < ply; i++) {
        if (board->hash == seen[i]) {
            return 0;
        }
    }
    seen[ply] = board->hash;
    ply++;

    // Check if we are out of time. If so, abort
    if (clock() - start > max_thinking_time) {
        out_of_time = 1;
        ply--;
        return 0;
    }

    int nmoves = 0;

    // If alpha is never changed, we get an alpha cutoff on this node
    transposition.type = ALPHA_CUTOFF;
    transposition.hash = board->hash;
    transposition.age = board->nmoves;
    transposition.valid = 1;
    transposition.score = alpha;

    generate_moves(&out, board, who);

    nmoves = out.nmoves;

    // Terminal condition: no more moves are left, or we run out of depth
    if (depth < 10 || nmoves == 0) {
        // Score the node using the transposition table if posible,
        // and if not, using the static evaluation function
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

        if (!capturemode) {
            // If we haven't entered quiescent search mode, do so now.
            // We add a lot of extra depth, but we only consider captures and check giving moves
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

    // For quiescent search mode, initialize alpha to be the static score
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

    // Look in the transposition table to see if we have seen the position before,
    // and if so, return the score if possible.
    // Even if we can't return the score due to lack of depth,
    // the stored move is probably good, so we can improve the pruning
    if (!transposition_table_search(board, &out, depth, best, &transposition, &alpha, beta, capturemode, nullmode, &pvariation, who)) {
        goto CLEANUP1;
    }
    
    // Null pruning:
    // If we skip a move, and the move is still bad for the oponent,
    // then our move must have been great
    // We check if we have at least 5 pieces. Otherwise, we might encounter zugzwang
    if (depth >= 20 && nullmode == 0 && !capturemode && !out.check && board_npieces(board, who) > 4) {
        uint64_t old_enpassant = board_flip_side(board, 1);
        score = -search(board, &temp, depth - 30, -beta, -beta+1,
                0, 1, 1 - who);
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
        // Delta-pruning for quiescent search:
        // If after we capture and don't allow the opponent to respond and we're still more than a minor piece
        // worse than alpha, this capture must really suck, so no need to consider it.
        // We never prune if we are in check, because that could easily result in mistaken evaluation
        if (capturemode && !out.check) {
            if (out.moves[i].captured != -1 ||
                    out.moves[i].promotion != out.moves[i].piece) {
                // Don't consider bad captures
                value = move_see(board, &out, &out.moves[i]);
                if (value < 0)
                    continue;
                value += initial_score;
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
            } else {
                // After depth 40 (i.e. 8 plies of capture mode), restrict to captures only
                if (depth <= 40)
                    continue;
            }
        }
        if (out.moves[i].captured != -1 && depth <= 20 && !out.check) {
            // Don't consider bad captures
            // TODO: what happens if we end up skipping all legal moves? Should it trigger alpha cutoff?
            if (move_see(board, &out, &out.moves[i]) < 0)
                continue;
        }
        if (!try_move(board, &out.moves[i], best, &transposition, depth, &alpha, beta, capturemode, nullmode, &pvariation, who))
            goto CLEANUP1;
    }
    if (capturemode && !out.check) {
        ply--;
        return alpha;
    }

    // Futility pruning
    // If we reach a frontier node, we don't need to consider non-captures and non-checks
    // if the current score of the board is more than a minor piece less than alpha,
    // because our move can only increase the positional scores,
    // which is not that large of a factor
    // TODO: more agressive pruning. If ply>6 and depth<=20, also prune with delta_cutoff=500
    if ((depth <= 10 || (depth <= 20 && ply > 6))
            && !out.check && alpha > -CHECKMATE/2 && beta < CHECKMATE/2 && nmoves > 6) {
        initial_score = board_score(board, who, &out, alpha, beta);
        if (who) initial_score = -initial_score;
            // Futility pruning
        int delta_cutoff = 300;
        if (depth > 10) delta_cutoff = 500;
        if (initial_score + delta_cutoff < alpha) {
            ply--;
            return initial_score + delta_cutoff;
        }
    }

    for (i = ncaptures; i < out.nmoves; i++) {
        if (!try_move(board, &out.moves[i], best, &transposition, depth, &alpha, beta, capturemode, nullmode, &pvariation, who))
            goto CLEANUP1;
    }
CLEANUP1:
    ply--;
    if (capturemode || out_of_time) return alpha;
    transposition_table_update(&transposition);
    return alpha;
}

int prev_score[2];

move_t find_best_move(struct board* board, char who, int maxt, char flags) {
    out_of_time = 0;
    int alpha, beta;
    int d, s;
    move_t best, temp;

    max_thinking_time = maxt * CLOCKS_PER_SEC;

    if (!(flags & FLAGS_DYNAMIC_DEPTH)) {
        max_thinking_time = 1000 * CLOCKS_PER_SEC;
    }

    struct deltaset out;
    generate_moves(&out, board, who);

    start = clock();

    alpha_cutoff_count = 0;
    beta_cutoff_count = 0;
    short_circuit_count = 0;
    branches = 0;

    alpha = -INFINITY;
    beta = INFINITY;

    static int leeway_table[32] = {40, 30, 30, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
    };

    if ((flags & FLAGS_USE_OPENING_TABLE) &&
            opening_table_read(board->hash, &best) == 0) {
        fprintf(stderr, "Applying opening...\n");
        apply_move(board, who, &best);
        out_of_time = 0;
        ply = 0;
        // Analyze this position so that when we leave the opening,
        // we have some entries in the transposition table
        s = search(board, &temp, 60, alpha, beta,
            0 /* Capture mode */, 0 /* null-mode */, 1 - who);
        reverse_move(board, who, &best);
    } else {
        move_t temp;
        int maxdepth;

        if (flags & FLAGS_DYNAMIC_DEPTH) {
            maxdepth = 400;
        } else {
            maxdepth = 100;
        }

        ply = 0;

        // A depth-4 search should always be accomplishable within the time limit
        s = search(board, &best, 40, -INFINITY, INFINITY,
            0 /* Capture mode */, 0 /* null-mode */, who);
        prev_score[who] = s;
        temp = best;
        // Iterative deepening
        for ( d = 60; d < maxdepth; d += 20) {
            // Aspirated search: we hope that the score is between alpha and beta.
            // If so, then we have greatly increased search speed.
            // If not, we have to restart search
            alpha = prev_score[who] - leeway_table[(d-60)/10];
            beta = prev_score[who] + leeway_table[(d-60)/10];
            int changea = leeway_table[(d-60)/10];
            int changeb = leeway_table[(d-60)/10];
            while (1) {
                ply = 0;
                s = search(board, &best, d, alpha, beta,
                    0 /* Capture mode */, 0 /* null-mode */, who);
                if (out_of_time) {
                    break;
                }
                if (s <= alpha) {
                    alpha = alpha - changea;
                    changea *= 4;
                }
                else if (s >= beta) {
                    beta = beta + changeb;
                    changeb *= 4;
                } else {
                    assert(best.piece != -1);
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
                if (s > CHECKMATE - 1000 || s < -CHECKMATE + 1000) {
                    break;
                }
            }
        }
    }

    char buffer[8];
    assert(best.piece != -1);
    move_to_algebraic(board, buffer, &best);
    fprintf(stderr, "Best scoring move is %s: %.2f\n", buffer, s/100.0);
    fprintf(stderr, "Searched %d moves, #alpha: %d, #beta: %d, shorts: %d, depth: %d\n",
            branches, alpha_cutoff_count, beta_cutoff_count, short_circuit_count, d-20);
    return best;
}
