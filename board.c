#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pieces.h"

uint64_t square_hash_codes[64][12];
uint64_t castling_hash_codes[4];
uint64_t enpassant_hash_codes[8];
uint64_t side_hash_code;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * UTILITY CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define LSB(u) ((u) & -(u))
#define LSBINDEX(u) (__builtin_ctzll(u))
#define MSB(u) (0x8000000000000000ull >> __builtin_clzll(u))

#define AFILE 0x0101010101010101ull
#define HFILE 0x8080808080808080ull
#define RANK1 0x00000000000000ffull
#define RANK2 0x000000000000ff00ull
#define RANK3 0x0000000000ff0000ull
#define RANK6 0x0000ff0000000000ull
#define RANK7 0x00ff000000000000ull
#define RANK8 0xff00000000000000ull

#define NORTH 0
#define SOUTH 1
#define EAST 2
#define WEST 3
#define NORTHEAST 4
#define NORTHWEST 5
#define SOUTHEAST 6
#define SOUTHWEST 7

void copy_board(struct board* out, struct board* in) {
    memcpy(out->pieces, in->pieces, sizeof(out->pieces));
}

char get_piece_on_square(struct board* board, int square);

void board_init(struct board* out) {
    if (board_init_from_fen(out, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"))
        exit(1);
}

void board_flip_side(struct board* board) {
    board->who = 1 - board->who;
    board->hash ^= side_hash_code;
}

int board_init_from_fen(struct board* out, char* position) {
    /* Format of position:
     * r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -
     */
    int i;
    int rank = 7;
    int file = 0;
    int square, n;
    uint64_t mask;

    out->nmoves = 0;
    out->nmovesnocapture = 0;
    out->castled = 0;
    out->cancastle = 0;
    out->hash = 0;

    for (i = 0; i < 12; i++)
        P2BM(out, i) = 0;

    while (*position) {
        if (rank > 7 || rank < 0 || file > 7 || file < 0) return 1;
        square = 8 * rank + file;
        mask = 1ull << square;
        switch (*position) {
            case 'r':
                out->pieces[1][ROOK] |= mask;
                break;
            case 'R':
                out->pieces[0][ROOK] |= mask;
                break;
            case 'n':
                out->pieces[1][KNIGHT] |= mask;
                break;
            case 'N':
                out->pieces[0][KNIGHT] |= mask;
                break;
            case 'b':
                out->pieces[1][BISHOP] |= mask;
                break;
            case 'B':
                out->pieces[0][BISHOP] |= mask;
                break;
            case 'q':
                out->pieces[1][QUEEN] |= mask;
                break;
            case 'Q':
                out->pieces[0][QUEEN] |= mask;
                break;
            case 'k':
                out->pieces[1][KING] |= mask;
                break;
            case 'K':
                out->pieces[0][KING] |= mask;
                break;
            case 'p':
                out->pieces[1][PAWN] |= mask;
                break;
            case 'P':
                out->pieces[0][PAWN] |= mask;
                break;
            case '/':
                break;
            default:
                n = *position - '0';
                if (n < 0 || n > 9) {
                    return 1;
                }
                file += n - 1;
        }
        if (*position != '/')
            file += 1;
        if (file == 8 && rank == 0) break;
        if (file == 8) {
            rank -= 1;
            file = 0;
        }
        position++;
    }

    if (*position == 0) return 1;

    if (*(++position) != ' ') return 1;
    position++;

    if (*position == 'w')
        out->who = 0;
    else if (*position == 'b'){
        out->who = 1;
        out->hash ^= side_hash_code;
    } else
        return 1;

    if (*(++position) != ' ') return 1;
    position++;

    while (*position) {
        switch (*position) {
            case 'K':
                out->cancastle |= 0x4;
                out->hash ^= castling_hash_codes[1];
                break;
            case 'Q':
                out->cancastle |= 0x8;
                out->hash ^= castling_hash_codes[0];
                break;
            case 'k':
                out->cancastle |= 0x1;
                out->hash ^= castling_hash_codes[3];
                break;
            case 'q':
                out->cancastle |= 0x2;
                out->hash ^= castling_hash_codes[2];
                break;
            case '-':
                break;
            default:
                return 1;
        }
        if (*(++position) == ' ') break;
    }
    if (*position == 0) return 1;
    position++;
    if (*position == 0) return 1;
    if (*position != '-') {
        if (*(position + 1) == 0) return 1;
        int rank, file;
        file = *position - 'a';
        rank = *position - '1';
        if (out->who) rank += 1;
        else rank -= 1;
        out->enpassant = 1ull << (rank * 8 + file);
        out->hash ^= enpassant_hash_codes[file];
        position++;
    }

    if (*(++position) != ' ') return 1;
    position++;
    int a, b;
    if (sscanf(position, "%d %d", &a, &b) < 2)
        return 1;

    out->nmovesnocapture = a;
    out->nmoves = 2 * (b - 1) + out->who;

    for (square = 0; square < 64; square++) {
        char piece = get_piece_on_square(out, square);
        if (piece != -1) {
            out->hash ^= square_hash_codes[square][piece];
        }
    }

    return 0;
}

int bitmap_count_ones(uint64_t bmap) {
    int count;
    count = 0;
    while (bmap) {
        bmap &= (bmap - 1);
        count ++;
    }
    return count;
}

char get_piece_on_square(struct board* board, int square) {
    int who;
    uint64_t mask = (1ull << square);
    for (who = 0; who < 2; who++) {
        if (mask & board->pieces[who][PAWN])
            return (who * 6 + PAWN);
        else if (mask & board->pieces[who][KNIGHT])
            return (who * 6 + KNIGHT);
        else if (mask & board->pieces[who][BISHOP])
            return (who * 6 + BISHOP);
        else if (mask & board->pieces[who][ROOK])
            return (who * 6 + ROOK);
        else if (mask & board->pieces[who][QUEEN])
            return (who * 6 + QUEEN);
        else if (mask & board->pieces[who][KING])
            return (who * 6 + KING);
    }
    return -1;
}

int algebraic_to_move(char* input, struct board* board, struct delta* move) {
    int rank1, rank2, file1, file2;
    rank1 = input[1] - '1';
    rank2 = input[3] - '1';
    file1 = input[0] - 'a';
    file2 = input[2] - 'a';

    move->square1 = rank1 * 8 + file1;
    move->square2 = rank2 * 8 + file2;
    move->captured = -1;
    move->cancastle = 0;
    move->enpassant = 0;
    move->misc = 0;

    char piece1 = get_piece_on_square(board, move->square1) % 6;
    if (piece1 == -1) return -1;

    char piece2 = get_piece_on_square(board, move->square2) % 6;
    if (piece2 != -1)
        move->captured = piece2;
    // en passant
    else if (piece1 == PAWN && file2 != file1) {
        move->captured = PAWN;
        move->misc |= 0x40;
    }

    move->piece = piece1;
    move->promotion = move->piece;
    
    if (input[4] != 0) {
        if (input[4] == 'C') {
            move->misc |= 0x80;
        }
        else {
            switch (input[4]) {
                case 'R':
                    move->promotion = ROOK;
                    break;
                case 'N':
                    move->promotion = KNIGHT;
                    break;
                case 'B':
                    move->promotion = BISHOP;
                    break;
                case 'Q':
                    move->promotion = QUEEN;
                    break;
                default:
                    return -1;
            }
        }
    }

    return 0;
}

void move_to_algebraic(struct board* board, char* buffer, struct delta* move) {
    int rank1 = move->square1 / 8;
    int rank2 = move->square2 / 8;
    int file1 = move->square1 % 8;
    int file2 = move->square2 % 8;
    /*
    switch (move->piece) {
        case PAWN:
            buffer[0] = 'P';
            break;
        case KNIGHT:
            buffer[0] = 'N';
            break;
        case BISHOP:
            buffer[0] = 'B';
            break;
        case ROOK:
            buffer[0] = 'R';
            break;
        case QUEEN:
            buffer[0] = 'Q';
            break;
        case KING:
            buffer[0] = 'K';
            break;
    }
    */
    buffer[0] = 'a' + file1;
    buffer[1] = '1' + rank1;
    buffer[2] = 'a' + file2;
    buffer[3] = '1' + rank2;
    buffer[4] = 0;
    if (move->misc & 0x80) {
        buffer[4] = 'C';
        if (file2 == 2)
            buffer[5] = 'a';
        else
            buffer[5] = 'h';
        buffer[6] = '1' + rank2;
        buffer[7] = 0;
    }
    if (move->promotion != move->piece) {
        switch (move->promotion) {
            case PAWN:
                buffer[4] = 'P';
                break;
            case KNIGHT:
                buffer[4] = 'N';
                break;
            case BISHOP:
                buffer[4] = 'B';
                break;
            case ROOK:
                buffer[4] = 'R';
                break;
            case QUEEN:
                buffer[4] = 'Q';
                break;
            case KING:
                buffer[4] = 'K';
                break;
        }
        buffer[5] = 0;
    }
}

void moveset_push(struct moveset* moveset, struct moveset_piece* move) {
    assert(moveset->npieces < 18);
    moveset->moves[moveset->npieces] = *move;
    moveset->nmoves += bitmap_count_ones(move->board);
    moveset->npieces += 1;
}

int moveset_pop(struct moveset* moveset, struct moveset_piece* out) {
    if (moveset->npieces == 0) return -1;
    *out = moveset->moves[moveset->npieces];
    moveset->npieces -= 1;
    moveset->nmoves -= bitmap_count_ones(out->board);
    return 0;
}

int moveset_to_deltaset(struct board* board, struct moveset* mvs, struct deltaset *out) {
    out->nmoves = mvs->nmoves;
    out->moves = malloc(sizeof(move_t) * mvs->nmoves);
    if (!out->moves) return -1;
    int i, j, k, square;
    uint64_t temp, mask;
    i = 0;
    for (j = 0; j < mvs->npieces; j++) {
        if (mvs->moves[j].castle) {
            out->moves[i].misc = 0x80;
            out->moves[i].enpassant = 0;
            out->moves[i].cancastle = 0;
            out->moves[i].piece = KING;
            out->moves[i].promotion = KING;
            out->moves[i].captured = -1;
            out->moves[i].square2 = LSBINDEX(mvs->moves[j].board);
            out->moves[i].square1 = mvs->moves[j].square;
            i++;
        } else {
            bmloop(mvs->moves[j].board, square, temp) {
                mask = 1ull << square;
                if (mvs->moves[j].piece == PAWN &&
                        ((1ull << square) & (RANK1 | RANK8))) {
                    for (k = ROOK; k < KING; k++) {
                        out->moves[i].misc = 0;
                        out->moves[i].enpassant = 0;
                        out->moves[i].cancastle = 0;
                        out->moves[i].promotion = k;
                        out->moves[i].piece = mvs->moves[j].piece;
                        out->moves[i].captured = -1;
                        out->moves[i].square2 = square;
                        out->moves[i].square1 = mvs->moves[j].square;

                        if (mask & (P2BM(board, WHITEPAWN) | P2BM(board, BLACKPAWN)))
                            out->moves[i].captured = PAWN;
                        else if (mask & (P2BM(board, WHITEKNIGHT) | P2BM(board, BLACKKNIGHT)))
                            out->moves[i].captured = KNIGHT;
                        else if (mask & (P2BM(board, WHITEBISHOP) | P2BM(board, BLACKBISHOP)))
                            out->moves[i].captured = BISHOP;
                        else if (mask & (P2BM(board, WHITEROOK) | P2BM(board, BLACKROOK)))
                            out->moves[i].captured = ROOK;
                        else if (mask & (P2BM(board, WHITEQUEEN) | P2BM(board, BLACKQUEEN)))
                            out->moves[i].captured = QUEEN;

                        i++;
                    }
                } else {
                    out->moves[i].misc = 0;
                    out->moves[i].enpassant = 0;
                    out->moves[i].cancastle = 0;
                    out->moves[i].promotion = mvs->moves[j].piece;
                    out->moves[i].piece = mvs->moves[j].piece;
                    out->moves[i].captured = -1;
                    out->moves[i].square2 = square;
                    out->moves[i].square1 = mvs->moves[j].square;

                    if (mask & (P2BM(board, WHITEPAWN) | P2BM(board, BLACKPAWN)))
                        out->moves[i].captured = PAWN;
                    else if (mask & (P2BM(board, WHITEKNIGHT) | P2BM(board, BLACKKNIGHT)))
                        out->moves[i].captured = KNIGHT;
                    else if (mask & (P2BM(board, WHITEBISHOP) | P2BM(board, BLACKBISHOP)))
                        out->moves[i].captured = BISHOP;
                    else if (mask & (P2BM(board, WHITEROOK) | P2BM(board, BLACKROOK)))
                        out->moves[i].captured = ROOK;
                    else if (mask & (P2BM(board, WHITEQUEEN) | P2BM(board, BLACKQUEEN)))
                        out->moves[i].captured = QUEEN;
                    // En-passant
                    else if (mvs->moves[j].piece == PAWN && (square - mvs->moves[j].square) % 8 != 0 && board->enpassant != 1) {
                        out->moves[i].captured = PAWN;
                    }

                    i++;
                }
            }
        }
    }
    return 0;
}

void moveset_print(struct board* board, struct moveset* mvs) {
    int i;
    char buffer[8];
    struct deltaset ds;
    moveset_to_deltaset(board, mvs, &ds);
    for (i = 0; i < ds.nmoves; i++) {
        move_to_algebraic(board, buffer, &ds.moves[i]);
        fprintf(stderr, "%s ", buffer);
    }
    free(ds.moves);
    fprintf(stderr, "\n");
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * MOVEMENT CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

uint64_t knight_move_table[64] = {
    0x0000000000020400ull, 0x0000000000050800ull, 0x00000000000a1100ull, 0x0000000000142200ull, 0x0000000000280440ull, 0x0000000000508800ull, 0x0000000000a01000ull, 0x0000000000402000ull,
    0x0000000002040004ull, 0x0000000005080008ull, 0x000000000a110011ull, 0x0000000014220022ull, 0x0000000028440044ull, 0x0000000050880088ull, 0x00000000a0100010ull, 0x0000000040200020ull,
    0x0000000204000402ull, 0x0000000508000805ull, 0x0000000a1100110aull, 0x0000001422002214ull, 0x0000002844004428ull, 0x0000005088008850ull, 0x000000a0100010a0ull, 0x0000004020002040ull,
    0x0000020400040200ull, 0x0000050800080500ull, 0x00000a1100110a00ull, 0x0000142200221400ull, 0x0000284400442800ull, 0x0000508800885000ull, 0x0000a0100010a000ull, 0x0000402000204000ull,
    0x0002040004020000ull, 0x0005080008050000ull, 0x000a1100110a0000ull, 0x0014220022140000ull, 0x0028440044280000ull, 0x0050880088500000ull, 0x00a0100010a00000ull, 0x0040200020400000ull,
    0x0204000402000000ull, 0x0508000805000000ull, 0x0a1100110a000000ull, 0x1422002214000000ull, 0x2844004428000000ull, 0x5088008850000000ull, 0xa0100010a0000000ull, 0x4020002040000000ull,
    0x0400040200000000ull, 0x0800080500000000ull, 0x1100110a00000000ull, 0x2200221400000000ull, 0x4400442800000000ull, 0x8800885000000000ull, 0x100010a000000000ull, 0x2000204000000000ull,
    0x0004020000000000ull, 0x0008050000000000ull, 0x00110a0000000000ull, 0x0022140000000000ull, 0x0044280000000000ull, 0x0088500000000000ull, 0x0010a00000000000ull, 0x0020400000000000ull
};

uint64_t king_move_table[64] = {
    0x0000000000000302ull, 0x0000000000000705ull, 0x0000000000000e0aull, 0x0000000000001c14ull, 0x0000000000003828ull, 0x0000000000007050ull, 0x000000000000e0a0ull, 0x000000000000c040ull,
    0x0000000000030203ull, 0x0000000000070507ull, 0x00000000000e0a0eull, 0x00000000001c141cull, 0x0000000000382838ull, 0x0000000000705070ull, 0x0000000000e0a0e0ull, 0x0000000000c040c0ull,
    0x0000000003020300ull, 0x0000000007050700ull, 0x000000000e0a0e00ull, 0x000000001c14100cull, 0x0000000038283800ull, 0x0000000070507000ull, 0x00000000e0a0e000ull, 0x00000000c040c000ull,
    0x0000000302030000ull, 0x0000000705070000ull, 0x0000000e0a0e0000ull, 0x0000001c1410000cull, 0x0000003828380000ull, 0x0000007050700000ull, 0x000000e0a0e00000ull, 0x000000c040c00000ull,
    0x0000030203000000ull, 0x0000070507000000ull, 0x00000e0a0e000000ull, 0x00001c141000000cull, 0x0000382838000000ull, 0x0000705070000000ull, 0x0000e0a0e0000000ull, 0x0000c040c0000000ull,
    0x0003020300000000ull, 0x0007050700000000ull, 0x000e0a0e00000000ull, 0x001c14100000000cull, 0x0038283800000000ull, 0x0070507000000000ull, 0x00e0a0e000000000ull, 0x00c040c000000000ull,
    0x0302030000000000ull, 0x0705070000000000ull, 0x0e0a0e0000000000ull, 0x1c1410000000000cull, 0x3828380000000000ull, 0x7050700000000000ull, 0xe0a0e00000000000ull, 0xc040c00000000000ull,
    0x0203000000000000ull, 0x0507000000000000ull, 0x0a0e000000000000ull, 0x141000000000000cull, 0x2838000000000000ull, 0x5070000000000000ull, 0xa0e0000000000000ull, 0x40c0000000000000ull,
};

uint64_t ray_table[8][64] = {0};


uint64_t rand64(void) {
    /* The state must be seeded so that it is not everywhere zero. */
    static int p = 0;

    static uint64_t s[16] = {
        0x123456789abcdef0ull,
        0x2eab0093287c1d11ull,
        0x18ae739cb0aa9301ull,
        0xa31a1b1582123a11ull,
        0x819a0a9916273eacull,
        0x74578b20199a8374ull,
        0x5738295d930ff847ull,
        0xf38d7c9dead03911ull,
        0x0aab930847292413ull,
        0xc136480012984021ull,
        0xe920490d89190cc1ull,
        0xc361161034920591ull,
        0x92078ca398d93013ull,
        0x6b9348b734812d98ull,
        0x31239b8930a0e923ull,
        0x412b9c09285f5901ull,
    };
    const uint64_t s0 = s[p];
    uint64_t s1 = s[p = (p + 1) & 15];
    s1 ^= s1 << 31; // a
    s[p] = s1 ^ s0 ^ (s1 >> 11) ^ (s0 >> 30); // b,c
    return s[p] * 1181783497276652981ull;
}
static uint64_t rand64u(void) {
    static uint64_t next = 0x1820381947542809;
 
    next = next * 1103515245 + 12345;
    return next;
}
void initialize_lookup_tables() {
    int i, j, k, l;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            for (k = i + 1; k < 8; k++)
                ray_table[NORTH][i * 8 + j] |= (1ull << (k * 8 + j));
            for (k = j + 1; k < 8; k++)
                ray_table[EAST][i * 8 + j] |= (1ull << (i * 8 + k));
            for (k = i - 1; k >= 0; k--)
                ray_table[SOUTH][i * 8 + j] |= (1ull << (k * 8 + j));
            for (k = j - 1; k >= 0; k--)
                ray_table[WEST][i * 8 + j] |= (1ull << (i * 8 + k));

            for (k = i + 1, l = j + 1; k < 8 && l < 8; k++ && l++)
                ray_table[NORTHEAST][i * 8 + j] |= (1ull << (k * 8 + l));
            for (k = i + 1, l = j - 1; k < 8 && l >=0; k++ && l--)
                ray_table[NORTHWEST][i * 8 + j] |= (1ull << (k * 8 + l));
            for (k = i - 1, l = j + 1; k >=0 && l < 8; k-- && l++)
                ray_table[SOUTHEAST][i * 8 + j] |= (1ull << (k * 8 + l));
            for (k = i - 1, l = j - 1; k >=0 && l >=0; k-- && l--)
                ray_table[SOUTHWEST][i * 8 + j] |= (1ull << (k * 8 + l));
        }
    }
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

static uint64_t ray_attacks_positive(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy, int direction) {
    uint64_t ray_mask = ray_table[direction][square];
    uint64_t blockers = friendly_occupancy | enemy_occupancy;
    uint64_t lsb = LSB(ray_mask & blockers);
    uint64_t attacks = ray_mask & (lsb - 1);
    attacks |= (lsb & enemy_occupancy);
    return attacks;
}

static uint64_t ray_attacks_negative(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy, int direction) {
    uint64_t ray_mask = ray_table[direction][square];
    uint64_t blockers = (friendly_occupancy | enemy_occupancy) & ray_mask;
    uint64_t msb = MSB(blockers);
    uint64_t attacks = 0;
    if (blockers) {
        attacks = ray_mask & (-msb ^ msb);
        attacks |= (msb & enemy_occupancy);
    }
    else
        attacks = ray_mask;
    
    return attacks;
}

static void ray_blockers_positive(int square, uint64_t blockers, int direction,
        uint64_t king, uint64_t* pinned, uint64_t* pinners) {
    uint64_t ray_mask = ray_table[direction][square];
    uint64_t lsb = LSB(ray_mask & blockers);
    if (king & ray_attacks_positive(square, 0, blockers ^ lsb, direction)) {
        *pinned |= lsb;
        *pinners |= (1ull << square);
    }
}


static void ray_blockers_negative(int square, uint64_t blockers, int direction,
        uint64_t king, uint64_t* pinned, uint64_t* pinners) {
    uint64_t ray_mask = ray_table[direction][square];
    uint64_t msb = MSB(blockers & ray_mask);
    if (blockers & ray_mask) {
        if (king & ray_attacks_negative(square, 0, blockers ^ msb, direction)) {
            *pinned |= msb;
            *pinners |= (1ull << square);
        }
    }
}

static uint64_t attack_set_pawn_white_capture(int square, uint64_t enpassant, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    uint64_t attacks = 0;
    uint64_t pawn = 1ull << square;
    // For non-hfile, attack right
    attacks |= ((pawn & ~HFILE) << 9) & enemy_occupancy;
    // For non-afile, attack left
    attacks |= ((pawn & ~AFILE) << 7) & enemy_occupancy;
    // For non-hfile, attack en-passant
    attacks |= (((pawn & ~HFILE) << 1) & enpassant) << 8;
    // For non-afile, attack en-passant
    attacks |= (((pawn & ~AFILE) >> 1) & enpassant) << 8;
    return attacks;
}

static uint64_t attack_set_pawn_white(int square, uint64_t enpassant, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    uint64_t blockers = friendly_occupancy | enemy_occupancy;
    uint64_t attacks;
    uint64_t pawn = 1ull << square;
    attacks = (pawn << 8) & ~blockers;
    // if attacks = 0, this is blocked. Otherwise, we see if we can advance further
    attacks |= ((attacks & RANK3) << 8) & ~blockers;
    attacks |= attack_set_pawn_white_capture(square, enpassant, friendly_occupancy, enemy_occupancy);

    return attacks;
}

static uint64_t attack_set_pawn_black_capture(int square, uint64_t enpassant, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    uint64_t attacks;
    uint64_t pawn = (1ull << square);
    // For non-afile, attack left
    attacks = ((pawn & ~AFILE) >> 9) & enemy_occupancy;
    // For non-hfile, attack right
    attacks |= ((pawn & ~HFILE) >> 7) & enemy_occupancy;
    // For non-afile, attack en-passant
    attacks |= (((pawn & ~AFILE) >> 1) & enpassant) >> 8;
    // For non-hfile, attack en-passant
    attacks |= (((pawn & ~HFILE) <<1) & enpassant) >> 8;
    return attacks;
}

static uint64_t attack_set_pawn_black(int square, uint64_t enpassant, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    uint64_t blockers = friendly_occupancy | enemy_occupancy;
    uint64_t attacks;
    uint64_t pawn = (1ull << square);
    attacks = (pawn >> 8) & ~blockers;
    // if attacks = 0, this is blocked. Otherwise, we see if we can advance further
    attacks |= ((attacks & RANK6) >> 8) & ~blockers;
    attacks |= attack_set_pawn_black_capture(square, enpassant, friendly_occupancy, enemy_occupancy);

    return attacks;
}

uint64_t (*attack_set_pawn[2])(int, uint64_t, uint64_t, uint64_t) = {attack_set_pawn_white, attack_set_pawn_black};
uint64_t (*attack_set_pawn_capture[2])(int, uint64_t, uint64_t, uint64_t) = {attack_set_pawn_white_capture, attack_set_pawn_black_capture};

// Parallel computation of pawn attack sets

static uint64_t attack_set_pawn_multiple_white_capture_left(uint64_t pawns, uint64_t enpassant, uint64_t enemy_occupancy) {
    uint64_t attacks;
    // For non-afile, attack left
    attacks = ((pawns & ~AFILE) << 7) & enemy_occupancy;
    // For non-afile, attack en-passant
    attacks |= (((pawns & ~AFILE) >> 1) & enpassant) << 8;
    return attacks;
}

static uint64_t attack_set_pawn_multiple_white_capture_right(uint64_t pawns, uint64_t enpassant, uint64_t enemy_occupancy) {
    uint64_t attacks;
    // For non-hfile, attack right
    attacks = ((pawns & ~HFILE) << 9) & enemy_occupancy;
    // For non-hfile, attack en-passant
    attacks |= (((pawns & ~HFILE) << 1) & enpassant) << 8;
    return attacks;
}

static uint64_t attack_set_pawn_multiple_white_capture(uint64_t pawns, uint64_t enpassant, uint64_t enemy_occupancy) {
    return attack_set_pawn_multiple_white_capture_left(pawns, enpassant, enemy_occupancy) |
        attack_set_pawn_multiple_white_capture_right(pawns, enpassant, enemy_occupancy);
}

static uint64_t attack_set_pawn_multiple_black_capture_left(uint64_t pawns, uint64_t enpassant, uint64_t enemy_occupancy) {
    uint64_t attacks;
    // For non-afile, attack left
    attacks = ((pawns & ~AFILE) >> 9) & enemy_occupancy;
    // For non-afile, attack en-passant
    attacks |= (((pawns & ~AFILE) >> 1) & enpassant) >> 8;
    return attacks;
}

static uint64_t attack_set_pawn_multiple_black_capture_right(uint64_t pawns, uint64_t enpassant, uint64_t enemy_occupancy) {
    uint64_t attacks;
    // For non-hfile, attack right
    attacks = ((pawns & ~HFILE) >> 7) & enemy_occupancy;
    // For non-hfile, attack en-passant
    attacks |= (((pawns & ~HFILE) << 1) & enpassant) >> 8;
    return attacks;
}

static uint64_t attack_set_pawn_multiple_black_capture(uint64_t pawns, uint64_t enpassant, uint64_t enemy_occupancy) {
    return attack_set_pawn_multiple_black_capture_left(pawns, enpassant, enemy_occupancy) |
        attack_set_pawn_multiple_black_capture_right(pawns, enpassant, enemy_occupancy);
}

static uint64_t attack_set_pawn_multiple_white_advance_one(uint64_t pawns, uint64_t enpassant, uint64_t blockers) {
    uint64_t attacks;
    attacks = (pawns << 8) & ~blockers;
    return attacks;
    // if attacks = 0, this is blocked. Otherwise, we see if we can advance further
}

static uint64_t attack_set_pawn_multiple_white_advance_two(uint64_t pawns, uint64_t enpassant, uint64_t blockers) {
    uint64_t attacks;
    attacks = (pawns << 8) & ~blockers;
    // if attacks = 0, this is blocked. Otherwise, we see if we can advance further
    attacks |= ((attacks & RANK3) << 8) & ~blockers;
    return attacks;
}

static uint64_t attack_set_pawn_multiple_black_advance_one(uint64_t pawns, uint64_t enpassant, uint64_t blockers) {
    uint64_t attacks;
    attacks = (pawns >> 8) & ~blockers;
    return attacks;
}

static uint64_t attack_set_pawn_multiple_black_advance_two(uint64_t pawns, uint64_t enpassant, uint64_t blockers) {
    uint64_t attacks;
    attacks = (pawns >> 8) & ~blockers;
    // if attacks = 0, this is blocked. Otherwise, we see if we can advance further
    attacks = ((attacks & RANK6) >> 8) & ~blockers;
    return attacks;
}

uint64_t (*attack_set_pawn_multiple_capture[2])(uint64_t, uint64_t, uint64_t) = {attack_set_pawn_multiple_white_capture, attack_set_pawn_multiple_black_capture};

uint64_t (*attack_set_pawn_multiple_capture_left[2])(uint64_t, uint64_t, uint64_t) = {attack_set_pawn_multiple_white_capture_left, attack_set_pawn_multiple_black_capture_left};
uint64_t (*attack_set_pawn_multiple_capture_right[2])(uint64_t, uint64_t, uint64_t) = {attack_set_pawn_multiple_white_capture_right, attack_set_pawn_multiple_black_capture_right};

uint64_t (*attack_set_pawn_multiple_advance_one[2])(uint64_t, uint64_t, uint64_t) = {attack_set_pawn_multiple_white_advance_one, attack_set_pawn_multiple_black_advance_two};
uint64_t (*attack_set_pawn_multiple_advance_two[2])(uint64_t, uint64_t, uint64_t) = {attack_set_pawn_multiple_white_advance_two, attack_set_pawn_multiple_black_advance_two};

static uint64_t attack_set_knight(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    return knight_move_table[square] & (~friendly_occupancy);
}

static uint64_t attack_set_king(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    return king_move_table[square] & (~friendly_occupancy);
}

static void pin_set_rook(int square, uint64_t blockers,
        uint64_t king, uint64_t* pinned, uint64_t* pinner) {
    ray_blockers_positive(square, blockers, NORTH, king, pinned, pinner);
    ray_blockers_positive(square, blockers, EAST, king, pinned, pinner);
    ray_blockers_negative(square, blockers, SOUTH, king, pinned, pinner);
    ray_blockers_negative(square, blockers, WEST, king, pinned, pinner);
}

static void pin_set_bishop(int square, uint64_t blockers,
        uint64_t king, uint64_t* pinned, uint64_t* pinner) {
    ray_blockers_positive(square, blockers, NORTHEAST, king, pinned, pinner);
    ray_blockers_positive(square, blockers, NORTHWEST, king, pinned, pinner);
    ray_blockers_negative(square, blockers, SOUTHEAST, king, pinned, pinner);
    ray_blockers_negative(square, blockers, SOUTHWEST, king, pinned, pinner);
}

static uint64_t attack_set_rook(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    return (
            ray_attacks_positive(square, friendly_occupancy, enemy_occupancy, NORTH) |
            ray_attacks_positive(square, friendly_occupancy, enemy_occupancy, EAST) |
            ray_attacks_negative(square, friendly_occupancy, enemy_occupancy, SOUTH) |
            ray_attacks_negative(square, friendly_occupancy, enemy_occupancy, WEST)
           );
}

static uint64_t attack_set_bishop(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    return (
            ray_attacks_positive(square, friendly_occupancy, enemy_occupancy, NORTHEAST) |
            ray_attacks_positive(square, friendly_occupancy, enemy_occupancy, NORTHWEST) |
            ray_attacks_negative(square, friendly_occupancy, enemy_occupancy, SOUTHEAST) |
            ray_attacks_negative(square, friendly_occupancy, enemy_occupancy, SOUTHWEST)
           );
}

static uint64_t attack_set_queen(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    return attack_set_rook(square, friendly_occupancy, enemy_occupancy) | attack_set_bishop(square, friendly_occupancy, enemy_occupancy);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * CHECKING CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

uint64_t is_attacked(struct board* board, uint64_t friendly_occupancy, uint64_t enemy_occupancy, int who, int square) {
    // Returns bitmask of locations of attackers belonging to who on square
    uint64_t attackers = 0;
    uint64_t bishop_attacks = attack_set_bishop(square, 0, friendly_occupancy | enemy_occupancy);
    uint64_t rook_attacks = attack_set_rook(square, 0, friendly_occupancy | enemy_occupancy);
    uint64_t * pieces = board->pieces[who];
    // Ignore en-passant for attack set
    // To see if black can attack square, check if white pawn can move from square to one of black pawns
    attackers |= (attack_set_pawn_capture[1-who](square, 0, 0, friendly_occupancy | enemy_occupancy) & pieces[PAWN]);
    attackers |= (attack_set_knight(square, 0, friendly_occupancy | enemy_occupancy) & pieces[KNIGHT]);
    attackers |= (bishop_attacks & pieces[BISHOP]);
    attackers |= (rook_attacks & pieces[ROOK]);
    attackers |= ((bishop_attacks | rook_attacks) & pieces[QUEEN]);
    attackers |= (attack_set_king(square, 0, friendly_occupancy | enemy_occupancy) & pieces[KING]);
    return attackers;
}

uint64_t is_attacked_slider(struct board* board, uint64_t friendly_occupancy, uint64_t enemy_occupancy, int who, int square) {
    // Returns bitmask of locations of attackers belonging to who on square
    uint64_t attackers = 0;
    uint64_t bishop_attacks = attack_set_bishop(square, friendly_occupancy, enemy_occupancy);
    uint64_t rook_attacks = attack_set_rook(square, friendly_occupancy, enemy_occupancy);
    uint64_t * pieces = board->pieces[who];
    attackers |= (bishop_attacks & pieces[BISHOP]);
    attackers |= (rook_attacks & pieces[ROOK]);
    attackers |= ((bishop_attacks | rook_attacks) & pieces[QUEEN]);
    return attackers;
}

uint64_t is_in_check(struct board* board, int who, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    uint64_t king = board->pieces[who][KING];
    int square = LSBINDEX(king);
    return is_attacked(board, enemy_occupancy, friendly_occupancy, 1 - who, square);
}

uint64_t is_in_check_slider(struct board* board, int who, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    uint64_t king = board->pieces[who][KING];
    int square = LSBINDEX(king);
    return is_attacked_slider(board, enemy_occupancy, friendly_occupancy, 1 - who, square);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * MOVE VALIDATION CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int is_valid_move_with_moveset(struct board* board, char who, struct delta move,
        struct moveset* mvs) {
    int i;
    uint64_t dest = (1ull << move.square2);
    if (move.captured != -1) {
        if ((move.captured != PAWN || move.piece != PAWN) &&
                !(dest & P2BM(board, 6 * (1-who) + move.captured)))
            return 0;
        else if (move.captured == PAWN && move.piece == PAWN) {
            if (who) {
                if (!(board->enpassant & (dest >> 8)) &&
                        !(dest & P2BM(board, 6 * (1-who) + move.captured)))
                    return 0;
            } else {
                if (!(board->enpassant & (dest << 8)) &&
                        !(dest & P2BM(board, 6 * (1-who) + move.captured)))
                    return 0;
            }
        }
    }
    for (i = 0; i < mvs->npieces; i++) {
        if (mvs->moves[i].piece == move.piece && mvs->moves[i].square == move.square1)
        {
            if ((mvs->moves[i].board & dest) && !(move.misc & 0x80)) return 1;
            if (mvs->moves[i].castle && (move.misc & 0x80)) return 1;
        }
    }
    return 0;
}

int is_valid_move(struct board* board, char who, struct delta move) {
    struct moveset mvs;
    mvs.npieces = 0;
    mvs.nmoves = 0;
    generate_moves(&mvs, board, who);
    if (is_valid_move_with_moveset(board, who, move, &mvs)) {
        apply_move(board, who, &move);
        if (is_in_check(board, who, board_friendly_occupancy(board, who),
                    board_enemy_occupancy(board, who))) {
            reverse_move(board, who, &move);
            return 0;
        }
        reverse_move(board, who, &move);
        return 1;
    }
    return 0;
}

int apply_move(struct board* board, char who, struct delta* move) {
    uint64_t hupdate = side_hash_code;
    move->enpassant = LSBINDEX(board->enpassant);
    move->cancastle = board->cancastle;
    move->misc &= 0xc0;
    move->misc |= board->nmovesnocapture;
    board->who = 1 - board->who;

    if (board->enpassant != 1) hupdate ^= enpassant_hash_codes[move->enpassant % 8];
    board->enpassant = 1;

    uint64_t mask1 = (1ull << move->square1);
    uint64_t mask2 = (1ull << move->square2);

    P2BM(board, who * 6 + move->promotion) ^= mask2;
    P2BM(board, who * 6 + move->piece) ^= mask1;
    board->nmoves += 1;
    if (who) board->nmovesnocapture += 1;

    hupdate ^= square_hash_codes[move->square1][6 * who + move->piece];
    hupdate ^= square_hash_codes[move->square2][6 * who + move->promotion];

    // If capture, reset nmovesnocapture clock
    if (move->captured != -1) {
        if (!(mask2 & P2BM(board, 6 * (1-who) + move->captured))) {
            // En passant
            if (who) {
                P2BM(board, 6 * (1-who) + move->captured) ^= (mask2 << 8);
                hupdate ^= square_hash_codes[move->square2 + 8][6 * (1-who) + move->captured];
            }
            else {
                P2BM(board, 6 * (1-who) + move->captured) ^= (mask2 >> 8);
                hupdate ^= square_hash_codes[move->square2 - 8][6 * (1-who) + move->captured];
            }
            move->misc |= 0x40;
        }
        else {
            P2BM(board, 6 * (1-who) + move->captured) ^= mask2;
            hupdate ^= square_hash_codes[move->square2][6 * (1-who) + move->captured];
            if (move->captured == ROOK) {
                if (move->square2 == 56)
                    board->cancastle &= 0x0d;
                else if (move->square2 == 63)
                    board->cancastle &= 0x0e;
                else if (move->square2 == 0)
                    board->cancastle &= 0x07;
                else if (move->square2 == 7)
                    board->cancastle &= 0x0b;
            }
        }
        board->nmovesnocapture = 0;
    }

    // If pawn move, reset nmovesnocapture clock, and set enpassant if applicable
    if (move->piece == PAWN) {
        board->nmovesnocapture = 0;
        int nmove = (move->square2 - move->square1) * (1 - 2 * who);
        if (nmove == 16) {
            hupdate ^= enpassant_hash_codes[move->square2 % 8];
            board->enpassant = mask2;
        }
    }

    // Handle castling
    if (move->misc & 0x80) {
        board->castled |= 1 << who;
        int rank2 = move->square2 / 8;
        int file2 = move->square2 % 8;
        if (file2 == 2) {
            P2BM(board, 6*who+ROOK) ^= (0x09ull << (rank2 * 8));
            hupdate ^= square_hash_codes[who ? 56 : 0][6 * who + ROOK] ^ square_hash_codes[who ? 59 : 3][6 * who + ROOK];
        } else {
            P2BM(board, 6*who+ROOK) ^= (0xa0ull << (rank2 * 8));
            hupdate ^= square_hash_codes[who ? 63 : 7][6 * who + ROOK] ^ square_hash_codes[who ? 61 : 5][6 * who + ROOK];
        }
    }

    // Handle Castling Priviledge
    if (move->piece == KING)
        board->cancastle &= 0x03 << (2 * who);
    else if (move->piece == ROOK && move->square1 == 56)
        board->cancastle &= 0x0d;
    else if (move->piece == ROOK && move->square1 == 63)
        board->cancastle &= 0x0e;
    else if (move->piece == ROOK && move->square1 == 0)
        board->cancastle &= 0x07;
    else if (move->piece == ROOK && move->square1 == 7)
        board->cancastle &= 0x0b;

    if (!(board->cancastle & 0x08) && move->cancastle & 0x8)
        hupdate ^= castling_hash_codes[0];
    if (!(board->cancastle & 0x04) && move->cancastle & 0x4)
        hupdate ^= castling_hash_codes[1];
    if (!(board->cancastle & 0x02) && move->cancastle & 0x2)
        hupdate ^= castling_hash_codes[2];
    if (!(board->cancastle & 0x01) && move->cancastle & 0x1)
        hupdate ^= castling_hash_codes[3];

    move->hupdate = hupdate;

    board->hash ^= hupdate;

    return 0;
}

int reverse_move(struct board* board, char who, move_t* move) {
    board->enpassant = (1ull << move->enpassant);
    board->who = 1 - board->who;

    board->hash ^= move->hupdate;

    board->cancastle = move->cancastle;
    board->nmovesnocapture = move->misc & 0x3f;
    move->misc &= 0xc0;
    board->nmoves -= 1;

    uint64_t mask1 = (1ull << move->square1);
    uint64_t mask2 = (1ull << move->square2);
    P2BM(board, 6*who + move->promotion) ^= mask2;
    P2BM(board, 6*who + move->piece) ^= mask1;
    // If capture
    if (move->captured != -1) {
        if (move->misc & 0x40) {
            // En passant
            P2BM(board, 6 * (1-who) + move->captured) ^= board->enpassant;
        }
        else
            P2BM(board, 6*(1-who) + move->captured) ^= mask2;
    }
    // Handle castling
    if (move->misc & 0x80) {
        board->castled &= 1 << (1-who);
        int rank2 = move->square2 / 8;
        int file2 = move->square2 % 8;
        if (file2 == 2) {
            P2BM(board, 6*who+ROOK) ^= (0x09ull << (rank2 * 8));
        } else {
            P2BM(board, 6*who+ROOK) ^= (0xa0ull << (rank2 * 8));
        }
    }

    return 0;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * MOVE GENERATION CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

uint64_t attacked_squares(struct board* board, char who, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    // All attacked squares, disregarding check, along with protections
    uint64_t attack = 0;
    uint64_t temp;
    int square;
    attack |= attack_set_pawn_multiple_capture[who](board->pieces[who][PAWN], 0, friendly_occupancy | enemy_occupancy);
    // Rooks
    bmloop(board->pieces[who][ROOK], square, temp) {
        attack |= attack_set_rook(square, 0, friendly_occupancy | enemy_occupancy);
    }
    // Knights
    bmloop(board->pieces[who][KNIGHT], square, temp) {
        attack |= attack_set_knight(square, 0, friendly_occupancy | enemy_occupancy);
    }
    // Bishops
    bmloop(board->pieces[who][BISHOP], square, temp) {
        attack |= attack_set_bishop(square, 0, friendly_occupancy | enemy_occupancy);
    }
    // Queens
    bmloop(board->pieces[who][QUEEN], square, temp) {
        attack |= attack_set_queen(square, 0, friendly_occupancy | enemy_occupancy);
    }
    // King
    bmloop(board->pieces[who][KING], square, temp) {
        attack |= attack_set_king(square, 0, friendly_occupancy | enemy_occupancy);
    }
    return attack;
}

void pinned_pieces(struct board* board, uint64_t king, char who,
        uint64_t friendly_occupancy, uint64_t enemy_occupancy,
        uint64_t* pinned, uint64_t* bishop_pinners, uint64_t* rook_pinners
        ) {
    uint64_t temp;
    int square;
    *pinned = 0;
    *bishop_pinners = 0;
    *rook_pinners = 0;
    // Rooks
    bmloop(board->pieces[who][ROOK], square, temp) {
        pin_set_rook(square, friendly_occupancy | enemy_occupancy,
                king, pinned, rook_pinners);
    }
    // Bishops
    bmloop(board->pieces[who][BISHOP], square, temp) {
        pin_set_bishop(square, friendly_occupancy | enemy_occupancy,
                king, pinned, bishop_pinners);
    }
    // Queens
    bmloop(board->pieces[who][QUEEN], square, temp) {
        pin_set_rook(square, friendly_occupancy | enemy_occupancy,
                king, pinned, rook_pinners);
        pin_set_bishop(square, friendly_occupancy | enemy_occupancy,
                king, pinned, bishop_pinners);
    }
}

uint64_t board_friendly_occupancy(struct board* board, char who) {
    return board->pieces[who][PAWN] | board->pieces[who][KNIGHT] | board->pieces[who][BISHOP] | board->pieces[who][ROOK] | board->pieces[who][QUEEN] | board->pieces[who][KING];
}

uint64_t board_enemy_occupancy(struct board* board, char who) {
    return board->pieces[1-who][PAWN] | board->pieces[1-who][KNIGHT] | board->pieces[1-who][BISHOP] | board->pieces[1-who][ROOK] | board->pieces[1-who][QUEEN] | board->pieces[1-who][KING];
}

int board_nmoves_accurate(struct board* board, char who) {
    struct moveset mvs;
    struct deltaset out;
    int i, count;
    mvs.npieces = 0;
    mvs.nmoves = 0;
    generate_moves(&mvs, board, who);
    moveset_to_deltaset(board, &mvs, &out);

    count = 0;
    for (i = 0; i < out.nmoves; i++) {
        apply_move(board, who, &out.moves[i]);
        if (!is_in_check(board, who, board_friendly_occupancy(board, who),
                    board_enemy_occupancy(board, who))) {
            count += 1;
        }
        reverse_move(board, who, &out.moves[i]);
    }
    free(out.moves);
    return count;
}

void generate_moves(struct moveset* mvs, struct board* board, char who) {
    // Generate pseudo-legal moves
    // Some pins against the king will be disregarded, but
    // picked up next cycle
    uint64_t temp, square;
    uint64_t attack, opponent_attacks;
    uint64_t friendly_occupancy, enemy_occupancy;
    uint64_t check, mask, nopiececheck;
    struct moveset_piece moves;

    // uint64_t pinned, bishop_pinners, rook_pinners;

    mvs->npieces = 0;
    mvs->nmoves = 0;
    mvs->imincheck = 0;
    mvs->check = 0;

    mask = ~0;
    int check_count = 0;
    friendly_occupancy = board_friendly_occupancy(board, who);
    enemy_occupancy = board_enemy_occupancy(board, who);

    // squares an opponent can attack if king is not present.
    uint64_t king = board->pieces[who][KING];

    if (is_in_check(board, 1-who, enemy_occupancy, friendly_occupancy)) {
        mvs->imincheck = 1;
        return;
    }

    opponent_attacks = attacked_squares(board, 1-who, enemy_occupancy, friendly_occupancy ^ king);

    //pinned_pieces(board, king, 1-who, friendly_occupancy, enemy_occupancy,
    //    &pinned, &bishop_pinners, &rook_pinners);

    check = is_in_check(board, who, friendly_occupancy, enemy_occupancy);
    check_count = (check != 0);
    mvs->check = (check != 0);
    if (check & (check - 1)) check_count = 2;

    if (check_count == 2) {
        // We can only move king
        square = LSBINDEX(board->pieces[who][KING]);
        attack = attack_set_king(square, friendly_occupancy, enemy_occupancy);
        moves.board = attack & (~opponent_attacks);
        moves.piece = KING;
        moves.square = square;
        moves.castle = 0;
        moveset_push(mvs, &moves);
    }
    else {
        if (check) {
            mask = check;
            uint64_t raynw, rayne, raysw, rayse, rayn, rays, raye, rayw;
            raynw = 0, rayne = 0, raysw = 0, rayne = 0, rayn = 0, rays = 0, raye = 0, rayw = 0;

            int attacker = LSBINDEX(check);
            if (check &
                    (board->pieces[1-who][ROOK] | board->pieces[1-who][QUEEN])) {
                rayn = ray_attacks_positive(attacker, enemy_occupancy, friendly_occupancy, NORTH);
                raye = ray_attacks_positive(attacker, enemy_occupancy, friendly_occupancy, EAST);
                rays = ray_attacks_negative(attacker, enemy_occupancy, friendly_occupancy, SOUTH);
                rayw = ray_attacks_negative(attacker, enemy_occupancy, friendly_occupancy, WEST);
            }

            if (check &
                    (board->pieces[1-who][BISHOP] | board->pieces[1-who][QUEEN])) {
                raynw = ray_attacks_positive(attacker, enemy_occupancy, friendly_occupancy, NORTHWEST);
                rayne = ray_attacks_positive(attacker, enemy_occupancy, friendly_occupancy, NORTHEAST);
                raysw = ray_attacks_negative(attacker, enemy_occupancy, friendly_occupancy, SOUTHWEST);
                rayse = ray_attacks_negative(attacker, enemy_occupancy, friendly_occupancy, SOUTHEAST);
            }
            if (raynw & king) mask |= raynw;
            else if (raysw & king) mask |= raysw;
            else if (rayne & king) mask |= rayne;
            else if (rayse & king) mask |= rayse;
            else if (rayn & king) mask |= rayn;
            else if (rayw & king) mask |= rayw;
            else if (rays & king) mask |= rays;
            else if (raye & king) mask |= raye;
        }

        moves.castle = 0;
        // The following are ordered in likelihood that moving the piece is a good move
        // Knights
        moves.piece = KNIGHT;
        bmloop(board->pieces[who][KNIGHT], square, temp) {
            attack = attack_set_knight(square, friendly_occupancy, enemy_occupancy) & mask;
            moves.board = attack;
            moves.square = square;
            moveset_push(mvs, &moves);
        }
        // Bishops
        moves.piece = BISHOP;
        bmloop(board->pieces[who][BISHOP], square, temp) {
            attack = attack_set_bishop(square, friendly_occupancy, enemy_occupancy) & mask;
            moves.board = attack;
            moves.square = square;
            moveset_push(mvs, &moves);
        }

        // Pawns
        moves.piece = PAWN;
        bmloop(board->pieces[who][PAWN], square, temp) {
            attack = attack_set_pawn[who](square, board->enpassant, friendly_occupancy, enemy_occupancy);
            // Special case: attacker is an advanced by two pawn,
            // which can be captured en passant
            if ((check & board->enpassant) && board->enpassant != 1) {
                if (who)
                    attack &= (mask | (mask >> 8));
                else
                    attack &= (mask | (mask << 8));
            } else {
                attack &= mask;
            }
            moves.board = attack;
            moves.square = square;
            moveset_push(mvs, &moves);
            mvs->nmoves += bitmap_count_ones((RANK1 | RANK8) & attack) * 3;
        }

        // Queens
        moves.piece = QUEEN;
        bmloop(board->pieces[who][QUEEN], square, temp) {
            attack = attack_set_queen(square, friendly_occupancy, enemy_occupancy) & mask;
            moves.board = attack;
            moves.square = square;
            moveset_push(mvs, &moves);
        }
        // Rooks
        moves.piece = ROOK;
        bmloop(board->pieces[who][ROOK], square, temp) {
            attack = attack_set_rook(square, friendly_occupancy, enemy_occupancy) & mask;
            moves.board = attack;
            moves.square = square;
            moveset_push(mvs, &moves);
        }

        // King
        moves.piece = KING;
        square = LSBINDEX(king);
        attack = attack_set_king(square, friendly_occupancy, enemy_occupancy);
        attack = ~opponent_attacks & attack;
        moves.board = attack;
        moves.square = square;
        moves.castle = 0;
        moveset_push(mvs, &moves);
        if (who) {
            // Black queenside
            if ((board->cancastle & 0x02) &&
                    !(0x0e00000000000000ull & (friendly_occupancy | enemy_occupancy)) &&
                    !(0x1c00000000000000ull & opponent_attacks)) {
                moves.board = 0x0400000000000000ull;
                moves.piece = KING;
                moves.square = square;
                moves.castle = 3;
                moveset_push(mvs, &moves);
            }
            // Black kingside
            if ((board->cancastle & 0x01) &&
                    !(0x6000000000000000ull & (friendly_occupancy | enemy_occupancy)) &&
                    !(0x7000000000000000ull & opponent_attacks)) {
                moves.board = 0x4000000000000000ull;
                moves.piece = KING;
                moves.square = square;
                moves.castle = 4;
                moveset_push(mvs, &moves);
            }
        } else {
            // White queenside;
            if ((board->cancastle & 0x08) &&
                    !(0x000000000000000eull & (friendly_occupancy | enemy_occupancy)) &&
                    !(0x000000000000001cull & opponent_attacks)) {
                moves.board = 0x0000000000000004ull;
                moves.piece = KING;
                moves.square = square;
                moves.castle = 1;
                moveset_push(mvs, &moves);
            }
            // White kingside
            if ((board->cancastle & 0x04) &&
                    !(0x0000000000000060ull & (friendly_occupancy | enemy_occupancy)) &&
                    !(0x0000000000000070ull & opponent_attacks)) {
                moves.board = 0x0000000000000040ull;
                moves.piece = KING;
                moves.square = square;
                moves.castle = 2;
                moveset_push(mvs, &moves);
            }
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Evaluation CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Evaluates relative score of white and black. Pawn = 1. who = whose turn. Positive is good for white.
// is_in_check = if current side is in check, nmoves = number of moves

// All tables are from white's perspective
// We give a bonus for all pieces the closer they move to the opposite side
int pawn_table[64] = {
    800, 800, 800, 800, 800, 800, 800, 800,
    100, 100, 100, 100, 100, 100, 100, 100, 
    40, 40, 45, 50, 50, 45, 40, 40,
    10, 30, 35, 40, 40, 35, 30, 10,
    10, 20, 20, 40, 40, 20, 20, 10,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 0, -10, -10, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0.0, 
};

int knight_table[64] = {
    -30, -20, -10, -10, -10, -10, -20, -30,
     20,  20,  30,  30,  30,  30,  20,  20,
     20,  30,  35,  35,  35,  35,  30,  20,
    -30, -10,  25,  30,  30,  25, -10, -30,
    -30, -10,  25,  30,  30,  25, -10, -30,
    -30, -10,  35,  25,  25,  35, -10, -30,
    -40, -10, -10, -10, -10, -10, -10, -40,
    -50, -40, -30, -30, -30, -30, -40, -50,

};

int bishop_table[64] = {
    -20, -20, -20, -20, -20, -20, -20, -20,
      0,  20, 30,  30,  30,  30,  10,   0,
     30,  30,  30,  30,  30,  30,  0,  30,
    -30,  0,  10,  20,  20,  10,  0, -30,
    -30,  0,  10,  20,  20,  10,  0, -30,
    -30,  0,  20,  10,  10,  10,  0, -30,
    -30,  10,  0,  0,  0,  0,  10, -30,
    -40, -40, -40, -40, -40, -40, -40, -40,
};

int rook_table[64] = {
    70, 70, 70, 70, 70, 70, 70, 70,
    80, 80, 80, 80, 80, 80, 80, 80,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 10, 10, 10, 10, 0, 0,
};

int queen_table[64] = {
    70, 70, 70, 70, 70, 70, 70, 70,
    60, 60, 60, 60, 60, 60, 60, 60,
    40, 40, 40, 50, 50, 40, 40, 40,
    0, 0, 20, 30, 30, 20, 0, 0,
    0, 0, 20, 30, 30, 20, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

int king_table[64] = {
    -100, -100, -100, -100, -100, -100, -100, -100,
    -100, -100, -100, -100, -100, -100, -100, -100,
    -100, -100, -100, -100, -100, -100, -100, -100,
    -100, -100, -100, -100, -100, -100, -100, -100,
    -50, -50, -50, -60, -60, -50, -50, -50,
    -20, -20, -40, -50, -50, -40, -20, -20,
    -10, -10, -10, -20, -20, -10, -10, -10,
    10, 10, 10, -10, -10, 10, 10, 10,
};

int king_table_endgame[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20, -20, -20, -20, -20, -20, -40,
    -30, -20,  30,  30,  30,  30, -20, -30,
    -30, -20,  30,  40,  40,  30, -20, -30,
    -30, -20,  30,  40,  40,  30, -20, -30,
    -30, -20,  30,  30,  30,  30, -20, -30,
    -40, -20, -20, -20, -20, -20, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50,
};

int passed_pawn_table[8] = {0, 0, 0, 26, 77, 154, 256, 800};

#define BLACK_CENTRAL_SQUARES 0x0000281428140000ull
#define WHITE_CENTRAL_SQUARES 0x0000142814280000ull

/* Scoring the board:
 * We score the board in units of centipawns, taking the following
 * into consideration:
 *  1. Material
 *  2. Piece location (differs for king between endgame and midgame)
 *  3. Presence of bishop pair (half a pawn)
 *  4. Pawn structure
 *      a. passed pawn (bonus)
 *      b. isolated pawn (penalty)
 *      c. doubled pawns (penalty)
 *  5. Doubled rooks (bonus)
 *  6. Rooks on open files (bonus, 1/5 of a pawn)
 *  7. Rooks on semiopen files (bonus, 1/10 of a pawn)
 *  8. Central pawns on same color for bishop (penalty)
 *  9. Undefended attacked pieces (heavy penalty)
 *  10. Number of available attacks, disregarding king pins (bonus)
 *  11. Castling rights (penalty if you can't castle)
 *  12. King safety
 *      a. Open file (penalty)
 *      b. Lack of pawn shield for castled king (heavy penalty)
 *      c. Pawn storm (bonus for attacker)
 *  13. Trading down bonus
 *
 * TODO list:
 *  1. Endgame table
 *  2. Finer material nuances, like material hash table
 */
int board_score(struct board* board, char who, struct moveset* mvs, int nmoves) {
    int i;
    int seen;
    int rank, file, pwho;
    piece_t piece;
    if (nmoves < 0)
        nmoves = board_nmoves_accurate(board, who);
    who = -who * 2 + 1;
    if (mvs->nmoves == 0 && mvs->imincheck) {
        // Checkmate. A checkmate is better if it happens sooner
        // If we are about to be mated, choose a longer mate
        // so that we can profit on opponent mistakes
        return (CHECKMATE - board->nmoves) * who;
    }
    if (mvs->nmoves == 0 && mvs->check) {
        return -(CHECKMATE - board->nmoves) * who;
    }
    // Stalemate = draw
    if (mvs->nmoves == 0) return 0;
    if (board->nmovesnocapture >= 50) return 0;

    int score = 0;
    seen = 0;
    uint64_t temp;
    int square, count;

    uint64_t white_pawns, white_minor, white_major, black_pawns,
             black_minor, black_major, white, black, white_king, black_king;
    white_pawns = P2BM(board, WHITEPAWN);
    black_pawns = P2BM(board, BLACKPAWN);
    white_minor = P2BM(board, WHITEKNIGHT) | P2BM(board, WHITEBISHOP);
    black_minor = P2BM(board, BLACKKNIGHT) | P2BM(board, BLACKBISHOP);
    white_major = P2BM(board, WHITEROOK) | P2BM(board, WHITEQUEEN);
    black_major = P2BM(board, BLACKROOK) | P2BM(board, BLACKQUEEN);
    white_king = P2BM(board, WHITEKING);
    black_king = P2BM(board, BLACKKING);

    white = white_pawns | white_minor | white_major | white_king;
    black = black_pawns | black_minor | black_major | black_king;

    uint64_t whiteattack, blackattack, whiteundefended, blackundefended;
    whiteattack = attacked_squares(board, 0, white, black);
    blackattack = attacked_squares(board, 1, black, white);

    whiteundefended = blackattack ^ (whiteattack & blackattack);
    blackundefended = whiteattack ^ (whiteattack & blackattack);

    int nwhiteminor, nwhitemajor, nblackmajor, nblackminor, endgame;
    nwhitemajor = bitmap_count_ones(white_major);
    nwhiteminor = bitmap_count_ones(white_minor);
    nblackmajor = bitmap_count_ones(black_major);
    nblackminor = bitmap_count_ones(black_minor);

    endgame = ((nwhiteminor <= 2 && nwhitemajor <= 1) ||
            (nwhiteminor == 0 && nwhitemajor <= 2) ||
            (nblackmajor == 0)) &&
            ((nblackminor <= 2 && nblackmajor <= 1) ||
            (nblackminor <= 2 && nblackmajor <= 1) ||
            (nblackmajor == 0));

    /*
    if (endgame && !white_pawns && !black_pawns) {
        // Some endgame positions to help the engine achieve better depth
        uint64_t white_queen, black_queen;
        white_queen = P2BM(board, WHITEQUEEN);
        black_queen = P2BM(board, BLACKQUEEN);
        if (white_queen && nblackmajor == 0 && nblackminor <= 1)
            return CHECKMATE/2; // Queen vs king+minor piece = win
        if (white_queen && nblackmajor == 1 && !black_queen)
            return CHECKMATE/2; // Queen vs king+minor piece = win
        if (black_queen && nwhitemajor == 0 && nwhiteminor <= 1)
            return -CHECKMATE/2; // Queen vs king+minor piece = win
        if (black_queen && nwhitemajor == 1 && !white_queen)
            return -CHECKMATE/2; // Queen vs king+minor piece = win
    }
    */

    // undeveloped pieces penalty
    if (RANK1 & white_minor)
        score -= 40;
    if (RANK8 & black_minor)
        score += 40;

    int whitematerial, blackmaterial;
    whitematerial = 0;
    blackmaterial = 0;

    uint64_t mask;

    count = 0;
    bmloop(P2BM(board, WHITEPAWN), square, temp) {
        count += 1;
        rank = square / 8;
        file = square & 0x7;
        whitematerial += 100;
        // Pawns become much more valuable at the endgame
        if (endgame) whitematerial += 35;
        score += pawn_table[56 - square + file + file];
        // undefended pieces are likely taken
        if ((1ull << square) & whiteundefended)
            score -= 50;
        // doubled pawns are bad
        if ((AFILE << file)  & (white_pawns ^ (1ull << square)))
            score -= 20;
        // passed pawns are good
        if (!((AFILE << square) & black_pawns))
            score += passed_pawn_table[rank];

        mask = 0;
        // isolated pawns are bad
        if (file != 0) mask |= (AFILE << (file - 1));
        if (file != 7) mask |= (AFILE << (file + 1));
        if (!(mask & white_pawns)) score -= 20;
    }
    // If you have no pawns, endgames will be hard
    if (!count) score -= 120;

    count = 0;
    bmloop(P2BM(board, BLACKPAWN), square, temp) {
        count += 1;
        file = square & 0x7;
        rank = square / 8;
        blackmaterial += 100;
        if (endgame) blackmaterial += 35;
        score -= pawn_table[square];
        if ((1ull << square) & blackundefended)
            score += 50;
        if ((AFILE << file) & (black_pawns ^ (1ull << square)))
            score += 20;
        if (!(((AFILE << file) >> (56 - 8 * rank)) & white_pawns))
            score -= passed_pawn_table[8-rank];

        mask = 0;
        if (file != 0) mask |= (AFILE << (file - 1));
        if (file != 7) mask |= (AFILE << (file + 1));
        if (!(mask & black_pawns)) score += 20;
    }
    if (!count) score += 120;

    bmloop(P2BM(board, WHITEKNIGHT), square, temp) {
        file = square & 0x7;
        whitematerial += 320;
        score += knight_table[56 - square + file + file];
        if ((1ull << square) & whiteundefended)
            score -= 150;
    }
    bmloop(P2BM(board, BLACKKNIGHT), square, temp) {
        blackmaterial += 320;
        score -= knight_table[square];
        if ((1ull << square) & blackundefended)
            score += 150;
    }

    count = 0;
    bmloop(P2BM(board, WHITEBISHOP), square, temp) {
        file = square & 0x7;
        whitematerial += 333;
        score += bishop_table[56 - square + file + file];
        count += 1;
        if ((1ull << square) & whiteundefended)
            score -= 150;
        // At least before end-game, central pawns on same
        // colored squares are bad for bishops
        if ((1ull << square) & BLACK_CENTRAL_SQUARES) {
            score -= bitmap_count_ones((white_pawns | black_pawns) & BLACK_CENTRAL_SQUARES) * 15;
        } else {
            score -= bitmap_count_ones((white_pawns | black_pawns) & WHITE_CENTRAL_SQUARES) * 15;
        }
    }
    // Bishop pairs are very valuable
    // In the endgame, 2 bishops can checkmate a king,
    // whereas 2 knights can't
    if (endgame)
        score += (count == 2) * 100;
    else
        score += (count == 2) * 50;

    count = 0;
    bmloop(P2BM(board, BLACKBISHOP), square, temp) {
        blackmaterial += 333;
        score -= bishop_table[square];
        count += 1;
        if ((1ull << square) & blackundefended)
            score += 150;
        if ((1ull << square) & BLACK_CENTRAL_SQUARES) {
            score += bitmap_count_ones((white_pawns | black_pawns) & BLACK_CENTRAL_SQUARES) * 15;
        } else {
            score += bitmap_count_ones((white_pawns | black_pawns) & WHITE_CENTRAL_SQUARES) * 15;
        }
    }

    if (endgame)
        score -= (count == 2) * 100;
    else
        score -= (count == 2) * 50;

    bmloop(P2BM(board, WHITEROOK), square, temp) {
        file = square & 0x7;
        whitematerial += 510;
        // Rooks becomes slightly stronger at the endgame
        if (endgame)
            whitematerial += 20;
        score += rook_table[56 - square + file + file];
        // Rooks on open files are great
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score += 20;
        // Rooks on semiopen files are good
        else if ((AFILE << file) & black_pawns)
            score += 10;

        // Doubled rooks are very powerful.
        // We add 80 (40 on this, 40 on other)
        if ((AFILE << file) & (white_major ^ (1ull << square)))
            score += 30;

        if ((1ull << square) & whiteundefended)
            score -= 320;
    }
    bmloop(P2BM(board, BLACKROOK), square, temp) {
        file = square & 0x7;
        blackmaterial += 510;
        if (endgame) blackmaterial += 20;
        score -= rook_table[square];
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score -= 20;
        else if ((AFILE << file) & white_pawns)
            score -= 10;

        if ((AFILE << file) & (black_major ^ (1ull << square)))
            score -= 30;

        if ((1ull << square) & blackundefended)
            score += 320;
    }
    bmloop(P2BM(board, WHITEQUEEN), square, temp) {
        file = square & 0x7;
        whitematerial += 880;
        score += queen_table[56 - square + file + file];
        // A queen counts as a rook
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score += 20;
        else if ((AFILE << file) & black_pawns)
            score += 10;

        if ((AFILE << file) & (white_major ^ (1ull << square)))
            score += 30;

        if ((1ull << square) & whiteundefended)
            score -= 620;
    }
    bmloop(P2BM(board, BLACKQUEEN), square, temp) {
        blackmaterial += 880;
        score -= queen_table[square];
        if (!((AFILE << file) & (white_pawns | black_pawns)))
            score -= 20;
        else if ((AFILE << file) & white_pawns)
            score -= 10;

        if ((AFILE << file) & (black_major ^ (1ull << square)))
            score -= 30;

        if ((1ull << square) & blackundefended)
            score += 620;
    }

    score += (whitematerial - blackmaterial);

    // Trading down is good for the side with more material
    if (whitematerial - blackmaterial > 0)
        score += (4006 - blackmaterial) / 8;
    else if (whitematerial - blackmaterial < 0)
        score -= (4006 - whitematerial) / 8;

    // castling is almost always awesome
    score += (board->castled & 1) * 100 - ((board->castled & 2) >> 1) * 100;
    score += ((board->cancastle & 12) != 0) * 100 - ((board->cancastle & 3) != 0 ) * 100;

    // King safety

    square = LSBINDEX(white_king);
    file = square & 0x7;

    if (endgame) {
        score += king_table_endgame[56 - square + file + file];
    } else {
        score += king_table[56 - square + file + file];
    }

    mask = AFILE << file;
    if (file != 0)
        mask |= (AFILE << (file - 1));
    if (file != 7)
        mask |= (AFILE << (file + 1));

    // open files are bad news for the king
    if (!((AFILE << file) & white_pawns))
        score -= 15;
    // we want a pawn shield
    if ((board->castled & 1) && !((mask >> 32) & white_pawns))
        score -= 10;
    if ((board->castled & 1) && !((mask >> 40) & white_pawns))
        score -= 10;
    if ((board->castled & 1) && !((mask >> 48) & white_pawns))
        score -= 10;
    // Uh-oh -- maybe a pawn storm!
    if ((mask >> 32) & black_pawns)
        score -= 10;
    if ((mask >> 40) & black_pawns)
        score -= 30;
    if ((mask >> 48) & black_pawns)
        score -= 50;

    uint64_t king_movements;
    king_movements = white_king | attack_set_king(square, white, black);
    count = bitmap_count_ones(king_movements & (~blackattack));
    if (who == 1 && mvs->check) {
        if (count <= 4) score -= (4 - count) * 60;
        if (count == 0) score -= 100;
        score -= 20;
    } else if (count <= 2 && (king_movements & (~blackattack))) {
        score -= (3 - count) *30;
    } else if (count == 0) score -= 20;

    square = LSBINDEX(black_king);
    file = square & 0x7;
    if (endgame) {
        score -= king_table_endgame[square];
    } else {
        score -= king_table[square];
    }

    mask = AFILE << file;
    if (file != 0)
        mask |= (AFILE << (file - 1));
    if (file != 7)
        mask |= (AFILE << (file + 1));

    if (!((AFILE << file) & black_pawns)) score -= 30;
    if ((board->castled & 1) && !((mask << 32) & black_pawns))
        score += 15;
    if ((board->castled & 1) && !((mask << 40) & black_pawns))
        score += 10;
    if ((board->castled & 1) && !((mask << 48) & black_pawns))
        score += 10;
    if ((mask << 32) & white_pawns)
        score += 10;
    if ((mask << 40) & white_pawns)
        score += 30;
    if ((mask << 48) & white_pawns)
        score += 50;

    king_movements = black_king | attack_set_king(square, black, white);
    count = bitmap_count_ones(king_movements & (~whiteattack));
    if (who == -1 && mvs->check) {
        if (count <= 4) score += (4 - count) * 60;
        if (count == 0) score += 100;
        score += 20;
    } else if (count <= 2 && (king_movements & (~whiteattack))) {
        score += (3 - count) *30;
    } else if (count == 0) score += 20;


    // the side with more options is better
    score += (bitmap_count_ones(whiteattack) - bitmap_count_ones(blackattack)) * 8;

    return score;
}
