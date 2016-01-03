#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>

// Public header file
#include "ace.h"


typedef int16_t piece_t;

#define bmloop(board, square, temp) \
    for (square=LSBINDEX(board), temp=board; temp; temp&=(temp-1), square=LSBINDEX(temp))
#define P2BM(board, piece) (*((*(board)->pieces)+(piece)))

struct board {
    uint64_t pieces[2][6];
    uint64_t enpassant; // location of last advanced-2 pawn
    short nmoves;
    char nmovesnocapture;
    char cancastle; // Castling priviledge
    uint64_t hash;
    char castled;
    char who;
};

#define CHECKMATE 30000
#define INFINITY 31000

struct board;
struct state;

struct moveset_piece {
    uint64_t board;
    char piece;
    char square;
    char castle;
};

void initialize_lookup_tables();
uint64_t rand64();

struct moveset {
    struct moveset_piece moves[18];
    short nmoves;
    char npieces;
    char check;
    char imincheck;
};

struct delta_compressed {
    // A reversible change to board
    char square1;
    char square2;
    char piece;
    char captured;

    char promotion;
    char cancastle; // 4 bit padding, 2 bit white, 2 bit black
    char enpassant; // en-passant square, if any
    char misc; // 1 bit for if move was a castle,
    // 6 bits for last nmovewithoutcapture
};

struct delta {
    // A reversible change to board
    char square1;
    char square2;
    char piece;
    char captured;

    char promotion;
    char cancastle; // 4 bit padding, 2 bit white, 2 bit black
    char enpassant; // en-passant square, if any
    char misc; // 1 bit for if move was a castle,
    // 1 bit for if move was enpassant
    // 6 bits for last nmovewithoutcapture
    uint64_t hupdate;
};

struct transposition {
    uint64_t hash;
    int16_t score;
    char type;
    char valid;
    int16_t age;
    int16_t depth;
    struct delta_compressed move;
};

struct opening_entry {
    uint64_t hash;
    struct delta_compressed move[3];
    char valid;
    char nvar;
    char avoid;
};

void transposition_table_update(struct transposition* update);
int transposition_table_read(uint64_t hash, struct transposition* value);

typedef struct delta move_t;

struct deltaset {
    move_t* moves;
    int nmoves;
};

/* Moveset functions */
void moveset_push(struct moveset* moveset, struct moveset_piece* move);
int moveset_pop(struct moveset* moveset, struct moveset_piece* out);
void moveset_print(struct board* board, struct moveset* mvs);

int moveset_to_deltaset(struct board* board, struct moveset* mvs, struct deltaset* out);

/* Engine functions */
void engine_init(int depth, char flags);
void engine_init_from_position(char* position, int depth);
int engine_play();
int engine_move(char* move);
struct board* engine_get_board();
void engine_print();
char engine_get_who();

/* Board functions */
void board_init(struct board* board);
int board_init_from_fen(struct board* out, char* position);
int board_score(struct board* board, char who, struct moveset* mvs, int nmoves);

int apply_move(struct board* board, char who, struct delta* move);
int reverse_move(struct board* board, char who, move_t* move);

int algebraic_to_move(char* input, struct board* board, move_t* move);
void move_to_algebraic(struct board* board, char* buffer, struct delta* move);

void generate_moves(struct moveset* mvs, struct board* board, char who);

uint64_t board_friendly_occupancy(struct board* board, char who);
uint64_t board_enemy_occupancy(struct board* board, char who);

int board_nmoves_accurate(struct board* board, char who);
uint64_t is_in_check(struct board* board, int who, uint64_t friendly_occupancy, uint64_t enemy_occupancy);


int is_valid_move_with_moveset(struct board* board, char who, struct delta move,
        struct moveset* mvs);

#endif
