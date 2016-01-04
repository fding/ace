#include "board.h"
#include "pieces.h"
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "search.h"

struct state {
    struct board curboard;
    move_t all_moves[1024]; // Move history
    int depth;
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

// Used for draw detection
struct position_count position_count_table[256];

static void position_count_table_update(uint64_t hash) {
    int hash1 = hash & 0xff;
    if (position_count_table[hash1].valid && position_count_table[hash1].hash == hash) {
        position_count_table[hash1].count++;
    } else {
        position_count_table[hash1].valid = 1;
        position_count_table[hash1].hash = hash;
        position_count_table[hash1].count = 1;
    }
}

static int position_count_table_read(uint64_t hash) {
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
            index = rand64() % opening_table[hash1].nvar;
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


#define HASHMASK1 ((1ull << 24) - 1)
#define HASHMASK2 (((1ull << 48) - 1) ^ HASHMASK1)

void transposition_table_update(struct transposition * update) {
    int hash1 = HASHMASK1 & update->hash;
    update->valid = 1;
    if (transposition_table[hash1].valid) {
        if (transposition_table[hash1].hash == update->hash) {
            if (update->depth > transposition_table[hash1].depth)
                transposition_table[hash1] = *update;

        } else {
            transposition_table_size+=1;
            if (transposition_table[hash1].depth < update->depth
                    || transposition_table[hash1].age > update->age + 10)
                transposition_table[hash1] = *update;
        }
    }
    else
        transposition_table[hash1] = *update;
}

int transposition_table_read(uint64_t hash, struct transposition* value) {
    int hash1 = HASHMASK1 & hash;
    if (transposition_table[hash1].valid && transposition_table[hash1].hash == hash)
    {
        assert(transposition_table[hash1].valid == 1);
        *value = transposition_table[hash1];
        return 0;
    }
    return -1;
}

void engine_init(int depth, char flags) {
    rand64_seed(0);
    initialize_lookup_tables();
    board_init(&global_state.curboard);
    if (!(transposition_table = calloc(16777216, sizeof(struct transposition))))
        exit(1);
    if (!(opening_table = calloc(65536, sizeof(struct opening_entry))))
        exit(1);
    if (flags & FLAGS_USE_OPENING_TABLE)
        load_opening_table("openings.acebase");
    global_state.side = 0;
    global_state.current_side = 0;
    global_state.won = 0;
    global_state.depth = depth;
    global_state.flags = flags;

    // Seed the random number generator
    rand64_seed(time(NULL));
}


void engine_init_from_position(char* position, int depth, char flags) {
    rand64_seed(0);
    initialize_lookup_tables();
    board_init_from_fen(&global_state.curboard, position);
    global_state.current_side = global_state.curboard.who;
    if (!(transposition_table = calloc(16777216, sizeof(struct transposition))))
        exit(1);
    if (!(opening_table = calloc(65536, sizeof(struct opening_entry))))
        exit(1);
    if (flags & FLAGS_USE_OPENING_TABLE)
        load_opening_table("openings.acebase");
    global_state.side = 0;
    global_state.won = 0;
    global_state.depth = depth;
    global_state.flags = flags;
    rand64_seed(time(NULL));
}

int engine_score() {
    struct moveset mvs;
    generate_moves(&mvs, &global_state.curboard, global_state.current_side);
    return board_score(&global_state.curboard, global_state.current_side, &mvs, -1);
}

struct board* engine_get_board() {
    return &global_state.curboard;
}

char engine_get_who() {
    return global_state.current_side;
}

int engine_won() {
    return global_state.won;
}

void engine_perft(int depth, int who, uint64_t* count, uint64_t* enpassants, uint64_t* captures, uint64_t* check, uint64_t* promotions, uint64_t* castles) {
    struct moveset mvs;
    struct deltaset out;
    int i;
    int local_count = 0;
    char buffer[8];
    generate_moves(&mvs, &global_state.curboard, who);
    moveset_to_deltaset(&global_state.curboard, &mvs, &out);
    if (depth == 0 && mvs.check) *check += 1;
    for (i = 0; i < out.nmoves; i++) {
        move_to_algebraic(&global_state.curboard, buffer, &out.moves[i]);
        apply_move(&global_state.curboard, who, &out.moves[i]);
        local_count += 1;
        if (out.moves[i].captured != -1) {
            if (depth == 0) *captures += 1;
        }
        if (depth == 0 && (out.moves[i].misc & 0x40))
            *enpassants += 1;
        if (depth == 0 && (out.moves[i].misc & 0x80))
            *castles += 1;
        if (depth == 0 && (out.moves[i].promotion != out.moves[i].piece))
            *promotions += 1;
        if (depth == 0) *count += 1;
        else {
            int oldcount = *count;
            engine_perft(depth - 1, 1 - who, count, enpassants, captures, check, promotions, castles);
        }
        reverse_move(&global_state.curboard, who, &out.moves[i]);
    }
}

static int engine_move_internal(move_t move) {
    struct moveset mvs;

    if (global_state.won) return global_state.won;
    if (is_valid_move(&global_state.curboard, global_state.current_side, move))
        apply_move(&global_state.curboard, global_state.current_side, &move);
    else
        return -1;

    position_count_table_update(global_state.curboard.hash);
    if (position_count_table_read(global_state.curboard.hash) >= 4) {
        // Draw
        global_state.won = 1;
        return 0;
    }

    global_state.all_moves[global_state.curboard.nmoves - 1] = move;
    global_state.current_side = 1-global_state.current_side;

    generate_moves(&mvs, &global_state.curboard, global_state.current_side);
    int nmoves = board_nmoves_accurate(&global_state.curboard, global_state.current_side);

    if (mvs.check && nmoves == 0) 
        global_state.won = 3 - global_state.current_side;
    else if (nmoves == 0 || global_state.curboard.nmovesnocapture >= 100)
        global_state.won = 1;
    
    if (mvs.check) printf("Check!\n");
    return 0;
}

int engine_play() {
    if (global_state.won) return global_state.won;
    struct moveset mvs;
    generate_moves(&mvs, &global_state.curboard, global_state.current_side);
    int nmoves = board_nmoves_accurate(&global_state.curboard, global_state.current_side);
    if (nmoves == 0) {
        if (mvs.check)
            global_state.won = 3 - global_state.current_side;
        else if (nmoves == 0)
            global_state.won = 1;
        return global_state.won;
    }

    clock_t start = clock();

    move_t move = generate_move(&global_state.curboard, global_state.current_side, &global_state.depth, global_state.flags);
    char buffer[8];
    move_to_algebraic(&global_state.curboard, buffer, &move);
    printf("%s\n", buffer);
    fflush(stdout);
    fprintf(stderr, "Spent %lu milliseconds for move %s\n", (clock() - start) * 1000 / CLOCKS_PER_SEC, buffer);
    return engine_move_internal(move);
}

int engine_move(char * buffer) {
    move_t move;
    algebraic_to_move(buffer, engine_get_board(), &move);
    return engine_move_internal(move);
}

extern uint64_t transposition_table_size;
void engine_print() {
    struct board* board = &global_state.curboard;
    char piece;

    fprintf(stderr, "%llu transpositions stored\n", transposition_table_size);
    fprintf(stderr, "Search Depth: %d\n", global_state.depth);
    if (global_state.current_side == 0) {
        fprintf(stderr, "White to move!\n");
    } else {
        fprintf(stderr, "Black to move!\n");
    }
    fprintf(stderr, "Enpassant: %llx\n", board->enpassant);
    uint64_t mask;
    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            mask = 1ull << (8 * i + j);
            if (mask & P2BM(board, WHITEPAWN))
                fprintf(stderr, " P ");
            else if (mask & P2BM(board, WHITEKNIGHT))
                fprintf(stderr, " N ");
            else if (mask & P2BM(board, WHITEBISHOP))
                fprintf(stderr, " B ");
            else if (mask & P2BM(board, WHITEROOK))
                fprintf(stderr, " R ");
            else if (mask & P2BM(board, WHITEQUEEN))
                fprintf(stderr, " Q ");
            else if (mask & P2BM(board, WHITEKING))
                fprintf(stderr, " K ");
            else if (mask & P2BM(board, BLACKPAWN))
                fprintf(stderr, " p ");
            else if (mask & P2BM(board, BLACKKNIGHT))
                fprintf(stderr, " n ");
            else if (mask & P2BM(board, BLACKBISHOP))
                fprintf(stderr, " b ");
            else if (mask & P2BM(board, BLACKROOK))
                fprintf(stderr, " r ");
            else if (mask & P2BM(board, BLACKQUEEN))
                fprintf(stderr, " q ");
            else if (mask & P2BM(board, BLACKKING))
                fprintf(stderr, " k ");
            else
                fprintf(stderr, " . ");
        }
        fprintf(stderr, "\n\n");
    }
    fprintf(stderr, "Hash: %llx\n", board->hash);
}

