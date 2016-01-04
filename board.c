#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pieces.h"
#include "magic.h"

uint64_t square_hash_codes[64][12];
uint64_t castling_hash_codes[4];
uint64_t enpassant_hash_codes[8];
uint64_t side_hash_code;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * UTILITY CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

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

int board_npieces(struct board* board, char who) {
    return bitmap_count_ones(board_friendly_occupancy(board, who));
}

void board_flip_side(struct board* board) {
    board->who = 1 - board->who;
    board->hash ^= side_hash_code;
}

int move_equal(move_t m1, move_t m2) {
    return (m1.square1 == m2.square2) && (m1.piece == m2.piece) && (m1.captured == m2.captured) && (m1.promotion == m2.promotion) && ((m1.misc & 0xc) == (m2.misc & 0xc));
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
    out->moves = malloc(sizeof(move_t) * mvs->nmoves);
    out->nmoves = mvs->nmoves;
    if (!out->moves)
        return -1;
    int i, j, k, square;
    uint64_t temp, mask, friendly_occupancy, enemy_occupancy, checkers;
    friendly_occupancy = board_friendly_occupancy(board, mvs->who);
    enemy_occupancy = board_enemy_occupancy(board, mvs->who);
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
                // First, detect if this move is legal
                // Only illegal moves generated by our move generator
                // are moving pieces pinned to the king
                if (mvs->moves[j].piece != KING) {
                    uint64_t new_friendly_occupancy, new_enemy_occupancy;
                    new_friendly_occupancy = friendly_occupancy ^ (1ull << mvs->moves[j].square) ^ mask;
                    new_enemy_occupancy = enemy_occupancy;
                    if (mvs->moves[j].piece == PAWN) {
                        // The case of en-passant is tricky, as it's the only
                        // time you can invoke a discovered check on your self
                        if ((square - mvs->moves[j].square) % 8 != 0 && 
                                !(mask & enemy_occupancy)) {
                            new_enemy_occupancy ^= board->enpassant;
                        }
                    }
                    checkers = is_in_check_slider(board, mvs->who, new_friendly_occupancy, new_enemy_occupancy);
                    if (checkers && ((checkers ^ mask) != 0))
                        continue;
                }
                if (mvs->moves[j].piece == PAWN &&
                        (mask & (RANK1 | RANK8))) {
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
    out->nmoves = i;
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
    0x0000000003020300ull, 0x0000000007050700ull, 0x000000000e0a0e00ull, 0x000000001c141c00ull, 0x0000000038283800ull, 0x0000000070507000ull, 0x00000000e0a0e000ull, 0x00000000c040c000ull,
    0x0000000302030000ull, 0x0000000705070000ull, 0x0000000e0a0e0000ull, 0x0000001c141c0000ull, 0x0000003828380000ull, 0x0000007050700000ull, 0x000000e0a0e00000ull, 0x000000c040c00000ull,
    0x0000030203000000ull, 0x0000070507000000ull, 0x00000e0a0e000000ull, 0x00001c141c000000ull, 0x0000382838000000ull, 0x0000705070000000ull, 0x0000e0a0e0000000ull, 0x0000c040c0000000ull,
    0x0003020300000000ull, 0x0007050700000000ull, 0x000e0a0e00000000ull, 0x001c141c00000000ull, 0x0038283800000000ull, 0x0070507000000000ull, 0x00e0a0e000000000ull, 0x00c040c000000000ull,
    0x0302030000000000ull, 0x0705070000000000ull, 0x0e0a0e0000000000ull, 0x1c141c0000000000ull, 0x3828380000000000ull, 0x7050700000000000ull, 0xe0a0e00000000000ull, 0xc040c00000000000ull,
    0x0203000000000000ull, 0x0507000000000000ull, 0x0a0e000000000000ull, 0x141c000000000000ull, 0x2838000000000000ull, 0x5070000000000000ull, 0xa0e0000000000000ull, 0x40c0000000000000ull,
};

uint64_t ray_table[8][64] = {0};

struct magic bishop_magics[64];
struct magic rook_magics[64];


static uint64_t attack_set_rook_nonmagic(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy);
static uint64_t attack_set_bishop_nonmagic(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy);

uint64_t attack_set_bishop(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    uint64_t index = friendly_occupancy | enemy_occupancy;
    index &= bishop_magics[square].mask;
    index *= bishop_magics[square].magic;
    index >>= bishop_magics[square].shift;
    return bishop_magics[square].table[index] & (~friendly_occupancy);
}

uint64_t attack_set_rook(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    uint64_t index = friendly_occupancy | enemy_occupancy;
    index &= rook_magics[square].mask;
    index *= rook_magics[square].magic;
    index >>= rook_magics[square].shift;
    return rook_magics[square].table[index] & (~friendly_occupancy);
}

#define BOUNDARY 0xff818181818181ffull
#define INTERIOR (~BOUNDARY)
void initialize_lookup_tables() {
    initialize_magics();
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

    for (i = 0; i < 64; i++) {
        uint64_t attacks = attack_set_rook_nonmagic(i, 0, 0) & rook_magics[i].mask;
        uint64_t current = 0;
        uint64_t temp = 0;
        while (1) {
            rook_magics[i].table[(current * rook_magics[i].magic) >> rook_magics[i].shift] = attack_set_rook_nonmagic(i, 0, current);
            if (current == attacks) break;
            temp = ~(LSB(attacks ^ current) - 1);
            current = (temp & current) ^ LSB(temp);
        }
    }

    for (i = 0; i < 64; i++) {
        uint64_t attacks = attack_set_bishop_nonmagic(i, 0, 0) & bishop_magics[i].mask;
        uint64_t current = 0;
        uint64_t temp = 0;
        while (1) {
            bishop_magics[i].table[(current * bishop_magics[i].magic) >> bishop_magics[i].shift] = attack_set_bishop_nonmagic(i, 0, current);
            if (current == attacks) break;
            temp = ~(LSB(attacks ^ current) - 1);
            current = (temp & current) ^ LSB(temp);
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

static uint64_t attack_set_pawn_multiple_white_capture_movement(uint64_t pawns) {
    return ((pawns & ~AFILE) << 7) | ((pawns & ~HFILE) << 9);
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

static uint64_t attack_set_pawn_multiple_black_capture_movement(uint64_t pawns) {
    return ((pawns & ~HFILE) >> 7) | ((pawns & ~AFILE) >> 9);
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

uint64_t (*attack_set_pawn_multiple_capture_movement[2])(uint64_t) = {attack_set_pawn_multiple_white_capture_movement, attack_set_pawn_multiple_black_capture_movement};

uint64_t (*attack_set_pawn_multiple_capture_left[2])(uint64_t, uint64_t, uint64_t) = {attack_set_pawn_multiple_white_capture_left, attack_set_pawn_multiple_black_capture_left};
uint64_t (*attack_set_pawn_multiple_capture_right[2])(uint64_t, uint64_t, uint64_t) = {attack_set_pawn_multiple_white_capture_right, attack_set_pawn_multiple_black_capture_right};

uint64_t (*attack_set_pawn_multiple_advance_one[2])(uint64_t, uint64_t, uint64_t) = {attack_set_pawn_multiple_white_advance_one, attack_set_pawn_multiple_black_advance_two};
uint64_t (*attack_set_pawn_multiple_advance_two[2])(uint64_t, uint64_t, uint64_t) = {attack_set_pawn_multiple_white_advance_two, attack_set_pawn_multiple_black_advance_two};

static uint64_t attack_set_knight(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    return knight_move_table[square] & (~friendly_occupancy);
}

uint64_t attack_set_king(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
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

static uint64_t attack_set_rook_nonmagic(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    return (
            ray_attacks_positive(square, friendly_occupancy, enemy_occupancy, NORTH) |
            ray_attacks_positive(square, friendly_occupancy, enemy_occupancy, EAST) |
            ray_attacks_negative(square, friendly_occupancy, enemy_occupancy, SOUTH) |
            ray_attacks_negative(square, friendly_occupancy, enemy_occupancy, WEST)
           );
}

static uint64_t attack_set_bishop_nonmagic(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
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
static uint64_t attack_set_queen_nonmagic(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    return attack_set_rook_nonmagic(square, friendly_occupancy, enemy_occupancy) | attack_set_bishop_nonmagic(square, friendly_occupancy, enemy_occupancy);
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
    uint64_t bishop_attacks = attack_set_bishop(square, 0, friendly_occupancy | enemy_occupancy);
    uint64_t rook_attacks = attack_set_rook(square, 0, friendly_occupancy | enemy_occupancy);
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
    attack |= attack_set_pawn_multiple_capture_movement[who](board->pieces[who][PAWN]);
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
    mvs.npieces = 0;
    mvs.nmoves = 0;

    __builtin_prefetch(ray_table);
    __builtin_prefetch(knight_move_table);
    __builtin_prefetch(king_move_table);

    generate_moves(&mvs, board, who);
    moveset_to_deltaset(board, &mvs, &out);
    free(out.moves);
    return out.nmoves;
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
    mvs->check = 0;
    mvs->who = who;

    mask = ~0;
    int check_count = 0;
    friendly_occupancy = board_friendly_occupancy(board, who);
    enemy_occupancy = board_enemy_occupancy(board, who);

    // squares an opponent can attack if king is not present.
    uint64_t king = board->pieces[who][KING];

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
            raynw = 0, rayne = 0, raysw = 0, rayse = 0, rayn = 0, rays = 0, raye = 0, rayw = 0;

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
            assert(attack_set_bishop(square, friendly_occupancy, enemy_occupancy) == attack_set_bishop_nonmagic(square,friendly_occupancy,enemy_occupancy));
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
            assert(attack_set_queen(square, friendly_occupancy, enemy_occupancy) == attack_set_queen_nonmagic(square,friendly_occupancy,enemy_occupancy));
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

