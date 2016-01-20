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
#define HASHMASK2 (((1ull << 52) - 1) & ~HASHMASK1)

static int is_checkmate(int score) {
    return (score > CHECKMATE - 1200) || (score < -CHECKMATE + 1200);
}

int tt_hits = 0;
int tt_tot = 0;

static void transposition_table_update_with_hash(int loc, struct transposition * update, int count) {
    if (transposition_table[loc].valid) {
        if (transposition_table[loc].hash == update->hash) {
            if (update->depth > transposition_table[loc].depth)
                transposition_table[loc] = *update;
            transposition_table[loc].valid = update->valid;
        }
        else {
#ifdef CUCKOO_HASHING
            if (count < 8) {
                int hash1 = transposition_table[loc].hash & HASHMASK1;
                int hash2 = (transposition_table[loc].hash & HASHMASK2) >> 26;
                struct transposition trans = transposition_table[loc];
                if (hash1 != loc)
                    transposition_table_update_with_hash(hash1, &trans, count + 1);
                else if (hash2 != loc)
                    transposition_table_update_with_hash(hash2, &trans, count + 1);
            }
#endif
            if (transposition_table[loc].depth + 1 * transposition_table[loc].age < update->depth + 1 * update->age)
                transposition_table[loc] = *update;
        }
    }
    else {
        transposition_table[loc] = *update;
    }
}

static void transposition_table_update(struct transposition * update) {
    int hash1 = HASHMASK1 & update->hash;
#ifdef CUCKOO_HASHING
    if (transposition_table[hash1].hash != update->hash) {
        uint64_t hash2 = (HASHMASK2 & update->hash) >> 26;
        transposition_table_update_with_hash(hash2, update, 0);
    }
    else {
#endif
        transposition_table_update_with_hash(hash1, update, 0);
#ifdef CUCKOO_HASHING
    }
#endif
}

static int transposition_table_read(uint64_t hash, struct transposition** value) {
    int hash1 = HASHMASK1 & hash;
    if (transposition_table[hash1].hash == hash) {
        *value = &transposition_table[hash1];
        tt_hits += 1;
        return 0;
    }
#ifdef CUCKOO_HASHING
    else {
        uint64_t hash2 = (HASHMASK2 & hash) >> 26;
        if (transposition_table[hash2].hash == hash) {
            *value = &transposition_table[hash2];
            tt_hits += 1;
            return 0;
        }
    }
#endif
    tt_tot += 1;
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

static void print_position_table() {
    int sums[64];
    for (int i = 0; i < 64; i++)
        sums[i] = 0;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 64; j++)
            for (int k = 0; k < 64; k++) 
                sums[k] += history[i][j][k];
    }
    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            fprintf(stderr, "%d ", sums[8*i+j]);
        }
        fprintf(stderr, "\n");
    }
}

static int sort_deltaset(struct board* board, char who, struct deltaset* set, move_t* tablemove);
int search(struct board* board, move_t* restrict best, move_t* restrict prev, int depth, int alpha, int beta, int extensions, 
        int nullmode, char who);

static int transposition_table_search(struct board* board, struct deltaset* set, int depth, move_t* best, move_t* move,
        int * alpha, int beta);

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
    int i, k, ts;
    move_t tm;
    for (i = start + 1; i < end; i++) {
        ts = scores[i];
        move_copy(&tm, &moves[i]);
        for (k = i; k > start && scores[k - 1] < ts; k--) {
            move_copy(&moves[k], &moves[k-1]);
            scores[k] = scores[k-1];
        }
        scores[k] = ts;
        move_copy(&moves[k], &tm);
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

static int sort_deltaset_qsearch(struct board* board, char who, struct deltaset* set, move_t * tablemove) {
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
    int i;
    move_t temp;
    struct transposition * stored;
    int scores[256];

    for (i = 0; i < set->nmoves; i++) {
        scores[i] = 0;
        // Found in transposition table
        if (move_equal(*tablemove, set->moves[i]))
            scores[i] = 60000;
        else
            scores[i] = move_see(board, set, &set->moves[i]);
        if (gives_check(board, board_occupancy(board, who) | board_occupancy(board, 1-who), &set->moves[i], who))
            scores[i] += 500;
    }

    // Sort the moves based on score of following position
    // This will move the checkmates to the front of the list,
    // immediately followed by check giving moves
    // All other moves are also sorted,
    // but later on, they will be scrambled.
    // However, this preliminary sort will help break ties
    insertion_sort(set->moves, scores, 0, set->nmoves);

    return set->nmoves;
}
static int sort_deltaset(struct board* board, char who, struct deltaset* set, move_t * tablemove) {
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
        scores[i] = 0;

        // Found in transposition table
        if (move_equal(*tablemove, set->moves[i]))
            scores[i] = 60000;

        // This way, checks will be before all other moves except checkmates
        if (gives_check(board, board_occupancy(board, 1-who) | board_occupancy(board, who), &set->moves[i], who))
            scores[i] += 20000 + move_see(board, set, &set->moves[i]);
        if (scores[i] > 10000) k++;
    }
    int nchecks = k;

    // Sort the moves based on score of following position
    // This will move the checkmates to the front of the list,
    // immediately followed by check giving moves
    // All other moves are also sorted,
    // but later on, they will be scrambled.
    // However, this preliminary sort will help break ties
    insertion_sort(set->moves, scores, 0, set->nmoves);

    int start = k;
    int ts;
    for (i = k; i < set->nmoves; i++) {
        if (set->moves[i].captured != -1 || 
                (set->moves[i].piece == PAWN && set->moves[i].promotion != PAWN)) {
            scores[k] = move_see(board, set, &set->moves[i]);
            move_copy(&temp, &set->moves[k]);
            move_copy(&set->moves[k++], &set->moves[i]);
            move_copy(&set->moves[i], &temp);
        }
    }
    insertion_sort(set->moves, scores, start, k);

    for (i = start; i < k; i++) {
        if (scores[i] < 0) break;
    }

    k = i;

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
        scores[i] = history[who][set->moves[i].square1][set->moves[i].square2];
    }
    insertion_sort(set->moves, scores, k, set->nmoves);
    return nchecks;
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


static int transposition_table_search(struct board* board, struct deltaset* set, int depth, move_t* best, move_t* move, int * alpha, int beta) {
    struct transposition * stored;
    int score;
    move->piece = -1;
    if (transposition_table_read(board->hash, &stored) == 0 && position_count_table_read(board->hash) < 1 && ply > 1) {
        score = transform_checkmate(board, stored);
        if (stored->depth >= depth) {
            if ((stored->type & EXACT)) {
                move_copy(best, &stored->move);
                // *best = set->moves[stored->move];
                *alpha = score;
                return 0;
            } 
            if ((stored->type & ALPHA_CUTOFF) && score <= *alpha) {
                *alpha = score;
                return 0;
            }
            if ((stored->type & BETA_CUTOFF) && score >= beta) {
                *alpha = score;
                return 0;
            }
        } 
        if (stored->type & MOVESTORED) {
            move_copy(move, &stored->move);
            // *move = set->moves[stored->move];
            return 1;
        }
    }
    return -1;
}

int qsearch(struct board* board, int depth, int alpha, int beta, char who) {
    beta_cutoff_count += 1;
    branches += 1;
    alpha = MAX(alpha, -CHECKMATE + board->nmoves);
    beta = MIN(beta, CHECKMATE - board->nmoves - 1);
    if (alpha >= beta) return alpha;

    struct deltaset out;
    move_t temp;
    int score = 0;
    int i = 0;
    struct transposition * stored;

    // Check if we are out of time. If so, abort
    // Since clock() is costly, don't do this all the time!
    if (branches % 65536 == 0) {
        if (clock() - start > max_thinking_time) {
            out_of_time = 1;
            return 0;
        }
    }

    move_t tablemove;
    move_t best;

    int nmoves = 0;
    generate_captures(&out, board, who);

    struct deltaset out1;
    generate_moves(&out1, board, who);

    nmoves = out.nmoves;

    // For quiescent search mode, initialize alpha to be the static score
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
    int initial_score = score;

    // Terminal condition: no more moves are left, or we run out of depth
    if (depth < 10 || nmoves == 0) {
        return score;
    }

    if (initial_score >= beta) return initial_score;
    if (initial_score + 1000 < alpha) return initial_score + 1000;
    alpha = MAX(alpha, initial_score);

    // Look in the transposition table to see if we have seen the position before,
    // and if so, return the score if possible.
    // Even if we can't return the score due to lack of depth,
    // the stored move is probably good, so we can improve the pruning
    tablemove.piece = -1;
    int res = transposition_table_search(board, &out1, depth, &best, &tablemove, &alpha, beta);
    if (res == 0) {
        return alpha;
    }

    sort_deltaset_qsearch(board, who, &out, &tablemove);

    int value = 0;
    int delta_cutoff = 300;
    for (i = 0; i < out.nmoves; i++) {
        // Delta-pruning for quiescent search:
        // If after we capture and don't allow the opponent to respond and we're still more than a minor piece
        // worse than alpha, this capture must really suck, so no need to consider it.
        // We never prune if we are in check, because that could easily result in mistaken evaluation
        if (!out.check) {
            int see = move_see(board, &out, &out.moves[i]);
            value = initial_score + see;
            if (value >= beta) {
                alpha = value;
                return alpha;
            }
            if (value + delta_cutoff < alpha) {
                continue;
            }
            if (alpha < value) {
                alpha = value;
            }
            /*
            // Don't consider bad captures
            // TODO: what happens if we end up skipping all legal moves? Should it trigger alpha cutoff?
            if (!pvnode && !out.check && see < 0)
                continue;
                */
        }

        apply_move(board, who, &out.moves[i]);
        score = -qsearch(board, depth - 10, -beta, -alpha, 1 - who);
        reverse_move(board, who, &out.moves[i]);
        if (alpha < score) {
            alpha = score;
            if (beta <= alpha) {
                return alpha;
            }
        }
    }
    return alpha;
}

int futility_margin[7] = {0, 100, 200, 300, 500, 900, 1200};

int search(struct board* board, move_t* restrict best, move_t* restrict prev,
        int depth, int alpha, int beta, int extensions, int nullmode, char who) {
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
    // Since clock() is costly, don't do this all the time!
    if (branches % 65536 == 0) {
        // Check if we are out of time. If so, abort
        if (clock() - start > max_thinking_time) {
            out_of_time = 1;
            ply--;
            return 0;
        }
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

    int initial_score = 0;
    initial_score = board_score(board, who, &out, alpha, beta);
    if (who) initial_score = -initial_score;

    if (extensions > 0 && !nullmode) {
        if (out.check) {
            if (extensions > 20) {
                extensions -= 10;
                depth += 10;
            }
            else {
                if (prev) {
                    int see = 0;
                    if ((1ull << prev->square2) & out.my_attacks)
                        see -= material_table[prev->piece];
                    if (prev->captured != -1) {
                        see += material_table[prev->captured];
                    }
                    if (prev->promotion != prev->piece) {
                        if (!((1ull << prev->square2) & out.my_attacks))
                            score += material_table[prev->promotion];
                    }
                    if (see >= 0) {
                        depth += 15;
                        extensions -= 15;
                    }
                }
            }
        }
        else if (nmoves == 1) {
            extensions -= 10;
            depth += 10;
        }
    }
    
    // Terminal condition: no more moves are left, or we run out of depth
    if (depth < 10 || nmoves == 0) {
        if (nmoves == 0) {
            ply--;
            return initial_score;
        }
        ply--;
        score = qsearch(board, 300, alpha, beta, who);
        return score;
    }

    transposition.depth = depth;

    move_t tablemove;
    tablemove.piece = -1;

    // Look in the transposition table to see if we have seen the position before,
    // and if so, return the score if possible.
    // Even if we can't return the score due to lack of depth,
    // the stored move is probably good, so we can improve the pruning
    int res = transposition_table_search(board, &out, depth, best, &tablemove, &alpha, beta);
    if (res == 0) {
        goto CLEANUP1;
    }

    /*
    // Razoring (TODO: check if a pawn is promotable)
    if (depth < 30 && !out.check && nmoves > 1 && beta == alpha + 1 && initial_score + 700 <= alpha && tablemove.piece == -1) {
        score = qsearch(board, 320, alpha, beta, who);
        if (score < alpha - 700)
            return score;
    }
    */
    
    // Null pruning:
    // If we skip a move, and the move is still bad for the oponent,
    // then our move must have been great
    // We check if we have at least 5 pieces. Otherwise, we might encounter zugzwang
    if (depth >= 20 && nullmode == 0 && !out.check && board_npieces(board, who) > 4) {
        uint64_t old_enpassant = board_flip_side(board, 1);
        score = -search(board, &temp, NULL, depth - 30, -beta, -beta+1, extensions, 1, 1 - who);
        if (score >= beta) {
            ply--;
            board_flip_side(board, old_enpassant);
            board->enpassant = old_enpassant;
            return score;
        } else {
            // Mate threat extension: if not doing anything allows opponents to checkmate us,
            // we are in a potentially dangerous situation, so extend search
            score = search(board, &temp, NULL, depth - 30, CHECKMATE/2 - 1, CHECKMATE/2, extensions, 1, 1 - who);
            board_flip_side(board, old_enpassant);
            board->enpassant = old_enpassant;
            alpha_cutoff_count += 1;
            if (score > CHECKMATE/2) {
                extensions -= 5;
                depth += 10;
            }
        }
    }

    // Internal iterative depening
    if (beta > alpha + 1 && tablemove.piece == -1 && depth >= 50 && !nullmode) {
        search(board, &tablemove, prev, depth - 20, alpha, beta, 0, nullmode, who);
    }

    int nchecks = sort_deltaset(board, who, &out, &tablemove);

    int value = 0;
    int allow_prune = !out.check && (nmoves > 6);
    int late_move = out.nmoves / 2;
    for (i = 0; i < out.nmoves; i++) {
        if (out.moves[i].captured != -1 && depth <= 20 && allow_prune) {
            // Don't consider bad captures
            // TODO: what happens if we end up skipping all legal moves? Should it trigger alpha cutoff?
            if (move_see(board, &out, &out.moves[i]) < 0)
                continue;
        }
        // Futility pruning
        // If we reach a frontier node, we don't need to consider non-winning captures and non-checks
        // if the current score of the board is more than a minor piece less than alpha,
        // because our move can only increase the positional scores,
        // which is not that large of a factor
        // TODO: more agressive pruning. If ply>7 and depth<=20, also prune with delta_cutoff=500
        if (out.moves[i].captured == -1 && out.moves[i].promotion == out.moves[i].piece && i >= nchecks && (depth <= 60)
                && alpha > -CHECKMATE/2 && beta < CHECKMATE/2 && allow_prune) {
            if (initial_score + futility_margin[depth / 10] < alpha) {
                continue;
            }
        }
        // Late move reduction:
        // If we are in a non-pv node, most moves are probably not that great (i.e. can improve upon the value in the pv node)
        // We also sort moves, so that good moves are probably at the front
        // Hence, if the move is non-tactical and appears near the end,
        // it probably isn't as good, so we can search at reduced depth
        if (i > late_move && i >= nchecks && allow_prune && beta == alpha + 1 && depth >= 30 && out.moves[i].captured == -1
                && out.moves[i].promotion == out.moves[i].piece) {
            depth -= 10;
        }

        move_t temp;
        move_t * move = &out.moves[i];

        apply_move(board, who, move);
        int s;
        if (pvariation || ply == 1) {
            s = -search(board, &temp, move, depth - 10, -beta, -alpha, extensions, nullmode, 1 - who);
        } else {
            s = -search(board, &temp, move, depth - 10, -alpha - 1, -alpha, extensions, nullmode, 1 - who);
            if (!out_of_time && s >= alpha && s < beta)
                s = -search(board, &temp, move, depth - 10, -beta, -alpha, extensions, nullmode, 1 - who);
        }
        reverse_move(board, who, move);
        if (alpha < s) {
            alpha = s;
            *best = *move;
            move_copy(&transposition.move, move);
            transposition.type = EXACT | MOVESTORED;
            transposition.score = s;
            pvariation = 0;
            history[who][move->square1][move->square2] += depth / 8;
        }
        if (beta <= alpha) {
            beta_cutoff_count += 1;
            if (move->captured == -1)
                update_killer(ply, move);
            transposition.type = BETA_CUTOFF | MOVESTORED;
            goto CLEANUP1;
        }
    }

CLEANUP1:
    ply--;
    if (out_of_time) return alpha;
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
        s = search(board, &temp, NULL, 60, alpha, beta, 30, 0 /* null-mode */, 1 - who);
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
        s = search(board, &best, NULL, 20, -INFINITY, INFINITY, 20, 0 /* null-mode */, who);
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
                s = search(board, &best, NULL, d, alpha, beta, 50, 0 /* null-mode */, who);
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
    fprintf(stderr, "Searched %d moves, #alpha: %d, #beta: %d, shorts: %d, depth: %d, TT hits: %.5f\n",
            branches, alpha_cutoff_count, beta_cutoff_count, short_circuit_count, d-20, tt_hits/((float) tt_tot));
    print_position_table();
    return best;
}
