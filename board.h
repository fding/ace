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

#define bmloop(board, square, temp) \
    for (square=LSBINDEX(board), temp=board; temp; temp&=(temp-1), square=LSBINDEX(temp))
#define P2BM(board, piece) (*((*(board)->pieces)+(piece)))

#define CHECKMATE 30000
#define INFINITY 31000

extern uint64_t square_hash_codes[64][12];
extern uint64_t castling_hash_codes[4];
extern uint64_t enpassant_hash_codes[8];
extern uint64_t side_hash_code;
extern struct transposition* transposition_table;

typedef int16_t piece_t;

struct board {
    uint64_t pieces[2][6];
    uint64_t enpassant; // location of last advanced-2 pawn
    short nmoves;
    char nmovesnocapture;
    char cancastle; // Castling priviledge
    uint64_t hash;
    char castled;
    unsigned char who;
};

struct delta_compressed {
    // A reversible change to board
    unsigned char square1;
    unsigned char square2;
    char piece;
    char captured;

    char promotion;
    char misc; // 1 bit for if move was a castle,
    char cancastle; // 4 bit padding, 2 bit white, 2 bit black
    char enpassant; // en-passant square, if any
    // 6 bits for last nmovewithoutcapture
};

struct delta {
    // A reversible change to board
    unsigned char square1;
    unsigned char square2;
    char piece; // 3 bits used
    char captured; // 3 bits used

    char promotion; // 3 bits used
    char misc; // 1 bit for if move was a castle,
    char cancastle; // 4 bit padding, 2 bit white, 2 bit black
    char enpassant; // en-passant square, if any
    // 1 bit for if move was enpassant
    // 6 bits for last nmovewithoutcapture
    uint64_t hupdate;
};

typedef struct delta move_t;

/* TODO
 * We can make this more compact as follows:
 * 5 byte hash (upper 5 bytes)
 * 1 byte type, including valid bit
 * 2 byte score
 * 6 byte move (square1, square2, piece, captured, promotion, misc)
 * 1 byte age
 * 1 byte depth
 */
struct transposition {
    uint64_t hash;
    int16_t score;
    char type;
    char valid;
    int16_t age;
    char depth;
    // 16 bytes
    // char move;
    struct delta_compressed move;
    // Make the size of each entry divide 64 for cache performance
    uint64_t padding;
};

struct opening_entry {
    uint64_t hash;
    struct delta_compressed move[3];
    char valid;
    char nvar;
    char avoid;
};

// Output of generate_moves
// It holds a set of legal moves, along with other useful properties
// computed along the way
struct deltaset {
    move_t moves[256];
    short nmoves;
    char check;
    unsigned char who;
    uint64_t pinned; // a bitmap of all pinned pieces
    uint64_t opponent_attacks;
    uint64_t my_attacks;
};

/* Board initialization and serialization */
void board_init(struct board* board);
char * board_init_from_fen(struct board* out, char* position);
void board_to_fen(struct board* out, char* fen);

/* Move initialization and serialization */
int move_equal(move_t m1, move_t m2);
int algebraic_to_move(char* input, struct board* board, move_t* move);
void move_to_algebraic(struct board* board, char* buffer, struct delta* move);

/* Scoring */
int board_score(struct board* board, unsigned char who, struct deltaset* mvs, int alpha, int beta);

/* Moving */
int apply_move(struct board* board, unsigned char who, struct delta* move);
int reverse_move(struct board* board, unsigned char who, move_t* move);
void generate_moves(struct deltaset* mvs, struct board* board, unsigned char who);
void generate_captures(struct deltaset* mvs, struct board* board, unsigned char who);
int is_valid_move(struct board* board, unsigned char who, struct delta move);
uint64_t board_flip_side(struct board* board, uint64_t enpassant);

/* Board properties */
uint64_t board_occupancy(struct board* board, unsigned char who);
int board_npieces(struct board* out, unsigned char who);
uint64_t is_in_check(struct board* board, int who, uint64_t friendly_occupancy, uint64_t enemy_occupancy);
uint64_t is_in_check_slider(struct board* board, int who, uint64_t friendly_occupancy, uint64_t enemy_occupancy);
char get_piece_on_square(struct board* board, int square);
uint64_t attacked_squares(struct board* board, unsigned char who, uint64_t occ);

/* Repeated position detection */
int position_count_table_read(uint64_t hash);
void position_count_table_update(uint64_t hash);

/* Opening tables */
int opening_table_read(uint64_t hash, move_t* move);
void opening_table_update(uint64_t hash, move_t move, char avoid);
void save_opening_table(char * fname);

#endif
