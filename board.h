#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>

// Public header file
#include "ace.h"

#include "util.h"
#include "timer.h"

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

#define CHECKMATE 24000
#define INFINITY 25000


#define CASTLE_PRIV_WQ 0x8
#define CASTLE_PRIV_WK 0x4
#define CASTLE_PRIV_BQ 0x2
#define CASTLE_PRIV_BK 0x1

extern uint64_t square_hash_codes[64][12];
extern uint64_t castling_hash_codes[4];
extern uint64_t enpassant_hash_codes[8];
extern uint64_t side_hash_code;
extern struct ttable_entry* ttable;


extern int hashmapsize;
extern int draw_value;

typedef int16_t piece_t;

/* Data structure for the board state:
 *  1. pieces: an array of 64-bit bitmaps for all the pieces.
 *    For example, pieces[WHITE][KNIGHT] holds the bitmap
 *    for the location of the white knight.
 *  2. enpassant: a bitmap for the location of the last
 *    pawn advanced 2 squares, which is eligible for en-passant capture
 *  3. nmoves: the number of half moves that has elapsed
 *  4. nmovesnocapture: the number of half moves that elapsed without
 *    any pawn advances or captures, necessary for draw detection.
 *  5. cancastle: the castling privileges
 *  6. hash: the Zobrist 64-bit hash of the board
 *  7. castled: indicator for if either side has castled
 *  8. who: the side to move
 */ 
struct board {
    uint64_t pieces[2][6];
    uint64_t enpassant;
    uint64_t hash;
    uint64_t pawn_hash;
    short nmoves;
    char nmovesnocapture;
    char cancastle;
    char castled;
    side_t who;
    uint8_t kingsq[2];
};

/* Data structure for move.
 * Stores all the metadata necessary for a quick reversible change of the board:
 *  1. square1: the original square of the piece
 *  2. square2: the destination square of the piece
 *  3. piece: the piece (PAWN, KNIGHT, ...) that was moved
 *  4. captured: if applicable, the piece (PAWN, ...) that was captured, if not, -1
 *  5. promotion: what the piece becomes at the end of the move. Usually, promotion == piece,
 *      but pawns moving to the 8-th rank will have promotion = {KNIGHT, BISHOP, ROOK, QUEEN}
 *  6. misc: the top bit (0x80 & misc) is set if the move was castling, and the second
 *      top bit (0x40 & misc) is set if the move was an en-passant capture
 *  
 *  The remaining bits are used to store necessary metadata for the move to be reversed.
 *  These are set in apply_move and used in reverse_move
 *  7. misc: the lower 6 bits (0x3f & misc) is set to the previous number of half-moves
 *      that passed without capture or pawn pushes.
 *  8. cancastle: the lower 4 bits hold the castling privileges before the move.
 *  9. enpassant: the square of the pawn eligible for enpassant capture before the move.
 *  10. hupdate: the diff of hash codes, that can be xor-ed into the move
 *
 * The entire data structure fits in 16 bytes, and the important parts of the move
 * (i.e. the real data of the move, as opposed to stored state)
 * fits in 8 bytes. The size of the struct and the exact ordering of the first
 * six elements is extremely important.
 * DO NOT CHANGE UNLESS YOU KNOW WHAT YOU ARE DOING.
 */
struct delta {
    unsigned char square1; // Needs 6 bits
    unsigned char square2; // Needs 6 bits
    char piece; // Needs 3 bits
    char captured; // Needs 3 bits

    char promotion; // Needs 2 bits
    char misc; // Needs 8 bits, only 2 bits need to be stored
    char cancastle; // Needs 4 bits
    char enpassant; // Needs 3 bits, maybe 4
    uint64_t hupdate;
};

/* A smaller version of struct delta that only holds the important data.
 * This is useful for storing moves persistently in memory.  This can't be fed
 * into apply_move, since apply_move has to store the diff of the hash code
 * somewhere.  To convert between struct delta and struct delta_compressed, use
 * the move_copy macro, which basically casts a struct delta pointer to a
 * struct delta_compressed pointer (since these two structs have the same
 * ordering of elements). Hopefully, the compiler is smart enough to generate
 * this as a single 64 bit move instruction 
 */

struct delta_compressed {
    unsigned char square1;
    unsigned char square2;
    char piece;
    char captured;

    char promotion;
    char misc;
    char cancastle;
    char enpassant;
};

typedef struct delta move_t;

// We only need to copy the first 8 bytes of m2 to m1, and this can be done in one operation
#define move_copy(m1, m2) (*((struct delta_compressed *) (m1)) = *((struct delta_compressed *) (m2)))


/* An entry in the transposition table This data structure is of size 16 bytes,
 * so that exactly 4 transpositions fit in a single cache slot. When
 * allocating the transposition table, we use posix_memalign to get a 64 byte
 * aligned pointer, so that all entries of the transposition fit in a single
 * cache line.
 *
 * The first 6 bytes of the transposition table is reserved for the stored move.
 * We store a 16-bit signed integer for the score, a 32-bit hash
 * (equal to the upper 32-bits of the 64-bit Zobrist hash),
 * type (ALPHA_CUTOFF, BETA_CUTOFF, EXACT, MOVESTORED),
 * depth of the search, and age (the number of half-moves that
 * have passed in the game at the time of search).
 * If the hash table has size 2^(2n) for some n,
 * then we use the lower 2n bits of the Zobrist hash to index into the
 * transposition table, and the upper 32 bits to guard against collisions.
 * Then, the probability that both the lower 2n bits and upper 32 bits
 * coincide is small if the number of entries in the hash table,
 * 2^(2n), is less than 2^(n + 16).
 * This is true if n < 16, or 2n < 32.
 * Since we support hash tables of size at most 8 GB,
 * 2n is always at most 29, so the probability of collision is small.
 */
union transposition {
    move_t move; // Currently 64 bits, can reduce to 32
    struct {
        unsigned char square1;
        unsigned char square2;
        char piece;
        char captured;
        char promotion;
        char misc;
        int16_t score;
        // 8 byte alignment
        uint32_t hash; // 12 bits??
        char type; // Needs 3 bits
        uint8_t depth; // Needs 8 bits
        int16_t age; // Needs 9 bits
    } metadata;
}; // Currently 128 bits

struct ttable_entry {
    union transposition slot1;
    union transposition slot2;
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
    move_t moves[228]; // 256
    short nmoves;
    char check;
    side_t who;
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
int algebraic_to_move(struct board* board, char* input, move_t* move);
void move_to_algebraic(struct board* board, char* buffer, move_t* move);
void calgebraic_to_move(struct board* board, char* input, move_t* move);
void move_to_calgebraic(struct board* board, char* buffer, move_t* move);

/* Scoring */
int board_score(struct board* board, side_t who, struct deltaset* mvs, int alpha, int beta);

/* Moving */
int apply_move(struct board* board, move_t* move);
int reverse_move(struct board* board, move_t* move);
void generate_moves(struct deltaset* mvs, struct board* board);
void generate_captures(struct deltaset* mvs, struct board* board);
int is_valid_move(struct board* board, side_t who, struct delta move);
int is_pseudo_valid_move(struct board* board, side_t who, struct delta move);
uint64_t board_flip_side(struct board* board, uint64_t enpassant);

/* Board properties */
uint64_t board_occupancy(struct board* board, side_t who);
int board_npieces(struct board* out, side_t who);

uint64_t is_attacked(struct board* board, uint64_t friendly_occupancy, uint64_t enemy_occupancy, side_t who, int square);
uint64_t is_attacked_slider(struct board* board, uint64_t friendly_occupancy, uint64_t enemy_occupancy, side_t who, int square);
uint64_t get_cheapest_attacker(struct board* board, uint64_t attackers, int who, int* piece);

uint64_t is_in_check(struct board* board, side_t who, uint64_t friendly_occupancy, uint64_t enemy_occupancy);
uint64_t is_in_check_slider(struct board* board, side_t who, uint64_t friendly_occupancy, uint64_t enemy_occupancy);
char get_piece_on_square(struct board* board, int square);
uint64_t attacked_squares(struct board* board, side_t who, uint64_t occ);
int gives_check(struct board * board, uint64_t occupancy, move_t* move, side_t who);

/* Repeated position detection */
int position_count_table_read(uint64_t hash);
void position_count_table_update(uint64_t hash);

/* Opening tables */
int opening_table_read(uint64_t hash, move_t* move);
void opening_table_update(uint64_t hash, move_t move, char avoid);
void save_opening_table(char * fname);

#endif
