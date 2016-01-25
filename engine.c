#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "board.h"
#include "moves.h"
#include "pieces.h"
#include "search.h"

const wchar_t pretty_piece_names[] = L"\x265f\x265c\x265e\x265d\x265b\x265a\x2659\x2656\x2658\x2657\x2655\x2654";

union transposition* ttable;
struct opening_entry* opening_table;
uint64_t ttable_size = 0;

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
struct position_count position_count_table[256];

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
    int hash1 = hash & 0xff;
    if (position_count_table[hash1].valid && position_count_table[hash1].hash == hash) {
        position_count_table[hash1].count++;
    } else {
        position_count_table[hash1].valid = 1;
        position_count_table[hash1].hash = hash;
        position_count_table[hash1].count = 1;
    }
}

int position_count_table_read(uint64_t hash) {
    int hash1 = hash & 0xff;
    if (position_count_table[hash1].valid && position_count_table[hash1].hash == hash)
        return position_count_table[hash1].count;
    return 0;
}

void opening_table_update(uint64_t hash, move_t move, char avoid) {
    struct delta_compressed m = *((struct delta_compressed *) (&move));
    int hash1 = ((1ull << 16) - 1) & hash;

    if (!opening_table[hash1].valid) {
        opening_table[hash1].hash = hash;
        opening_table[hash1].valid = 1;
        opening_table[hash1].nvar = 0;
        opening_table[hash1].avoid = 0;
    }

    int i = 0;
    if (opening_table[hash1].hash == hash) {
        if (opening_table[hash1].nvar < 3) {
            for (i = 0; i < opening_table[hash1].nvar; i++) {
                struct delta_compressed entry = opening_table[hash1].move[i];
                if (entry.square1 == m.square1 && entry.square2 == m.square2
                        && entry.piece == m.piece && entry.captured == m.captured
                        && entry.promotion == m.promotion)
                    return;
            }
            opening_table[hash1].move[opening_table[hash1].nvar] = m;
            opening_table[hash1].avoid |= (avoid << opening_table[hash1].nvar++);
        }
    }
}

int opening_table_read(uint64_t hash, move_t* move) {
    int hash1 = ((1ull << 16) - 1) & hash;
    int index;
    if (opening_table[hash1].valid && opening_table[hash1].hash == hash) {
        if (opening_table[hash1].nvar == 0) return -1;
        while (1) {
            index = rand() % opening_table[hash1].nvar;
            if (!(opening_table[hash1].avoid & (1 << index))) {
                *move = *((move_t *) &opening_table[hash1].move[index]);
                return 0;
            }
        }
    }
    return -1;
}

void load_opening_table(char * fname) {
    FILE * file = fopen(fname, "r");
    fread(opening_table, sizeof(struct opening_entry), 65536, file);
    fclose(file);
}

void save_opening_table(char * fname) {
    FILE * file = fopen(fname, "w");
    fprintf(stderr, "Saving opening table...\n");
    fwrite(opening_table, sizeof(struct opening_entry), 65536, file);
    fclose(file);
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
        if (posix_memalign((void **) &ttable, 64, hashmapsize * sizeof(union transposition))) {
            exit(1);
        }
        memset(ttable, 0, hashmapsize * sizeof(union transposition));
        if (!(opening_table = calloc(65536, sizeof(struct opening_entry))))
            exit(1);
        if (flags & FLAGS_USE_OPENING_TABLE)
            load_opening_table("openings.acebase");
    }
    state.flags = flags;
}

int engine_reset_hashmap(int hashsize) {
    hashmapsize = MSB(hashsize);
    free(ttable);
    if (posix_memalign((void **) &ttable, 64, hashmapsize * sizeof(union transposition))) {
        return 1;
    }
    memset(ttable, 0, hashmapsize * sizeof(union transposition));
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
    memset(ttable, 0, hashmapsize * sizeof(union transposition));
    memset(history, 0, 2 * 64 * 64 * sizeof(int));
}

int engine_score() {
    struct deltaset mvs;
    generate_moves(&mvs, &state.board);
    return board_score(&state.board, state.board.who, &mvs,
            -INFINITY, INFINITY);
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
        uint64_t* check, uint64_t* promotions, uint64_t* castles) {
    struct deltaset mvs;
    int i;
    generate_moves(&mvs, &state.board);
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
            engine_perft(depth, depth - 1, 1 - who, count, enpassants, captures, check, promotions, castles);
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
    if (position_count_table_read(state.board.hash) >= 4) {
        // Draw
        state.won = GAME_DRAW;
        return 0;
    }

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
    return engine_move_internal(move);
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

