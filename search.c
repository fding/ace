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
int main_branches = 0;

extern int evaluation_cache_calls;
extern int evaluation_cache_hits;

// Global state

int ply;
int out_of_time = 0;
int ttable_stored_count = 0;
volatile int signal_stop = 0;

int material_table[6] = {100, 500, 300, 300, 900, 30000};

struct killer_slot {
    move_t mate_killer;
    move_t m1;
    move_t m2;
};

struct killer_slot killer[32];
uint64_t seen[64];
uint32_t history[2][64][64];

static int ttable_search(struct board* restrict board, int who, int depth, move_t* restrict best,
        move_t* restrict move, int * restrict alpha, int beta);
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
            *entry = *update;
        } else {
            if (!secondary_entry->metadata.type) {
                ttable_stored_count++;
                *secondary_entry = *update;
                return;
            }
            if (secondary_entry->metadata.hash == update->metadata.hash) {
                *secondary_entry = *update;
            } else {
                if (update->metadata.depth >= entry->metadata.depth) {
                    *entry = *update;
                } else {
                    if (secondary_entry->metadata.depth >= entry->metadata.depth) {
                        *entry = *secondary_entry;
                    }
                    *secondary_entry = *update;
                }
            }
        }
    } else {
        if (secondary_entry->metadata.type) {
            if (secondary_entry->metadata.hash == update->metadata.hash) {
                *secondary_entry = *update;
            } else {
                *entry = *update;
                ttable_stored_count++;
            }
        } else {
            *entry = *update;
            ttable_stored_count++;
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
    if (ttable[loc].slot2.metadata.hash == sig) {
        *value = &ttable[loc].slot2;
        tt_hits += 1;
        return 0;
    } else if (ttable[loc].slot1.metadata.hash == sig) {
        *value = &ttable[loc].slot1;
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
static void update_killer(int ply, move_t* m, int beta) {
    // TODO: enforce not equal?
    if (beta > CHECKMATE/2) {
        move_copy(&killer[ply].mate_killer, m);
    } else if (!move_equal(*m, killer[ply].m1)) {
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

int no_recap_move_see(struct board* board, move_t* move) {
    return material_table[move->captured] + material_table[move->promotion] - material_table[move->piece];
}

// Static exchange evaluation for a move
int move_see(struct board* board, move_t* move) {
    uint64_t white_occupancy = board_occupancy(board, 0);
    uint64_t black_occupancy = board_occupancy(board, 1);
    uint64_t from = (1ull) << (move->square1);
    int who = (white_occupancy & from) ? 0 : 1;
    uint64_t occupancy = white_occupancy | black_occupancy;

    int gain[32];
    int d = 0;
    uint64_t max_xray = board->pieces[0][PAWN] | board->pieces[1][PAWN] |
                        board->pieces[0][BISHOP] | board->pieces[1][BISHOP] |
                        board->pieces[0][ROOK] | board->pieces[1][ROOK] |
                        board->pieces[0][QUEEN] | board->pieces[1][QUEEN];
    uint64_t attackers = is_attacked(board, 0, occupancy, 0, move->square2) |
        is_attacked(board, 0, occupancy, 1, move->square2);
    int piece = move->piece;

    if (move->captured != -1)
        gain[d] = material_table[move->captured] + material_table[move->promotion] - material_table[move->piece];
    else
        gain[d] = material_table[move->promotion] - material_table[move->piece];

    do {
        d++;
        gain[d] = material_table[piece] - gain[d-1];
        if (MAX(-gain[d-1], gain[d]) < 0)
            break;
        attackers ^= from;
        occupancy ^= from;
        if (from & max_xray) {
            attackers |= (is_attacked_slider(board, 0, occupancy, 0, move->square2) |
                          is_attacked_slider(board, 0, occupancy, 1, move->square2))
                         & occupancy;
        }
        from = get_cheapest_attacker(board, attackers, (d + who) & 1, &piece);
    } while (from);

    while (--d) {
        gain[d-1] = -MAX(-gain[d-1], gain[d]);
    }

    return gain[0];
}

struct sorted_move_iterator {
    int scores[256];
    move_t* moves;
    move_t* move;
    uint64_t undefended;
    int16_t idx;
    uint8_t depth;
    uint8_t end;
    uint8_t sorted_count;
    uint8_t finished_tablemove;
};

#define SORTPHASE0 200000000
#define SORTPHASE1 180000000
#define SORTPHASE2 160000000
#define SORTPHASE3 140000000
#define SORTPHASE4 120000000
#define SORTPHASE5 100000000
#define SORTPHASE6 80000000
#define SORTPHASE7 60000000
#define PHASEGAP 20000000

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
static void sorted_move_iterator_score(struct sorted_move_iterator* move_iter, struct board* board, char who, int phase, int start) {
    for (int i = start; i < move_iter->end; i++) {
        int see = move_see(board, &move_iter->moves[i]);
        int non_queen_promotion = move_iter->moves[i].piece != move_iter->moves[i].promotion && move_iter->moves[i].promotion != QUEEN;
        if (phase == 0) {
            if (move_equal(move_iter->moves[i], killer[ply].mate_killer)) {
                move_iter->scores[i] = SORTPHASE1 + 30000;
                move_iter->sorted_count += 1;
                continue;
            }
            if (see > 0 && !non_queen_promotion) {
                move_iter->scores[i] = SORTPHASE1 + see;
                move_iter->sorted_count += 1;
                continue;
            }
            if (move_equal(move_iter->moves[i], killer[ply].m1)) {
                move_iter->scores[i] = SORTPHASE2;
                move_iter->sorted_count += 1;
                continue;
            }
            if (move_equal(move_iter->moves[i], killer[ply].m2)) {
                move_iter->scores[i] = SORTPHASE3;
                move_iter->sorted_count += 1;
                continue;
            }
            move_iter->scores[i] = 0;
            continue;
        }
        apply_move(board, &move_iter->moves[i]);
        uint64_t occupancy = board_occupancy(board, 0) | board_occupancy(board, 1);
        uint64_t check_move = is_in_check(board, 1 - who, 0, occupancy);
        reverse_move(board, &move_iter->moves[i]);
        int history_score = history[who][move_iter->moves[i].square1][move_iter->moves[i].square2];
        history_score = MIN(history_score, PHASEGAP);

        if (move_iter->moves[i].captured != -1) {
            if (check_move) {
                move_iter->scores[i] = SORTPHASE4 + see;
                move_iter->sorted_count += 1;
                continue;
            }
            if (see == 0) {
                move_iter->scores[i] = SORTPHASE5 + history_score;
                move_iter->sorted_count += 1;
                continue;
            }
        }

        int undefended = move_iter->undefended & (1ull << move_iter->moves[i].square2) ? -material_table[move_iter->moves[i].piece] : 0;
        if (check_move) {
            move_iter->scores[i] = SORTPHASE6 + history_score + undefended;
            move_iter->sorted_count += 1;
            continue;
        }
        move_iter->scores[i] = SORTPHASE7 + history_score + see + undefended;
        move_iter->sorted_count += 1;
    }
}

static int sorted_move_iterator_next(struct sorted_move_iterator* move_iter, struct board* board, char who, move_t * table_move) {
    move_iter->idx += 1;
    if (move_iter->idx >= move_iter->end) {
        return 0;
    }
    if (move_iter->idx == 0 && table_move->piece != -1) {
        move_iter->move = table_move;
        return 1;
    }

    int besti = move_iter->idx;
    if (!move_iter->finished_tablemove) {
        if (table_move->piece != -1) {
            for (int i = 1; i < move_iter->end; i++) {
                if (move_equal(*table_move, move_iter->moves[i])) {
                    move_t tm;
                    move_copy(&tm, &move_iter->moves[i]);
                    move_copy(&move_iter->moves[i], &move_iter->moves[0]);
                    move_copy(&move_iter->moves[0], &tm);
                    break;
                }
            }
            move_iter->sorted_count += 1;
        }
        sorted_move_iterator_score(move_iter, board, who, 0, move_iter->sorted_count);
        move_iter->finished_tablemove = 1;
    }
    if (move_iter->idx >= move_iter->sorted_count) {
        sorted_move_iterator_score(move_iter, board, who, 1, move_iter->sorted_count);
    }
    for (int i = move_iter->idx + 1; i < move_iter->end; i++) {
        if (move_iter->scores[i] > move_iter->scores[besti]) {
            besti = i;
        }
    }

    if (besti == move_iter->idx) {
        move_iter->move = &move_iter->moves[besti];
        return 1;
    }

    move_t tm;
    move_copy(&tm, &move_iter->moves[besti]);
    move_copy(&move_iter->moves[besti], &move_iter->moves[move_iter->idx]);
    move_copy(&move_iter->moves[move_iter->idx], &tm);
    int tmp;
    tmp = move_iter->scores[besti];
    move_iter->scores[besti] = move_iter->scores[move_iter->idx];
    move_iter->scores[move_iter->idx] = tmp;

    move_iter->move = &move_iter->moves[move_iter->idx];
    return 1;
}

static void sorted_move_iterator_init(struct sorted_move_iterator* move_iter, struct board* board, char who, struct deltaset* set, move_t * tablemove, int depth) {
    move_iter->idx = -1;
    move_iter->move = NULL;
    move_iter->moves = set->moves;
    move_iter->end = set->nmoves;
    move_iter->sorted_count = 0;
    move_iter->finished_tablemove = 0;
    move_iter->undefended = set->undefended_squares;
    move_iter->depth = depth;
}


/* Same thing, except that here, we only assume capture moves */
static void sort_deltaset_qsearch(struct board* board, char who, struct deltaset* set, move_t * tablemove) {
    int i;
    int scores[256];

    for (i = 0; i < set->nmoves; i++) {
        scores[i] = 0;
        // Found in transposition table
        if (tablemove->piece != -1 && move_equal(*tablemove, set->moves[i]))
            scores[i] = 60000;
        else
            scores[i] = move_see(board, &set->moves[i]);
    }

    insertion_sort(set->moves, scores, 0, set->nmoves);
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

static int ttable_search(struct board* restrict board, int who, int depth, move_t* restrict best, move_t* restrict move,
                         int* restrict alpha, int beta) {
    union transposition * stored;
    int score;
    move->piece = -1;
    // TODO: do we need ply > 1?
    if (ttable_read(board->hash, &stored) == 0 && position_count_table_read(board->hash) < 1) {
        score = transform_checkmate(board, stored);
        if (stored->metadata.depth >= depth / ONE_PLY) {
            if ((stored->metadata.type & EXACT)) {
                if (is_pseudo_valid_move(board, who, stored->move)) {
                    move_copy(best, &stored->move);
                    *alpha = score;
                    return 0;
                } else {
                    char buffer[8];
                    char board_buffer[128];
                    move_to_calgebraic(board, buffer, &stored->move);
                    board_to_fen(board, board_buffer);
                    fprintf(stderr, "(0) Trying to apply invalid move (%s) on board: %s\n", buffer, board_buffer);
                }
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
            if (is_pseudo_valid_move(board, who, stored->move)) {
                move_copy(move, &stored->move);
                return 1;
            } else {
                char buffer[8];
                char board_buffer[128];
                move_to_calgebraic(board, buffer, &stored->move);
                board_to_fen(board, board_buffer);
                fprintf(stderr, "enpassant: %llx, square2: %llx, eq: %d, piece: %d\n", board->enpassant, (1ull << stored->move.square2), board->enpassant == (1ull << stored->move.square2), stored->move.piece);
                fprintf(stderr, "(1) Trying to apply invalid move (%s) on board: %s\n", buffer, board_buffer);
            }
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
    if (alpha >= beta) {
        return alpha;
    }

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
    tablemove.piece = -1;

    int nmoves = 0;
    generate_qsearch_moves(&out, board);

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
            if (nmoves == 0) {
                generate_moves(&out1, board);
                score = board_score(board, who, &out1, alpha, beta);
            } else {
                score = board_score(board, who, &out, alpha, beta);
            }
            if (who) score = -score;
        }
        if (stored->metadata.type & MOVESTORED) {
            if (is_pseudo_valid_move(board, who, stored->move)) {
                move_copy(&tablemove, &stored->move);
            } else {
                char buffer[8];
                char board_buffer[128];
                move_to_calgebraic(board, buffer, &stored->move);
                board_to_fen(board, board_buffer);
                fprintf(stderr, "enpassant: %llx, square2: %llx, eq: %d, piece: %d\n", board->enpassant, (1ull << stored->move.square2), board->enpassant == (1ull << stored->move.square2), stored->move.piece);
                fprintf(stderr, "(1) Trying to apply invalid move (%s) on board: %s\n", buffer, board_buffer);
            }
        }
    } else {
        if (nmoves == 0) {
            generate_moves(&out1, board);
            score = board_score(board, who, &out1, alpha, beta);
        } else {
            score = board_score(board, who, &out, alpha, beta);
        }
        if (who) score = -score;
    }
    int initial_score = score;

    // Terminal condition: no more moves are left, or we run out of depth
    if (depth < ONE_PLY || (nmoves == 0 && !out.check)) {
        return score;
    }

    if (!out.check) {
        alpha = MAX(alpha, initial_score);
        if (alpha >= beta) return alpha;
    }

    // Futility pruning: if we are gifted a queen and is still below alpha,
    // assume there's nothing we could possibly do to improve the situation
    // and prune away the subtree
    // TODO: Consider case if we have a promotable pawn. That could trigger
    // two queen's worth of swing
    if (initial_score + 950 < alpha) {
        return initial_score + 950;
    }

    sort_deltaset_qsearch(board, who, &out, &tablemove);

    int value = 0;
    int delta_cutoff = 200;
    for (i = 0; i < out.nmoves; i++) {
        // Delta-pruning for quiescent search:
        // If after we capture and don't allow the opponent to respond and we're still more than a minor piece
        // worse than alpha, this capture must really suck, so no need to consider it.
        // We never prune if we are in check, because that could easily result in mistaken evaluation
        // Consider switching off for endgame
        int see = no_recap_move_see(board, &out.moves[i]);
        value = initial_score + see;
        if (value + delta_cutoff < alpha) {
            continue;
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
        if (alpha >= beta) {
            short_circuit_count++;
            return alpha;
        }
    }

    int is_pv_node = beta > alpha + 1;
    int orig_alpha = alpha;

    struct deltaset out;
    union transposition transposition;
    move_t temp;
    int score = 0;
    int i = 0;
    int extended = 0;
    char pvariation = 1;
    int type = ALPHA_CUTOFF;
    branches += 1;
    main_branches += 1;

    // Mark it as invalid, in case we return prematurely (due to time limit)
    best->piece = -1; 

    // Detect cycles, which result in a drawn position
    int min_plies = MAX(0, ply - 2 * board->nmovesnocapture - 1);
    for (i = ply - 4; i >= min_plies; i--) {
        if (board->hash == seen[i]) {
            short_circuit_count++;
            return 0;
        }
    }
    if (position_count_table_read(board->hash) >= 2 && ply > 0) {
        short_circuit_count++;
        return 0;
    }

    seen[ply] = board->hash;
    ply++;

    move_t tablemove;
    tablemove.piece = -1;

    // Look in the transposition table to see if we have seen the position before,
    // and if so, return the score if possible.
    // Even if we can't return the score due to lack of depth,
    // the stored move is probably good, so we can improve the pruning
    int res = ttable_search(board, who, depth, best, &tablemove, &alpha, beta);
    if (res == 0) {
        ply--;
        short_circuit_count++;
        return alpha;
    }

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
            if (prev) {
                reverse_move(board, prev);
                int see = move_see(board, prev);
                apply_move(board, prev);
                if (see >= 0) {
                    depth += ONE_PLY;
                    extensions -= ONE_PLY;
                    extended = 1;
                }
            }
        }
        if (nmoves <= 2) {
            depth += ONE_PLY;
            extensions -= ONE_PLY;
            extended = 1;
        }
    }

    if (!extended && (nmoves == 1)) {
        depth += ONE_PLY;
        extended = 1;
    }
    
    int initial_score = 0;
    // Terminal condition: no more moves are left, or we run out of depth
    if (depth < ONE_PLY || nmoves == 0) {
        if (nmoves == 0) {
            score = board_score(board, who, &out, alpha, beta);
            if (who) score = -score;
        } else {
            score = qsearch(board, timer, 32 * ONE_PLY, alpha, beta, who);
        }
        ply--;
        return score;
    }

    initial_score = board_score(board, who, &out, alpha, beta);
    if (who) initial_score = -initial_score;

    // Null pruning:
    // If we skip a move, and the move is still bad for the oponent,
    // then our move must have been great
    // We check if we have at least 5 pieces. Otherwise, we might encounter zugzwang
    // TODO: ply > 1?
    if (depth >= 2 * ONE_PLY && nullmode == 0 && !out.check &&
            popcnt(board->pieces[who][KNIGHT] | board->pieces[who][BISHOP] | board->pieces[who][ROOK] | board->pieces[who][QUEEN]) >= 3) {
        uint64_t old_enpassant = board_flip_side(board, 1);
        int rdepth = depth - 3 * ONE_PLY - depth / 4;
        score = -search(board, timer, &temp, NULL, rdepth, -beta, -beta + 1, 0, 1, 1 - who);
        if (score >= beta) {
            ply--;
            board_flip_side(board, old_enpassant);
            board->enpassant = old_enpassant;
            short_circuit_count++;
            return score;
        } else {
            // Mate threat extension: if not doing anything allows opponents to checkmate us,
            // we are in a potentially dangerous situation, so extend search
            score = search(board, timer, &temp, NULL, rdepth, CHECKMATE/2 - 1, CHECKMATE/2, 0, 1, 1 - who);
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
    if (is_pv_node && tablemove.piece == -1 && depth >= 6 * ONE_PLY && !nullmode) {
        search(board, timer, &tablemove, prev, depth - depth / 4 - ONE_PLY, alpha, beta, 0, nullmode, who);
    }

    struct sorted_move_iterator iter;
    sorted_move_iterator_init(&iter, board, who, &out, &tablemove, depth / ONE_PLY);

    int allow_prune = !out.check && (nmoves > 6) && !extended;
    int checked_one_capture = 0;
    for (i = 0; i < out.nmoves; i++) {
        sorted_move_iterator_next(&iter, board, who, &tablemove);
        move_t * move = iter.move;
        if (move->captured != -1 && depth <= 2 * ONE_PLY && allow_prune) {
            // Don't consider bad captures
            // TODO: what happens if we end up skipping all legal moves? Should it trigger alpha cutoff?
            if (move_see(board, move) < 0) {
                if (checked_one_capture)
                    continue;
                checked_one_capture = 1;
            }
        }
        // Futility pruning
        // If we reach a frontier node, we don't need to consider non-winning captures and non-checks
        // if the current score of the board is more than a minor piece less than alpha,
        // because our move can only increase the positional scores,
        // which is not that large of a factor
        // TODO: more agressive pruning. If ply>7 and depth<=20, also prune with delta_cutoff=500
        if (move->captured == -1 && move->promotion == move->piece && depth <= 6 * ONE_PLY
                && alpha > -CHECKMATE/2 && beta < CHECKMATE/2 && allow_prune) {
            if (!gives_check(board, board_occupancy(board, 1 - who) | board_occupancy(board, who), move, who)) {
                if (initial_score + futility_margin[depth / ONE_PLY] < alpha) {
                    continue;
                }
            }
        }
        // Late move reduction:
        // If we are in a non-pv node, most moves are probably not that great (i.e. can improve upon the value in the pv node)
        // We also sort moves, so that good moves are probably at the front
        // Hence, if the move is non-tactical and appears near the end,
        // it probably isn't as good, so we can search at reduced depth

        apply_move(board, move);
        int skip_deep_search = 0;
        int allow_lmr = allow_prune && depth >= 4 * ONE_PLY && (((i > 2 && !is_pv_node)) || (i >= 4 && move->captured == -1
                    && move->promotion == move->piece && alpha > -CHECKMATE/2 && beta < CHECKMATE/2));
        if (allow_lmr) {
            uint64_t occupancy = board_occupancy(board, 0) | board_occupancy(board, 1);
            uint64_t check_move = is_in_check(board, 1 - who, 0, occupancy);
            if (!check_move) {
                int reduction = ONE_PLY;
                if (i > 8)
                    reduction = 3 * ONE_PLY;
                else if (i > 4)
                    reduction = 2 * ONE_PLY;

                /*
                if (beta == alpha + 1) {
                    reduction += ONE_PLY;
                }
                */
                if (depth >= 8 * ONE_PLY) {
                    reduction += ONE_PLY;
                }

                score = -search(board, timer, &temp, move, depth - ONE_PLY - reduction, -alpha - 1, -alpha, 0, nullmode, 1 - who);

                if (score <= alpha) {
                    skip_deep_search = 1;
                    alpha_cutoff_count += 1;
                }
            }
        }

        if (!skip_deep_search) {
            if (pvariation || ply == 1) {
                score = -search(board, timer, &temp, move, depth - ONE_PLY, -beta, -alpha, extensions, nullmode, 1 - who);
            } else {
                score = -search(board, timer, &temp, move, depth - ONE_PLY, -alpha - 1, -alpha, 0, nullmode, 1 - who);
                if (!out_of_time && score > alpha && score < beta)
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
        }
        if (beta <= alpha) {
            beta_cutoff_count += 1;
            // TODO: Check if in null-pruning, in pv node? check is capture?
            update_killer(ply, move, alpha);
            history[who][move->square1][move->square2] += (depth / ONE_PLY) * (depth / ONE_PLY);
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

    if (best->piece != -1) {
        if (!move_equal(*best, transposition.move)) {
            printf("%llx\n", *((uint64_t *) best));
            printf("%llx\n", *((uint64_t *) &transposition.move));
            assert(0);
        }
    }

    if (!nullmode)
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
    main_branches = 0;

    alpha = -INFINITY;
    beta = INFINITY;

    tt_hits = 0;
    tt_tot = 0;
    memset(killer, 0, sizeof(killer));

    for (int w = 0; w < 2; w++) {
        for (int sq1 = 0; sq1 < 64; sq1++) {
            for (int sq2 = 0; sq2 < 64; sq2++) {
                history[w][sq1][sq2] = 0;
            }
        }
    }

    static int leeway_table[32] = {40, 40, 35, 35, 30, 30, 30, 30,
        25, 25, 20, 20, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
    };

    timer_start(timer);

    int maxdepth;

    if (flags & FLAGS_DYNAMIC_DEPTH) {
        maxdepth = 100 * ONE_PLY;
    } else {
        maxdepth = 10 * ONE_PLY;
    }

    ply = 0;
    // A depth-4 search should always be accomplishable within the time limit
    s = search(board, timer, &best, NULL, 4 * ONE_PLY, -INFINITY, INFINITY, 0, 0 /* null-mode */, who);
    prev_score[who] = s;
    temp = best;
    // Iterative deepening
    for (d = 6 * ONE_PLY; d < maxdepth; d += ONE_PLY) {
        tt_hits = 0;
        tt_tot = 0;
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
            if (out_of_time)
                break;

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
            }
            if (!infinite && is_checkmate(s)) {
                break;
            }
        }
    }

    assert(best.piece != -1);

    char buffer[8];
    move_to_calgebraic(board, buffer, &best);

    fprintf(stderr, "Best scoring move is %s: %.2f\n", buffer, s/100.0);
    fprintf(stderr, "Searched %d moves (%d main branches), #alpha: %d, #beta: %d, "
                    "shorts: %d, depth: %d, TT hits: %.5f, Eval hits: %.5f, total table usage: %d (out of %d)\n",
            branches, main_branches, alpha_cutoff_count, beta_cutoff_count, short_circuit_count, d / ONE_PLY,
            tt_hits/((float) tt_tot), evaluation_cache_hits / ((float) evaluation_cache_calls), ttable_stored_count);
    return best;
}

void search_stop() {
    signal_stop = 1;
}
