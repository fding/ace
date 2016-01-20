#include "board.h"
#include "moves.h"
#include "pieces.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "search.h"

struct state {
    struct board curboard;
    move_t all_moves[1024]; // Move history
    int max_thinking_time;
    char side; // which side the engine is on, 0 for white, 1 for black
    char current_side;
    char won; // 0 for undetermined, 1 for draw, 2 for white, 3 for black
    char flags;
};

struct position_count {
    uint64_t hash;
    char valid;
    char count;
};

uint64_t square_hash_codes[64][12];
uint64_t castling_hash_codes[4];
uint64_t enpassant_hash_codes[8];
uint64_t side_hash_code;


void initialize_hash_codes(void) {
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


// Used for draw detection
struct position_count position_count_table[256];

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

struct state global_state;

struct transposition* transposition_table;
struct opening_entry* opening_table;
uint64_t transposition_table_size = 0;

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


void engine_init(int max_thinking_time, char flags) {
    engine_init_from_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", max_thinking_time, flags);
}

int initialized = 0;

char* engine_init_from_position(char* position, int max_thinking_time, char flags) {
    char * pos;
    if (!initialized) {
        initialized = 1;
        initialize_move_tables();
        initialize_hash_codes();
        if (posix_memalign((void **) &transposition_table, 64, 67108864 * sizeof(struct transposition))) {
            exit(1);
        }
        memset(transposition_table, 0, 67108864 * sizeof(struct transposition));
        if (!(opening_table = calloc(65536, sizeof(struct opening_entry))))
            exit(1);
        if (flags & FLAGS_USE_OPENING_TABLE)
            load_opening_table("openings.acebase");
    }
    memset(position_count_table, 0, sizeof(position_count_table));
    pos = board_init_from_fen(&global_state.curboard, position);
    global_state.current_side = global_state.curboard.who;
    global_state.side = 0;
    global_state.won = 0;
    global_state.max_thinking_time = max_thinking_time;
    global_state.flags = flags;
    srand(time(NULL));
    return pos;
}

int engine_score() {
    struct deltaset mvs;
    generate_moves(&mvs, &global_state.curboard, global_state.current_side);
    return board_score(&global_state.curboard, global_state.current_side, &mvs, -31000, 31000);
}

struct board* engine_get_board() {
    return &global_state.curboard;
}

unsigned char engine_get_who() {
    return global_state.current_side;
}

int engine_won() {
    return global_state.won;
}

void engine_perft(int initial, int depth, int who, uint64_t* count, uint64_t* enpassants, uint64_t* captures, uint64_t* check, uint64_t* promotions, uint64_t* castles) {
    struct deltaset mvs;
    int i;
    generate_moves(&mvs, &global_state.curboard, who);
    char buffer[8];
    uint64_t oldhash;
    if (depth == 0 && mvs.check) *check += 1;
    for (i = 0; i < mvs.nmoves; i++) {
        oldhash = global_state.curboard.hash;
        apply_move(&global_state.curboard, who, &mvs.moves[i]);
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
            move_to_algebraic(&global_state.curboard, buffer, &mvs.moves[i]);
            fprintf(stderr, "%s: %llu\n", buffer, *count - oldcount);
        }
        reverse_move(&global_state.curboard, who, &mvs.moves[i]);
        assert(global_state.curboard.hash == oldhash);
    }
}

static int engine_move_internal(move_t move) {
    struct deltaset mvs;

    if (global_state.won) return global_state.won;
    if (is_valid_move(&global_state.curboard, global_state.current_side, move))
        apply_move(&global_state.curboard, global_state.current_side, &move);
    else {
        return -1;
    }

    position_count_table_update(global_state.curboard.hash);
    if (position_count_table_read(global_state.curboard.hash) >= 4) {
        // Draw
        global_state.won = 1;
        return 0;
    }

    global_state.all_moves[global_state.curboard.nmoves - 1] = move;
    global_state.current_side = 1-global_state.current_side;

    generate_moves(&mvs, &global_state.curboard, global_state.current_side);
    int nmoves = mvs.nmoves;

    if (mvs.check && nmoves == 0) 
        global_state.won = 3 - global_state.current_side;
    else if (nmoves == 0 || global_state.curboard.nmovesnocapture >= 100)
        global_state.won = 1;
    
    if (mvs.check) fprintf(stderr, "Check!\n");
    return 0;
}

int engine_play() {
    if (global_state.won) return global_state.won;
    struct deltaset mvs;
    generate_moves(&mvs, &global_state.curboard, global_state.current_side);
    int nmoves = mvs.nmoves;
    if (nmoves == 0) {
        if (mvs.check)
            global_state.won = 3 - global_state.current_side;
        else if (nmoves == 0)
            global_state.won = 1;
        return global_state.won;
    }

    clock_t start = clock();

    move_t move = find_best_move(&global_state.curboard, global_state.current_side, global_state.max_thinking_time, global_state.flags);
    char buffer[8];
    if (global_state.flags & FLAGS_UCI_MODE) {
        move_to_algebraic(&global_state.curboard, buffer, &move);
        printf("bestmove %s\n", buffer);
    }
    else {
        move_to_calgebraic(&global_state.curboard, buffer, &move);
        printf("%s\n", buffer);
    }
    fflush(stdout);
    fprintf(stderr, "Spent %lu milliseconds for move %s\n", (clock() - start) * 1000 / CLOCKS_PER_SEC, buffer);
    return engine_move_internal(move);
}

int engine_move(char * buffer) {
    move_t move;
    calgebraic_to_move(buffer, engine_get_board(), &move);
    return engine_move_internal(move);
}

extern uint64_t transposition_table_size;

void engine_print() {
    struct board* board = &global_state.curboard;
    char fen[256];
    struct deltaset mvs;
    generate_moves(&mvs, &global_state.curboard, global_state.current_side);

    fprintf(stderr, "%llu transpositions stored\n", transposition_table_size);
    board_to_fen(board, fen);
    fprintf(stderr, "FEN: %s\n", fen);
    int score = board_score(board, global_state.current_side, &mvs, -31000, 31000);
    fprintf(stderr, "Static Board Score: %.2f\n", score / 100.0);
    if (global_state.current_side == 0) {
        fprintf(stderr, "White to move!\n");
    } else {
        fprintf(stderr, "Black to move!\n");
    }
    fprintf(stderr, "Enpassant: %llx\n", board->enpassant);
    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            char piece = get_piece_on_square(board, 8 * i + j);
            if (piece == -1)
                fprintf(stderr, " . ");
            else
                fprintf(stderr, " %c ", piece_names[piece]);
        }
        fprintf(stderr, "\n\n");
    }
    fprintf(stderr, "Hash: %llx\n", board->hash);
}

