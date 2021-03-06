#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "board.h"
#include "moves.h"
#include "evaluation_parameters.h"
#include "pieces.h"
#include "search.h"
#include "cJSON.h"

const wchar_t pretty_piece_names[] = L"\x265f\x265c\x265e\x265d\x265b\x265a\x2659\x2656\x2658\x2657\x2655\x2654";

struct ttable_entry* ttable;
uint64_t ttable_size = 0;

int hashmapsize = 16777216;

void initialize_endgame_tables();
void load_evaluation_params();
void read_table(int* table, int max, const cJSON* source);

struct {
    struct board board;
    move_t moves[1024]; // Move history
    char won;
    int flags;
} state;

struct position_count {
    uint64_t hash;
    char valid;
    char count;
};

// Position count table: counts the number of time a position has occured in the past
// Used for draw detection
struct position_count position_count_table[1024];

uint64_t square_hash_codes[64][12];
uint64_t castling_hash_codes[4];
uint64_t enpassant_hash_codes[8];
uint64_t side_hash_code;

static void initialize_hash_codes(void) {
    int i, j;
    rand64_seed(0);
    // Initialize hash-codes
    for (i = 0; i < 64; i++) {
        for (j = 0; j < 12; j++) {
            square_hash_codes[i][j] = rand64();
        }
    }
    for (i = 0; i < 4; i++) {
        castling_hash_codes[i] = rand64();
    }
    for (i = 0; i < 8; i++) {
        enpassant_hash_codes[i] = rand64();
    }

    side_hash_code = rand64();
}

void position_count_table_update(uint64_t hash) {
    int hash1 = hash & 0x3ff;
    if (position_count_table[hash1].valid && position_count_table[hash1].hash == hash) {
        position_count_table[hash1].count++;
    } else {
        position_count_table[hash1].valid = 1;
        position_count_table[hash1].hash = hash;
        position_count_table[hash1].count = 1;
    }
}

int position_count_table_read(uint64_t hash) {
    int hash1 = hash & 0x3ff;
    if (position_count_table[hash1].valid && position_count_table[hash1].hash == hash)
        return position_count_table[hash1].count;
    return 0;
}

// The value of a draw.
// Normally, it is 0, but against weaker opponents,
// we can set it to negative, so that even if we are behind,
// we try to win instead of draw
// Currently, it does nothing.
int draw_value = 0;
int engine_set_param(int name, int value) {
    if (name == ACE_PARAM_CONTEMPT) {
        draw_value = value;
        return 0;
    }
    return 1;
}

void engine_init(int flags) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        initialize_move_tables();
        initialize_hash_codes();
        initialize_endgame_tables();
        if (posix_memalign((void **) &ttable, 64, hashmapsize * sizeof(struct ttable_entry))) {
            exit(1);
        }
        memset(ttable, 0, hashmapsize * sizeof(struct ttable_entry));
        load_evaluation_params();
    }
    state.flags = flags;
}

void load_evaluation_params() {
    FILE* f = fopen("params.json", "r");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *params_contents = malloc(fsize + 1);
    fread(params_contents, 1, fsize, f);
    fclose(f);
    params_contents[fsize] = 0;
    cJSON *params = cJSON_Parse(params_contents);
    for (struct cJSON* head = params->child; head != NULL; head = head->next) {
        if (strcmp(head->string, "MIDGAME_PAWN_VALUE") == 0) {
            MIDGAME_PAWN_VALUE = head->valueint;
        } else if (strcmp(head->string, "ENDGAME_PAWN_VALUE") == 0) {
            ENDGAME_PAWN_VALUE = head->valueint;
        } else if (strcmp(head->string, "MIDGAME_KNIGHT_VALUE") == 0) {
            MIDGAME_KNIGHT_VALUE = head->valueint;
        } else if (strcmp(head->string, "MIDGAME_BISHOP_VALUE") == 0) {
            MIDGAME_BISHOP_VALUE = head->valueint;
        } else if (strcmp(head->string, "MIDGAME_ROOK_VALUE") == 0) {
            MIDGAME_ROOK_VALUE = head->valueint;
        } else if (strcmp(head->string, "MIDGAME_QUEEN_VALUE") == 0) {
            MIDGAME_QUEEN_VALUE = head->valueint;
        } else if (strcmp(head->string, "MIDGAME_BISHOP_PAIR") == 0) {
            MIDGAME_BISHOP_PAIR = head->valueint;
        } else if (strcmp(head->string, "MIDGAME_ROOK_PAIR") == 0) {
            MIDGAME_ROOK_PAIR = head->valueint;
        } else if (strcmp(head->string, "HANGING_PIECE_PENALTY") == 0) {
            HANGING_PIECE_PENALTY = head->valueint;
        } else if (strcmp(head->string, "MIDGAME_BACKWARD_PAWN") == 0) {
            MIDGAME_BACKWARD_PAWN = head->valueint;
        } else if (strcmp(head->string, "MIDGAME_SUPPORTED_PAWN") == 0) {
            MIDGAME_SUPPORTED_PAWN = head->valueint;
        } else if (strcmp(head->string, "KNIGHT_OUTPOST_BONUS") == 0) {
            KNIGHT_OUTPOST_BONUS = head->valueint;
        } else if (strcmp(head->string, "KNIGHT_ALMOST_OUTPOST_BONUS") == 0) {
            KNIGHT_ALMOST_OUTPOST_BONUS = head->valueint;
        } else if (strcmp(head->string, "BISHOP_OUTPOST_BONUS") == 0) {
            BISHOP_OUTPOST_BONUS = head->valueint;
        } else if (strcmp(head->string, "BISHOP_ALMOST_OUTPOST_BONUS") == 0) {
            BISHOP_ALMOST_OUTPOST_BONUS = head->valueint;
        } else if (strcmp(head->string, "ROOK_OUTPOST_BONUS") == 0) {
            ROOK_OUTPOST_BONUS = head->valueint;
        } else if (strcmp(head->string, "ROOK_OPENFILE") == 0) {
            ROOK_OPENFILE = head->valueint;
        } else if (strcmp(head->string, "ROOK_SEMIOPENFILE") == 0) {
            ROOK_SEMIOPENFILE = head->valueint;
        } else if (strcmp(head->string, "ROOK_BLOCKED_FILE") == 0) {
            ROOK_BLOCKEDFILE = head->valueint;
        } else if (strcmp(head->string, "ROOK_TARASCH_BONUS") == 0) {
            ROOK_TARASCH_BONUS = head->valueint;
        } else if (strcmp(head->string, "QUEEN_XRAYED") == 0) {
            QUEEN_XRAYED = head->valueint;
        } else if (strcmp(head->string, "KING_XRAYED") == 0) {
            KING_XRAYED = head->valueint;
        } else if (strcmp(head->string, "PINNED_PENALTY") == 0) {
            PINNED_PENALTY = head->valueint;
        } else if (strcmp(head->string, "CFDE_PAWN_BLOCK") == 0) {
            CFDE_PAWN_BLOCK = head->valueint;
        } else if (strcmp(head->string, "KING_SEMIOPEN_FILE_PENALTY") == 0) {
            KING_SEMIOPEN_FILE_PENALTY = head->valueint;
        } else if (strcmp(head->string, "KING_OPEN_FILE_PENALTY") == 0) {
            KING_OPEN_FILE_PENALTY = head->valueint;
        } else if (strcmp(head->string, "CASTLE_OBSTRUCTION_PENALTY") == 0) {
            CASTLE_OBSTRUCTION_PENALTY = head->valueint;
        } else if (strcmp(head->string, "CAN_CASTLE_BONUS") == 0) {
            CAN_CASTLE_BONUS = head->valueint;
        } else if (strcmp(head->string, "TEMPO_BONUS") == 0) {
            TEMPO_BONUS = head->valueint;
        } else if (strcmp(head->string, "doubled_pawn_penalty") == 0) {
            read_table(doubled_pawn_penalty, 8, head->child);
        } else if (strcmp(head->string, "passed_pawn_table") == 0) {
            read_table(passed_pawn_table, 8, head->child);
        } else if (strcmp(head->string, "passed_pawn_table_endgame") == 0) {
            read_table(passed_pawn_table_endgame, 8, head->child);
        } else if (strcmp(head->string, "passed_pawn_blockade_table") == 0) {
            read_table(passed_pawn_blockade_table, 8, head->child);
        } else if (strcmp(head->string, "passed_pawn_blockade_table_endgame") == 0) {
            read_table(passed_pawn_blockade_table_endgame, 8, head->child);
        } else if (strcmp(head->string, "isolated_pawn_penalty") == 0) {
            read_table(isolated_pawn_penalty, 8, head->child);
        } else if (strcmp(head->string, "pawn_table") == 0) {
            read_table(pawn_table, 64, head->child);
        } else if (strcmp(head->string, "pawn_table_endgame") == 0) {
            read_table(pawn_table_endgame, 64, head->child);
        } else if (strcmp(head->string, "knight_table") == 0) {
            read_table(knight_table, 64, head->child);
        } else if (strcmp(head->string, "bishop_table") == 0) {
            read_table(bishop_table, 64, head->child);
        } else if (strcmp(head->string, "rook_table") == 0) {
            read_table(rook_table, 64, head->child);
        } else if (strcmp(head->string, "queen_table") == 0) {
            read_table(queen_table, 64, head->child);
        } else if (strcmp(head->string, "king_table") == 0) {
            read_table(king_table, 64, head->child);
        } else if (strcmp(head->string, "king_table_endgame") == 0) {
            read_table(king_table_endgame, 64, head->child);
        } else if (strcmp(head->string, "king_attacker_table") == 0) {
            read_table(king_attacker_table, 24, head->child);
        } else if (strcmp(head->string, "attack_count_table_rook") == 0) {
            read_table(attack_count_table_rook, 16, head->child);
        } else if (strcmp(head->string, "attack_count_table_queen") == 0) {
            read_table(attack_count_table_queen, 29, head->child);
        } else if (strcmp(head->string, "attack_count_bishop") == 0) {
            read_table(attack_count_table_bishop, 15, head->child);
        } else if (strcmp(head->string, "attack_count_knight") == 0) {
            read_table(attack_count_table_knight, 9, head->child);
        } else if (strcmp(head->string, "bishop_obstruction_table") == 0) {
            read_table(bishop_obstruction_table, 9, head->child);
        } else if (strcmp(head->string, "bishop_own_obstruction_table") == 0) {
            read_table(bishop_own_obstruction_table, 9, head->child);
        } else if (strcmp(head->string, "pawn_shield_table") == 0) {
            read_table(pawn_shield_table, 100, head->child);
        } else if (strcmp(head->string, "pawn_storm_table") == 0) {
            read_table(pawn_storm_table, 100, head->child);
        } else if (strcmp(head->string, "space_table") == 0) {
            read_table(space_table, 32, head->child);
        } else {
            fprintf(stderr, "Unknown parameter: %s\n", head->string);
        }
    }
    cJSON_Delete(params);
    free(params_contents);
}

void read_table(int* table, int max, const cJSON* source) {
    int i = 0;
    for (struct cJSON* head = source; head != NULL; head = head->next) {
        table[i] = head->valueint;
        i++;
        if (i >= max) break;
    }
}

int engine_reset_hashmap(int hashsize) {
    hashmapsize = MSB(hashsize);
    free(ttable);
    if (posix_memalign((void **) &ttable, 64, hashmapsize * sizeof(struct ttable_entry))) {
        return 1;
    }
    memset(ttable, 0, hashmapsize * sizeof(struct ttable_entry));
    return 0;
}

void engine_new_game() {
    engine_new_game_from_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

char* engine_new_game_from_position(char* position) {
    char * pos;
    memset(position_count_table, 0, sizeof(position_count_table));
    pos = board_init_from_fen(&state.board, position);
    state.won = 0;
    srand(time(NULL));
    return pos;
}

void engine_clear_state() {
    memset(position_count_table, 0, sizeof(position_count_table));
    memset(ttable, 0, hashmapsize * sizeof(struct ttable_entry));
    // memset(history, 0, 2 * 64 * 64 * sizeof(int));
}

void engine_print_moves() {
    char buffer[16];
    struct board clean_board;
    board_init(&clean_board);
    for (int i = 0; i < state.board.nmoves; i++) {
        move_to_calgebraic(&clean_board, buffer, &state.moves[i]);
        if (i % 2 == 0)
            printf("%d. ", i / 2 + 1);
        printf("%s ", buffer);
        apply_move(&clean_board, &state.moves[i]);
    }
}

int engine_score() {
    struct deltaset mvs;
    generate_moves(&mvs, &state.board);
    return board_score(&state.board, state.board.who, &mvs,
            -INFINITY, INFINITY);
}

int engine_qsearch_score() {
    struct timer* timer = new_infinite_timer();
    return qsearch(&state.board, timer, 12 * 8, -10000, 10000, state.board.who);
}

struct board* engine_get_board() {
    return &state.board;
}

unsigned char engine_get_who() {
    return state.board.who;
}

int engine_won() {
    return state.won;
}

void engine_perft(int initial, int depth, side_t who,
        uint64_t* count, uint64_t* enpassants, uint64_t* captures,
        uint64_t* check, uint64_t* promotions, uint64_t* castles,
        int eval, int* eval_score) {
    struct deltaset mvs;
    int i;
    generate_moves(&mvs, &state.board);
    if (eval) {
        eval_score += board_score(&state.board, who, &mvs, -30000, 30000);
    }
    char buffer[8];
    uint64_t oldhash;
    if (depth == 0 && mvs.check) *check += 1;
    for (i = 0; i < mvs.nmoves; i++) {
        oldhash = state.board.hash;
        apply_move(&state.board, &mvs.moves[i]);
        if (mvs.moves[i].captured != -1) {
            if (depth == 0) *captures += 1;
        }
        if (depth == 0 && (mvs.moves[i].misc & 0x40))
            *enpassants += 1;
        if (depth == 0 && (mvs.moves[i].misc & 0x80))
            *castles += 1;
        if (depth == 0 && (mvs.moves[i].promotion != mvs.moves[i].piece))
            *promotions += 1;
        int oldcount = *count;
        if (depth == 0) *count += 1;
        else {
            engine_perft(depth, depth - 1, 1 - who, count, enpassants, captures, check, promotions, castles, eval, eval_score);
        }
        if (depth == initial) {
            move_to_algebraic(&state.board, buffer, &mvs.moves[i]);
            fprintf(stderr, "%s: %llu\n", buffer, *count - oldcount);
        }
        reverse_move(&state.board, &mvs.moves[i]);
        assert(state.board.hash == oldhash);
    }
}

static int engine_move_internal(move_t move) {
    struct deltaset mvs;

    if (state.won) return state.won;
    if (is_valid_move(&state.board, state.board.who, move))
        apply_move(&state.board, &move);
    else {
        return -1;
    }

    position_count_table_update(state.board.hash);
    /*
    if (position_count_table_read(state.board.hash) >= 4) {
        // Draw
        state.won = GAME_DRAW;
        return 0;
    }
    */

    state.moves[state.board.nmoves - 1] = move;

    generate_moves(&mvs, &state.board);
    int nmoves = mvs.nmoves;

    // Checkmate
    if (mvs.check && nmoves == 0) {
        if (state.board.who) {
            state.won = GAME_WHITE_WON;
        }
        else {
            state.won = GAME_BLACK_WON;
        }
    }
    // Stalemate
    else if (nmoves == 0 || state.board.nmovesnocapture >= 100)
        state.won = GAME_DRAW;
    
    return 0;
}

int engine_search(char * move, int infinite_mode, int wtime, int btime, int winc, int binc, int moves_to_go) {
    struct deltaset mvs;
    struct timer* timer;

    if (state.won) return state.won;
    generate_moves(&mvs, &state.board);
    int nmoves = mvs.nmoves;
    if (mvs.check && nmoves == 0) {
        if (state.board.who)
            return GAME_BLACK_WON;
        else
            return GAME_WHITE_WON;
    }
    // Stalemate
    else if (nmoves == 0 || state.board.nmovesnocapture >= 100)
        state.won = GAME_DRAW;

    if (infinite_mode)
        timer = new_infinite_timer();
    else
        timer = new_timer(wtime, btime, winc, binc, moves_to_go, state.board.who);

    move_t ret = find_best_move(&state.board, timer, state.board.who, state.flags, infinite_mode);
    if (state.flags & FLAGS_UCI_MODE)
        move_to_algebraic(engine_get_board(), move, &ret);
    else
        move_to_calgebraic(engine_get_board(), move, &ret);

    free(timer);
    return state.won;
}

void engine_stop_search() {
    search_stop();
}

int engine_move(char * buffer) {
    move_t move;
    if (state.flags & FLAGS_UCI_MODE)
        algebraic_to_move(engine_get_board(), buffer, &move);
    else
        calgebraic_to_move(engine_get_board(), buffer, &move);
    int ret = engine_move_internal(move);
    return ret;
}

void engine_print() {
    struct board* board = &state.board;
    char fen[256];
    struct deltaset mvs;
    generate_moves(&mvs, &state.board);

    board_to_fen(board, fen);
    fprintf(stderr, "FEN: %s\n", fen);
    int score = board_score(board, state.board.who, &mvs, -INFINITY, INFINITY);
    fprintf(stderr, "Static Board Score: %.2f\n", score / 100.0);
    if (state.board.who == 0) {
        fprintf(stderr, "White to move!\n");
    } else {
        fprintf(stderr, "Black to move!\n");
    }
    for (int i = 7; i >= 0; i--) {
        fprintf(stderr, "%d", i+1);
        for (int j = 0; j < 8; j++) {
            char piece = get_piece_on_square(board, 8 * i + j);
            if (piece == -1)
                fprintf(stderr, " .");
            else
                fwprintf(stderr, L" %lc", pretty_piece_names[piece]);
        }
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "  a b c d e f g h\n");
}

