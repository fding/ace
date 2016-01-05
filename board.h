#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>

// Public header file
#include "ace.h"

#include "util.h"

#define AFILE 0x0101010101010101ull
#define HFILE 0x8080808080808080ull
#define RANK1 0x00000000000000ffull
#define RANK2 0x000000000000ff00ull
#define RANK3 0x0000000000ff0000ull
#define RANK6 0x0000ff0000000000ull
#define RANK7 0x00ff000000000000ull
#define RANK8 0xff00000000000000ull


extern uint64_t square_hash_codes[64][12];
extern uint64_t castling_hash_codes[4];
extern uint64_t enpassant_hash_codes[8];
extern uint64_t side_hash_code;

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
    move_t moves[256];
    short nmoves;
    char check;
    char who;
};

/* Board functions */
void board_init(struct board* board);
int board_init_from_fen(struct board* out, char* position);
int board_score(struct board* board, char who, struct deltaset* mvs, int nmoves);

int board_npieces(struct board* out, char who);

int apply_move(struct board* board, char who, struct delta* move);
int reverse_move(struct board* board, char who, move_t* move);

int algebraic_to_move(char* input, struct board* board, move_t* move);
void move_to_algebraic(struct board* board, char* buffer, struct delta* move);

void generate_moves(struct deltaset* mvs, struct board* board, char who);

uint64_t board_friendly_occupancy(struct board* board, char who);
uint64_t board_enemy_occupancy(struct board* board, char who);

int board_nmoves_accurate(struct board* board, char who);
uint64_t is_in_check(struct board* board, int who, uint64_t friendly_occupancy, uint64_t enemy_occupancy);
uint64_t is_in_check_slider(struct board* board, int who, uint64_t friendly_occupancy, uint64_t enemy_occupancy);


int move_equal(move_t m1, move_t m2);


int is_valid_move(struct board* board, char who, struct delta move);
void board_flip_side(struct board* board);


int position_count_table_read(uint64_t hash);
void position_count_table_update(uint64_t hash);

int opening_table_read(uint64_t hash, move_t* move);
void opening_table_update(uint64_t hash, move_t move, char avoid);
void save_opening_table(char * fname);

uint64_t attacked_squares(struct board* board, char who, uint64_t occ);

#endif
