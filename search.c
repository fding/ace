#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "pieces.h"
#include "search.h"
#include "timer.h"
#include "util.h"

// Transposition table code

#define ALPHA_CUTOFF 1
#define BETA_CUTOFF 2
#define EXACT 4
#define MOVESTORED 8

#define ONE_PLY 8

// Statistics

int tt_hits = 0;
int tt_tot = 0;
int alpha_cutoff_count = 0;
int beta_cutoff_count = 0;
int short_circuit_count = 0;
int branches = 0;

int hashmapsize = (1ull << 20);

// Global state

int ply;
int out_of_time = 0;
volatile int signal_stop = 0;

int material_table[5] = {100, 510, 325, 333, 900};

struct killer_slot {
    move_t m1;
    move_t m2;
};

struct killer_slot killer[32];
uint64_t seen[64];
int history[2][64][64];


static int ttable_search(struct board* board, int depth, move_t* best, move_t* move, int * alpha, int beta);
static int transform_checkmate(struct board* board, union transposition* trans);

static int is_checkmate(int score) {
    return (score > CHECKMATE - 1200) || (score < -CHECKMATE + 1200);
}

// Transposition table probing and updating

static void ttable_update_with_hash(int loc, union transposition * update, int count) {
    union transposition* entry = &ttable[loc].slot1;
    union transposition* secondary_entry = &ttable[loc].slot2;
    if (entry->metadata.type) {
        if (entry->metadata.hash == update->metadata.hash) {
            int should_replace = (!(entry->metadata.type & MOVESTORED) && (update->metadata.type & MOVESTORED)) ||
                (!(entry->metadata.type & EXACT) && (update->metadata.type & EXACT));
            if (should_replace || update->metadata.depth > entry->metadata.depth) {
                *entry = *update;
            }
        } else {
            if (!secondary_entry->metadata.type) {
                *secondary_entry = *update;
                return;
            }
            if (secondary_entry->metadata.hash == update->metadata.hash) {
                int should_replace = (!(secondary_entry->metadata.type & MOVESTORED) && (update->metadata.type & MOVESTORED)) ||
                    (!(secondary_entry->metadata.type & EXACT) && (update->metadata.type & EXACT));
                if (should_replace || update->metadata.depth > secondary_entry->metadata.depth)
                    *secondary_entry = *update;
            } else {
                if (update->metadata.depth > entry->metadata.depth)
                    *entry = *update;
                else {
                    if (secondary_entry->metadata.depth > entry->metadata.depth) {
                        *entry = *secondary_entry;
                    }
                    *secondary_entry = *update;
                }
            }
        }
    }
    else {
        *entry = *update;
        if (secondary_entry->metadata.type) {
            if (secondary_entry->metadata.hash == update->metadata.hash) {
                int should_replace = (!(secondary_entry->metadata.type & MOVESTORED) && (update->metadata.type & MOVESTORED)) ||
                    (!(secondary_entry->metadata.type & EXACT) && (update->metadata.type & EXACT));
                if (should_replace || update->metadata.depth > secondary_entry->metadata.depth)
                    *secondary_entry = *update;
            } else {
                *entry = *update;
            }
        }
    }
}

static void ttable_update(uint64_t hash, union transposition * update) {
    int loc = (hashmapsize - 1) & hash;
    update->metadata.hash = hash >> 32;
    ttable_update_with_hash(loc, update, 0);
}

static int ttable_read(uint64_t hash, union transposition** value) {
    int loc = (hashmapsize - 1) & hash;
    uint32_t sig = hash >> 32;
    tt_tot += 1;
    if (ttable[loc].slot1.metadata.hash == sig) {
        *value = &ttable[loc].slot1;
        tt_hits += 1;
        return 0;
    } else if (ttable[loc].slot2.metadata.hash == sig) {
        *value = &ttable[loc].slot2;
        tt_hits += 1;
        return 0;
    }
    return -1;
}

// Print principal variation
static void print_pv(struct board* board, int depth) {
    union transposition* stored;
    char buffer[8];
    move_t move;
    if (depth == 0) return;
    if (ttable_read(board->hash, &stored) == 0) {
        if (stored->metadata.type & MOVESTORED) {
            move_copy(&move, &stored->move);
            move_to_algebraic(board, buffer, &move);
            printf(" %s", buffer);
            apply_move(board, &move);
            print_pv(board, depth - 1);
            reverse_move(board, &move);
        }
    }
}

// Update killers for given ply
static void update_killer(int ply, move_t* m) {
    // TODO: enforce not equal?
    if (!move_equal(*m, killer[ply].m1)) {
        move_copy(&killer[ply].m2, &killer[ply].m1);
        move_copy(&killer[ply].m1, m);
    }
}

// Stable searching algorithm
static void insertion_sort(move_t* moves, int* scores, int start, int end) {
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

// Static exchange evaluation for a move
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

/* Sorts the available moves according to how good it is. The criterion is:
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
static int sort_deltaset(struct board* board, char who, struct deltaset* set, move_t * tablemove) {
    int i, k;
    move_t temp;
    int scores[256];
    k = 0;

    for (i = 0; i < set->nmoves; i++) {
        scores[i] = 0;

        // Found in transposition table
        if (tablemove->piece != -1 && move_equal(*tablemove, set->moves[i]))
            scores[i] = 60000;
        else {
            // This way, checks will be before all other moves except checkmates
            if (gives_check(board, board_occupancy(board, 1-who) | board_occupancy(board, who), &set->moves[i], who))
                scores[i] += 20000 + move_see(board, set, &set->moves[i]);

        apply_move(board, &set->moves[i]);
    union transposition * stored;
    if (ttable_read(board->hash, &stored) == 0) {
        scores[i] = -transform_checkmate(board, stored);
    }
        reverse_move(board, &set->moves[i]);
        }

        if (scores[i] > 10000) k++;
    }
    int nchecks = k;

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

    // History table
    for (i = k; i < set->nmoves; i++) {
        scores[i] = history[who][set->moves[i].square1][set->moves[i].square2];
    }

    insertion_sort(set->moves, scores, k, set->nmoves);
    return nchecks;
}

/* Same thing, except that here, we only assume capture moves */
static int sort_deltaset_qsearch(struct board* board, char who, struct deltaset* set, move_t * tablemove) {
    int i;
    int scores[256];

    for (i = 0; i < set->nmoves; i++) {
        scores[i] = 0;
        // Found in transposition table
        if (tablemove->piece != -1 && move_equal(*tablemove, set->moves[i]))
            scores[i] = 60000;
        else
            scores[i] = move_see(board, set, &set->moves[i]);
        if (gives_check(board, board_occupancy(board, who) | board_occupancy(board, 1-who), &set->moves[i], who))
            scores[i] += 500;
    }

    insertion_sort(set->moves, scores, 0, set->nmoves);
    return set->nmoves;
}

/* Transforms a mating score to its current context. For example, if we stored "mate on move 10"
 * in the table when we are searching for move 5, when we retrieve the entry when we are searching for move 7,
 * we want to change it to "mate on move */
static int transform_checkmate(struct board* board, union transposition* trans) {
    if (trans->metadata.score > CHECKMATE - 1200) {
        return trans->metadata.score + trans->metadata.age - board->nmoves;
    }
    else if (trans->metadata.score < -CHECKMATE + 1200) {
        return trans->metadata.score - trans->metadata.age + board->nmoves;
    }
    return trans->metadata.score;
}

static int ttable_search(struct board* board, int depth, move_t* best, move_t* move, int * alpha, int beta) {
    union transposition * stored;
    int score;
    move->piece = -1;
    // TODO: do we need ply > 1?
    if (ttable_read(board->hash, &stored) == 0 && position_count_table_read(board->hash) < 1) {
        score = transform_checkmate(board, stored);
        if (stored->metadata.depth >= depth / ONE_PLY) {
            if ((stored->metadata.type & EXACT)) {
                move_copy(best, &stored->move);
                *alpha = score;
                return 0;
            } 
            // The stored score is only an upper bound,
            // so we can only terminate if score is less than the current lower bound
            if ((stored->metadata.type & ALPHA_CUTOFF) && score <= *alpha) {
                *alpha = score;
                return 0;
            }
            // The stored score is only an lower bound,
            // so we can only terminate if score is greater than the current upper bound
            if ((stored->metadata.type & BETA_CUTOFF) && score >= beta) {
                *alpha = score;
                return 0;
            }
        } 
        if (stored->metadata.type & MOVESTORED) {
            move_copy(move, &stored->move);
            return 1;
        }
    }
    return -1;
}

/* Quiescent search: a modified search routine that only considers captures.
 * This is necessary to avoid the horizon effect. Without qsearch, we might search 6 plies deep
 * and be happy about winning a pawn, but if we search 1 ply deeper, we find that we lose our queen!
 * The problem is that the static board evaluation function is only accurate for quiet positions.
 * Hence, when we reach a leaf node of the regular search, we resolve all captures to bring the position
 * to a quiet state before evaluating.
 *
 * Qsearch can search to a much deeper depth than regular search since there are only finitely many captures
 * to make. We also prune away obviously bad captures (queen takes a defended pawn), to further speed up qsearch.
 * We allow non-captures in check positions, because we don't want to miss potential checkmates.
 * Despite pruning and the reduced branching factor, we spend most of our time in qsearch,
 * because it is called at every leaf node of the original search
 */
int qsearch(struct board* board, struct timer* timer, int depth, int alpha, int beta, char who) {
    branches += 1;
    alpha = MAX(alpha, -CHECKMATE + board->nmoves);
    beta = MIN(beta, CHECKMATE - board->nmoves - 1);
    if (alpha >= beta) return alpha;

    struct deltaset out;
    int score = 0;
    int i = 0;
    union transposition * stored;

    // Check if we are out of time. If so, abort
    // Since clock() is costly, don't do this all the time!
    if (branches % 65536 == 0) {
        if (!timer_continue(timer) || signal_stop) {
            out_of_time = 1;
        }
    }

    move_t tablemove;
    move_t best;

    int nmoves = 0;
    generate_captures(&out, board);

    struct deltaset out1;

    nmoves = out.nmoves;

    // Initialize alpha to be the static score.
    // This gives us the option of not doing anything and accepting the board as is.
    // Score the node using the transposition table if posible,
    // and if not, using the static evaluation function
    if (ttable_read(board->hash, &stored) == 0) {
        score = transform_checkmate(board, stored);
        if (!(stored->metadata.type & EXACT) && !((stored->metadata.type & ALPHA_CUTOFF) && score <= alpha) &&
             !((stored->metadata.type & BETA_CUTOFF) && score >=beta)) {
            generate_moves(&out1, board);
            score = board_score(board, who, &out1, alpha, beta);
            if (who) score = -score;
        }
    } else {
        generate_moves(&out1, board);
        score = board_score(board, who, &out1, alpha, beta);
        if (who) score = -score;
    }
    int initial_score = score;

    // Terminal condition: no more moves are left, or we run out of depth
    if (depth < ONE_PLY || nmoves == 0) {
        return score;
    }

    if (initial_score >= beta) return initial_score;

    // Futility pruning: if we are gifted a queen and is still below alpha,
    // assume there's nothing we could possibly do to improve the situation
    // and prune away the subtree
    if (initial_score + 1000 < alpha) return initial_score + 1000;

    alpha = MAX(alpha, initial_score);

    // Look in the transposition table to see if we have seen the position before,
    // and if so, return the score if possible.
    // Even if we can't return the score due to lack of depth,
    // the stored move is probably good, so we can improve the pruning
    tablemove.piece = -1;
    int res = ttable_search(board, depth, &best, &tablemove, &alpha, beta);
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
            if (!pvnode && !out.check && see < 0)
                continue;
            */
        }

        apply_move(board, &out.moves[i]);
        score = -qsearch(board, timer, depth - ONE_PLY, -beta, -alpha, 1 - who);
        reverse_move(board, &out.moves[i]);
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

/* The main search routine.
 * A negamax/pv-search routine.
 * The main gist of the algorithm is, at the given board position, generate all legal moves,
 * and sort them according to some measurement of goodness.
 * The best move is the best move found in previous searches (perhaps to a lower depth than required),
 * then checks, then good captures, then killer moves (moves that are good in sibling nodes),
 * then every thing else.
 * Now, apply the moves in turn and recursively search the resulting position.
 * When we find a move that improves alpha, we assume that all subsequent moves are worse.
 * To prove this, for every subsequent move, we call search with beta = alpha + 1,
 * which prunes away almost all subtrees.
 * If we find that this search returns a score less than alpha + 1, then this means that alpha
 * is indeed an upper bound for that move.
 * If the search returns a score greater than alpha, we have to do a full-window search.
 *
 * There are many details. The most important detail is probably null-pruning. For this,
 * we let the opponent move two times in a row, and if the resulting score is still very good for us,
 * that probably means our position to begin with is really good, so there is no need to search to deeper depth.
 * We also do more unsafe pruning such as futility pruning and late move reductions.
 */
int search(struct board* board, struct timer* timer, move_t* restrict best, move_t* restrict prev,
        int depth, int alpha, int beta, int extensions, int nullmode, char who) {

    // Checkmate pruning: if we found a mate in n in another branch, and we are n+1 away from root,
    // no need to consider the current node
    if (ply != 0) {
        alpha = MAX(alpha, -CHECKMATE + board->nmoves);
        beta = MIN(beta, CHECKMATE - board->nmoves - 1);
        if (alpha >= beta) return alpha;
    }

    struct deltaset out;
    union transposition transposition;
    move_t temp;
    int score = 0;
    int i = 0;
    int extended = 0;
    char pvariation = 1;
    int type = ALPHA_CUTOFF;
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
        if (!timer_continue(timer) || signal_stop) {
            out_of_time = 1;
            ply--;
            return 0;
        }
    }

    int nmoves = 0;

    generate_moves(&out, board);

    nmoves = out.nmoves;

    if (extensions > 0 && !nullmode) {
        if (out.check) {
            if (extensions > 2 * ONE_PLY) {
                extensions -= ONE_PLY;
                depth += ONE_PLY;
                extended = 1;
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
                        depth += ONE_PLY + ONE_PLY/2;
                        extensions -= ONE_PLY + ONE_PLY/2;
                        extended = 1;
                        if (nmoves == 1) {
                            depth += ONE_PLY/2;
                        }
                    }
                }
            }
        }
        else if (nmoves == 1) {
            extensions -= ONE_PLY;
            depth += ONE_PLY;
            extended = 1;
        }
    }
    
    int initial_score = 0;
    // Terminal condition: no more moves are left, or we run out of depth
    if (depth < ONE_PLY || nmoves == 0) {
        if (nmoves == 0) {
            initial_score = board_score(board, who, &out, alpha, beta);
            if (who) initial_score = -initial_score;
            ply--;
            return initial_score;
        }

        score = qsearch(board, timer, 32 * ONE_PLY, alpha, beta, who);
        ply--;
        return score;
    }


    move_t tablemove;
    tablemove.piece = -1;

    // Look in the transposition table to see if we have seen the position before,
    // and if so, return the score if possible.
    // Even if we can't return the score due to lack of depth,
    // the stored move is probably good, so we can improve the pruning
    int res = ttable_search(board, depth, best, &tablemove, &alpha, beta);
    if (res == 0) {
        ply--;
        return alpha;
    }

    initial_score = board_score(board, who, &out, alpha, beta);
    if (who) initial_score = -initial_score;

    // Razoring (TODO: check if a pawn is promotable)
    if (depth < 3 * ONE_PLY && !out.check && nmoves > 1 && beta == alpha + 1 && initial_score + 600 <= alpha && tablemove.piece == -1) {
        score = qsearch(board, timer, 32 * ONE_PLY, alpha, beta, who);
        if (score < alpha - 700) {
            ply--;
            return score;
        }
    }
    
    // Null pruning:
    // If we skip a move, and the move is still bad for the oponent,
    // then our move must have been great
    // We check if we have at least 5 pieces. Otherwise, we might encounter zugzwang
    // TODO: ply > 1?
    if (depth >= 2 * ONE_PLY && nullmode == 0 && !out.check &&
            (board->pieces[who][KNIGHT] | board->pieces[who][BISHOP] | board->pieces[who][ROOK] | board->pieces[who][QUEEN])) {
        uint64_t old_enpassant = board_flip_side(board, 1);
        int rdepth = depth - 3 * ONE_PLY - depth / 4;
        score = -search(board, timer, &temp, NULL, rdepth, -beta, -beta + 1, extensions, 1, 1 - who);
        if (score >= beta) {
            ply--;
            board_flip_side(board, old_enpassant);
            board->enpassant = old_enpassant;
            return score;
        } else {
            // Mate threat extension: if not doing anything allows opponents to checkmate us,
            // we are in a potentially dangerous situation, so extend search
            score = search(board, timer, &temp, NULL, rdepth, CHECKMATE/2 - 1, CHECKMATE/2, extensions, 1, 1 - who);
            board_flip_side(board, old_enpassant);
            board->enpassant = old_enpassant;
            if (score > CHECKMATE/2) {
                extensions -= ONE_PLY/2;
                depth += ONE_PLY;
                extended = 1;
            }
        }
    }

    // Internal iterative depening
    if (beta > alpha + 1 && tablemove.piece == -1 && depth >= 5 * ONE_PLY && !nullmode) {
        search(board, timer, &tablemove, prev, depth - 2 * ONE_PLY, alpha, beta, 0, nullmode, who);
    }

    int nchecks = sort_deltaset(board, who, &out, &tablemove);

    int allow_prune = !out.check && (nmoves > 6) && !extended;
    for (i = 0; i < out.nmoves; i++) {
        move_t * move = &out.moves[i];
        if (out.moves[i].captured != -1 && depth <= 2 * ONE_PLY && allow_prune) {
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
        if (out.moves[i].captured == -1 && out.moves[i].promotion == out.moves[i].piece && i >= nchecks && depth <= 6 * ONE_PLY
                && alpha > -CHECKMATE/2 && beta < CHECKMATE/2 && allow_prune) {
            if (initial_score + futility_margin[depth / ONE_PLY] < alpha) {
                continue;
            }
        }
        // Late move reduction:
        // If we are in a non-pv node, most moves are probably not that great (i.e. can improve upon the value in the pv node)
        // We also sort moves, so that good moves are probably at the front
        // Hence, if the move is non-tactical and appears near the end,
        // it probably isn't as good, so we can search at reduced depth

        apply_move(board, move);
        int skip_deep_search = 0;
        int allow_lmr = allow_prune && depth >= 3 * ONE_PLY && ((i > 2 && beta == alpha + 1));// || (i > 4 && i >= nchecks && out.moves[i].captured == -1
                    //&& out.moves[i].promotion == out.moves[i].piece));
        if (allow_lmr) {
            score = -search(board, timer, &temp, move, depth - 2 * ONE_PLY, -alpha - 1, -alpha, extensions, nullmode, 1 - who);
            if (score <= alpha) {
                skip_deep_search = 1;
                alpha_cutoff_count += 1;
            }
        }

        if (!skip_deep_search) {
            if (pvariation || ply == 1) {
                score = -search(board, timer, &temp, move, depth - ONE_PLY, -beta, -alpha, extensions, nullmode, 1 - who);
            } else {
                score = -search(board, timer, &temp, move, depth - ONE_PLY, -alpha - 1, -alpha, extensions, nullmode, 1 - who);
                if (!out_of_time && score >= alpha && score < beta)
                    score = -search(board, timer, &temp, move, depth - ONE_PLY, -beta, -alpha, extensions, nullmode, 1 - who);
            }
        }
        reverse_move(board, move);
        if (out_of_time) {
            return alpha;
        }
        if (alpha < score) {
            alpha = score;
            move_copy(best, move);
            move_copy(&transposition.move, move);
            type = EXACT | MOVESTORED;
            pvariation = 0;
            history[who][move->square1][move->square2] += depth / ONE_PLY;
        }
        if (beta <= alpha) {
            beta_cutoff_count += 1;
            if (move->captured == -1)
                update_killer(ply, move);
            type = BETA_CUTOFF | MOVESTORED;
            break;
        }
    }

    ply--;
    if (out_of_time) return alpha;

    transposition.metadata.type = type;
    transposition.metadata.age = board->nmoves;
    transposition.metadata.score = alpha;
    transposition.metadata.depth = depth / ONE_PLY;

    if (best->piece != -1)
        assert(move_equal(*best, transposition.move));

    ttable_update(board->hash, &transposition);
    return alpha;
}

int prev_score[2];

move_t find_best_move(struct board* board, struct timer* timer, char who, char flags, char infinite) {
    signal_stop = 0;
    out_of_time = 0;
    int alpha, beta;
    int d = 0, s;
    move_t best, temp;
    best.piece = -1;
    clock_t start = clock();

    alpha_cutoff_count = 0;
    beta_cutoff_count = 0;
    short_circuit_count = 0;
    branches = 0;

    alpha = -INFINITY;
    beta = INFINITY;

    static int leeway_table[32] = {40, 30, 20, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
    };

    timer_start(timer);

    if ((flags & FLAGS_USE_OPENING_TABLE) &&
            opening_table_read(board->hash, &best) == 0) {
        fprintf(stderr, "Applying opening...\n");
        apply_move(board, &best);
        char buffer[8];
        move_to_algebraic(board, buffer, &best);
        out_of_time = 0;
        ply = 0;
        // Analyze this position so that when we leave the opening,
        // we have some entries in the transposition table
        if (infinite) {
            for (d = 6 * ONE_PLY; d < 40 * ONE_PLY; d += ONE_PLY) {
                s = search(board, timer, &temp, NULL, d, alpha, beta, 3 * ONE_PLY, 0 /* null-mode */, 1 - who);
                if (out_of_time) break;
                if (flags & FLAGS_UCI_MODE) {
                    printf("info depth %d ", d / ONE_PLY);
                    printf("score ");
                    if (is_checkmate(s)) {
                        if (s > 0)
                            printf("mate %d ", (1 + CHECKMATE - s - board->nmoves)/2);
                        else
                            printf("mate -%d ", (1 + s + CHECKMATE - board->nmoves)/2);
                    }
                    else {
                        printf("cp %d ", s);
                    }
                    printf("time %lu pv %s", (clock() - start) * 1000 / CLOCKS_PER_SEC, buffer);
                    print_pv(board, d / ONE_PLY);
                    printf("\n");

                }
            }
        } else {
            s = search(board, timer, &temp, NULL, 6 * ONE_PLY, alpha, beta, 3 * ONE_PLY, 0 /* null-mode */, 1 - who);
            if (flags & FLAGS_UCI_MODE) {
                printf("info depth %d ", d / 10);
                printf("score ");
                if (is_checkmate(s)) {
                    if (s > 0)
                        printf("mate %d ", (1 + CHECKMATE - s - board->nmoves)/2);
                    else
                        printf("mate -%d ", (1 + s + CHECKMATE - board->nmoves)/2);
                }
                else {
                    printf("cp %d ", s);
                }
                printf("time %lu pv %s", (clock() - start) * 1000 / CLOCKS_PER_SEC, buffer);
                print_pv(board, d / ONE_PLY);
                printf("\n");
            }
        }
        reverse_move(board, &best);
    } else {
        move_t temp;
        int maxdepth;

        if (flags & FLAGS_DYNAMIC_DEPTH) {
            maxdepth = 40 * ONE_PLY;
        } else {
            maxdepth = 10 * ONE_PLY;
        }

        ply = 0;
        // A depth-4 search should always be accomplishable within the time limit
        s = search(board, timer, &best, NULL, 4 * ONE_PLY, -INFINITY, INFINITY, 0, 0 /* null-mode */, who);
        prev_score[who] = s;
        temp = best;
        // Iterative deepening
        for ( d = 6 * ONE_PLY; d < maxdepth; d += ONE_PLY) {
            // Aspirated search: we hope that the score is between alpha and beta.
            // If so, then we have greatly increased search speed.
            // If not, we have to restart search
            alpha = prev_score[who] - leeway_table[(d-6 * ONE_PLY)/ONE_PLY];
            beta = prev_score[who] + leeway_table[(d-6 * ONE_PLY)/ONE_PLY];
            int changea = leeway_table[(d-ONE_PLY)/ONE_PLY];
            int changeb = leeway_table[(d-ONE_PLY)/ONE_PLY];
            while (1) {
                ply = 0;
                s = search(board, timer, &best, NULL, d, alpha, beta, 5 * ONE_PLY, 0 /* null-mode */, who);
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
                s = prev_score[who];
                break;
            }
            else {
                prev_score[who] = s;
                timer_advise(timer, !move_equal(temp, best));
                temp = best;
                if (flags & FLAGS_UCI_MODE) {
                    printf("info depth %d ", d / ONE_PLY);
                    printf("score ");
                    if (is_checkmate(s)) {
                        if (s > 0)
                            printf("mate %d ", (1 + CHECKMATE - s - board->nmoves)/2);
                        else
                            printf("mate -%d ", (1 + s + CHECKMATE - board->nmoves)/2);
                    }
                    else {
                        printf("cp %d ", s);
                    }
                    printf("time %lu pv", (clock() - start) * 1000 / CLOCKS_PER_SEC);
                    print_pv(board, d / ONE_PLY);
                    printf("\n");
    struct deltaset out;
    generate_moves(&out, board);
    move_t tablemove;
    tablemove.piece = -1;
    sort_deltaset(board, who, &out, &tablemove);
    int i;
    char buffer[8];
    printf("Move sorting: ");
    for (i = 0; i < out.nmoves; i++) {
    union transposition * stored;
            apply_move(board, &out.moves[i]);
            int score=0;
            int asdf=0;
            const char* type = "Unknown";
    if (ttable_read(board->hash, &stored) == 0) {
        score = transform_checkmate(board, stored);
        asdf = stored->metadata.depth;
        if (stored->metadata.type & EXACT)
          type = "Exact";
        if (stored->metadata.type & ALPHA_CUTOFF)
          type = "Lower bound";
        if (stored->metadata.type & BETA_CUTOFF)
          type = "Upper bound";
    }
            reverse_move(board, &out.moves[i]);
      move_to_algebraic(board, buffer, &out.moves[i]);
      printf("%s (%d at depth %d. Type=%s) ", buffer, -score, asdf, type);
    }
    printf("\n");
                }
                if (!infinite && is_checkmate(s)) {
                    break;
                }
            }
        }
    }

    assert(best.piece != -1);

    char buffer[8];
    move_to_calgebraic(board, buffer, &best);

    fprintf(stderr, "Best scoring move is %s: %.2f\n", buffer, s/100.0);
    fprintf(stderr, "Searched %d moves, #alpha: %d, #beta: %d, shorts: %d, depth: %d, TT hits: %.5f\n",
            branches, alpha_cutoff_count, beta_cutoff_count, short_circuit_count, d - 2 * ONE_PLY, tt_hits/((float) tt_tot));
    return best;
}

void search_stop() {
    signal_stop = 1;
}
