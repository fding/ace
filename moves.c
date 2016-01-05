#include "moves.h"
#include "magic.h"
#include "board.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * MOVEMENT CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

uint64_t magic_table[107648];
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

uint64_t ray_attacks_positive(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy, int direction) {
    uint64_t ray_mask = ray_table[direction][square];
    uint64_t blockers = friendly_occupancy | enemy_occupancy;
    uint64_t lsb = LSB(ray_mask & blockers);
    uint64_t attacks = ray_mask & (lsb - 1);
    attacks |= (lsb & enemy_occupancy);
    return attacks;
}

uint64_t ray_attacks_negative(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy, int direction) {
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

char castle_priv[64];

void initialize_move_tables() {
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
        castle_priv[i] = 0xff;
    }

    // These masks control castling priv associated with rooks.
    castle_priv[56] = 0x0d;
    castle_priv[63] = 0x0e;
    castle_priv[0] = 0x07;
    castle_priv[7] = 0x0b;

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

uint64_t attack_set_knight(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    return knight_move_table[square] & (~friendly_occupancy);
}

uint64_t attack_set_king(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    return king_move_table[square] & (~friendly_occupancy);
}

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

uint64_t attack_set_queen(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    return attack_set_rook(square, friendly_occupancy, enemy_occupancy) | attack_set_bishop(square, friendly_occupancy, enemy_occupancy);
}

uint64_t xray_rook_attacks(int square, uint64_t occ, uint64_t blockers) {
   uint64_t attacks = attack_set_rook(square, 0, occ);
   blockers &= attacks;
   return (attacks ^ attack_set_rook(square, 0, occ ^ blockers)) | blockers;
}
 
uint64_t xray_bishop_attacks(int square, uint64_t occ, uint64_t blockers) {
   uint64_t attacks = attack_set_bishop(square, 0, occ);
   blockers &= attacks;
   return (attacks ^ attack_set_bishop(square, 0, occ ^ blockers)) | blockers;
}
