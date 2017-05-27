#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pieces.h"
#include "magic.h"
#include "moves.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * UTILITY CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

const int sorted_pieces[] = {KING, QUEEN, ROOK, KNIGHT, BISHOP, PAWN};

const char* piece_names = "PRNBQKprnbqk";

void board_init(struct board* out) {
    if (!board_init_from_fen(out, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"))
        exit(1);
}

int board_npieces(struct board* board, side_t who) {
    return popcnt(board_occupancy(board, who));
}

char get_piece_on_square(struct board* board, int square) {
    side_t who;
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

int move_equal(move_t m1, move_t m2) {
    // We just need the first 6 bytes to be equal
    uint64_t m1p, m2p;
    m1p = *((uint64_t *) &m1);
    m2p = *((uint64_t *) &m2);
    return (m1p & 0x0000c0ffffffffffull) == (m2p & 0x0000c0ffffffffffull);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * CHECKING CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

uint64_t is_attacked(struct board* board, uint64_t friendly_occupancy, uint64_t enemy_occupancy, side_t who, int square) {
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

uint64_t is_attacked_slider(struct board* board, uint64_t friendly_occupancy, uint64_t enemy_occupancy, side_t who, int square) {
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

uint64_t get_cheapest_attacker(struct board* board, uint64_t attackers, int who, int* piece) {
    uint64_t subset;
    subset = attackers & board->pieces[who][PAWN];
    if (subset) {*piece = PAWN; return subset & (-subset);}
    subset = attackers & board->pieces[who][KNIGHT];
    if (subset) {*piece = KNIGHT; return subset & (-subset);}
    subset = attackers & board->pieces[who][BISHOP];
    if (subset) {*piece = BISHOP; return subset & (-subset);}
    subset = attackers & board->pieces[who][ROOK];
    if (subset) {*piece = ROOK; return subset & (-subset);}
    subset = attackers & board->pieces[who][QUEEN];
    if (subset) {*piece = QUEEN; return subset & (-subset);}
    subset = attackers & board->pieces[who][KING];
    if (subset) {*piece = KING; return subset & (-subset);}
    return 0;
}

uint64_t is_in_check(struct board* board, side_t who, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    uint64_t king = board->pieces[who][KING];
    int square = LSBINDEX(king);
    return is_attacked(board, enemy_occupancy, friendly_occupancy, 1 - who, square);
}

int gives_check(struct board * board, uint64_t occupancy, move_t* move, side_t who) {
    switch (move->piece) {
        case PAWN:
            return attack_set_pawn[who](move->square2, 0, 0, occupancy) & board->pieces[1-who][KING];
        case ROOK:
            return attack_set_rook(move->square2, 0, occupancy) & board->pieces[1-who][KING];
        case KNIGHT:
            return attack_set_knight(move->square2, 0, occupancy) & board->pieces[1-who][KING];
        case BISHOP:
            return attack_set_bishop(move->square2, 0, occupancy) & board->pieces[1-who][KING];
        case QUEEN:
            return attack_set_queen(move->square2, 0, occupancy) & board->pieces[1-who][KING];
        case KING:
            // Kings could never give checks
            return 0;
        default:
            return 0;
    }
}

uint64_t is_in_check_slider(struct board* board, side_t who, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    uint64_t king = board->pieces[who][KING];
    int square = LSBINDEX(king);
    return is_attacked_slider(board, enemy_occupancy, friendly_occupancy, 1 - who, square);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * MOVE VALIDATION CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int is_valid_move(struct board* board, side_t who, struct delta move) {
    struct deltaset mvs;
    generate_moves(&mvs, board);
    int i;
    for (i = 0; i < mvs.nmoves; i++) {
        if (move_equal(mvs.moves[i], move))
            return 1;
    }
    return 0;
}

uint64_t board_flip_side(struct board* board, uint64_t enpassant) {
    uint64_t old_enpassant = board->enpassant;
    if (old_enpassant != 1) {
        board->hash ^= enpassant_hash_codes[LSBINDEX(old_enpassant) % 8];
    }
    board->enpassant = enpassant;
    if (enpassant != 1) {
        board->hash ^= enpassant_hash_codes[LSBINDEX(enpassant) % 8];
    }
    board->who = 1 - board->who;
    board->hash ^= side_hash_code;
    return old_enpassant;
}

int apply_move(struct board* board, struct delta* move) {
    side_t who = board->who;
    uint64_t hupdate = side_hash_code;
    move->enpassant = LSBINDEX(board->enpassant);
    move->cancastle = board->cancastle;
    move->misc &= 0xc0;
    move->misc |= board->nmovesnocapture;
    board->who = 1 - board->who;

    if (board->enpassant != 1) hupdate ^= enpassant_hash_codes[move->enpassant % 8];

    uint64_t mask1 = (1ull << move->square1);
    uint64_t mask2 = (1ull << move->square2);

    board->pieces[who][move->promotion] ^= mask2;
    board->pieces[who][move->piece] ^= mask1;
    board->nmoves += 1;
    board->nmovesnocapture += who;

    hupdate ^= square_hash_codes[move->square1][6 * who + move->piece];
    hupdate ^= square_hash_codes[move->square2][6 * who + move->promotion];

    if (move->piece == PAWN) {
        board->pawn_hash ^= square_hash_codes[move->square1][6 * who + move->piece];
        if (move->promotion == PAWN) {
            board->pawn_hash ^= square_hash_codes[move->square1][6 * who + move->promotion];
        }
    }

    if (move->piece == KING) {
        board->kingsq[who] = move->square2;
    }

    // If capture, reset nmovesnocapture clock
    if (move->captured != -1) {
        if (move->misc & 0x40) {
            // En passant
            board->pieces[1-who][move->captured] ^= board->enpassant;
            hupdate ^= square_hash_codes[move->enpassant][6 * (1-who) + move->captured];
            move->misc |= 0x40;
        }
        else {
            board->pieces[1-who][move->captured] ^= mask2;
            hupdate ^= square_hash_codes[move->square2][6 * (1-who) + move->captured];
            board->cancastle &= castle_priv[move->square2];
        }
        board->nmovesnocapture = 0;
        if (move->captured == PAWN) {
            board->pawn_hash ^= square_hash_codes[move->square2][6 * (1-who) + move->captured];
        }
    }

    board->enpassant = 1;

    // If pawn move, reset nmovesnocapture clock, and set enpassant if applicable
    if (move->piece == PAWN) {
        board->nmovesnocapture = 0;
        int nmove = move->square2 - move->square1;
        if (nmove == 16 || nmove == -16) {
            if ((((mask2 & ~AFILE) >> 1) | ((mask2 & ~HFILE) << 1)) & board->pieces[1-who][PAWN]) {
                hupdate ^= enpassant_hash_codes[move->square2 % 8];
                board->enpassant = mask2;
            }
        }
    }

    // Handle castling
    if (move->misc & 0x80) {
        board->castled |= 1 << who;
        int rank2 = move->square2 / 8;
        int file2 = move->square2 % 8;
        if (file2 == 2) {
            board->pieces[who][ROOK] ^= (0x09ull << (rank2 * 8));
            hupdate ^= square_hash_codes[who ? 56 : 0][6 * who + ROOK] ^ square_hash_codes[who ? 59 : 3][6 * who + ROOK];
        } else {
            board->pieces[who][ROOK] ^= (0xa0ull << (rank2 * 8));
            hupdate ^= square_hash_codes[who ? 63 : 7][6 * who + ROOK] ^ square_hash_codes[who ? 61 : 5][6 * who + ROOK];
        }
    }

    // Handle Castling Priviledge
    if (move->piece == KING)
        board->cancastle &= 0x03 << (2 * who);
    else if (move->piece == ROOK)
        board->cancastle &= castle_priv[move->square1];

    if (!(board->cancastle & CASTLE_PRIV_WQ) && move->cancastle & CASTLE_PRIV_WQ)
        hupdate ^= castling_hash_codes[0];
    if (!(board->cancastle & CASTLE_PRIV_WK) && move->cancastle & CASTLE_PRIV_WK)
        hupdate ^= castling_hash_codes[1];
    if (!(board->cancastle & CASTLE_PRIV_BQ) && move->cancastle & CASTLE_PRIV_BQ)
        hupdate ^= castling_hash_codes[2];
    if (!(board->cancastle & CASTLE_PRIV_BK) && move->cancastle & CASTLE_PRIV_BK)
        hupdate ^= castling_hash_codes[3];

    move->hupdate = hupdate;

    board->hash ^= hupdate;

    return 0;
}

int reverse_move(struct board* board, move_t* move) {
    board->enpassant = (1ull << move->enpassant);
    board->who = 1 - board->who;
    side_t who = board->who;

    board->hash ^= move->hupdate;

    board->cancastle = move->cancastle;
    board->nmovesnocapture = move->misc & 0x3f;
    move->misc &= 0xc0;
    board->nmoves -= 1;

    uint64_t mask1 = (1ull << move->square1);
    uint64_t mask2 = (1ull << move->square2);
    board->pieces[who][move->promotion] ^= mask2;
    board->pieces[who][move->piece] ^= mask1;
    if (move->piece == PAWN) {
        board->pawn_hash ^= square_hash_codes[move->square1][6 * who + move->piece];
        if (move->promotion == PAWN) {
            board->pawn_hash ^= square_hash_codes[move->square2][6 * who + move->promotion];
        }
    }

    if (move->piece == KING) {
        board->kingsq[who] = move->square1;
    }

    // If capture
    if (move->captured != -1) {
        if (move->misc & 0x40) {
            // En passant
            board->pieces[1-who][move->captured] ^= board->enpassant;
        }
        else
            board->pieces[1-who][move->captured] ^= mask2;
        if (move->captured == PAWN) {
            board->pawn_hash ^= square_hash_codes[move->square2][6 * (1-who) + move->captured];
        }
    }
    // Handle castling
    if (move->misc & 0x80) {
        board->castled &= ~(1 << who);
        int rank2 = move->square2 / 8;
        int file2 = move->square2 % 8;
        if (file2 == 2) {
            board->pieces[who][ROOK] ^= (0x09ull << (rank2 * 8));
        } else {
            board->pieces[who][ROOK] ^= (0xa0ull << (rank2 * 8));
        }
    }

    return 0;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * MOVE GENERATION CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

uint64_t attacked_squares(struct board* board, side_t who, uint64_t occ) {
    // All attacked squares, disregarding check, along with protections
    uint64_t attack = 0;
    uint64_t temp;
    int square;
    attack |= attack_set_pawn_multiple_capture_movement[who](board->pieces[who][PAWN]);
    // Rooks
    bmloop(board->pieces[who][ROOK], square, temp) {
        attack |= attack_set_rook(square, 0, occ);
    }
    // Knights
    bmloop(board->pieces[who][KNIGHT], square, temp) {
        attack |= attack_set_knight(square, 0, occ);
    }
    // Bishops
    bmloop(board->pieces[who][BISHOP], square, temp) {
        attack |= attack_set_bishop(square, 0, occ);
    }
    // Queens
    bmloop(board->pieces[who][QUEEN], square, temp) {
        attack |= attack_set_queen(square, 0, occ);
    }
    // King
    attack |= attack_set_king(LSBINDEX(board->pieces[who][KING]), 0, occ);
    return attack;
}

uint64_t board_occupancy(struct board* board, side_t who) {
    return board->pieces[who][PAWN] | board->pieces[who][KNIGHT] | board->pieces[who][BISHOP] |
        board->pieces[who][ROOK] | board->pieces[who][QUEEN] | board->pieces[who][KING];
}

static void deltaset_add_move(struct board* board, side_t who, struct deltaset * out,
        int piece, int square1, uint64_t attacks, uint64_t friendly, uint64_t enemy) {
    uint64_t temp;
    uint64_t mask1, mask2;
    int square2;
    mask1 = 1ull << square1;
    int i = out->nmoves;
    int j;
    int k;
    bmloop(attacks, square2, temp) {
        mask2 = 1ull << square2;
        // The case of en-passant is tricky, as it's the only
        // time you can invoke a discovered check on your self
        if (piece == PAWN && (square2-square1) % 8 != 0 && !(mask2 & enemy)) {
            if (is_in_check_slider(board, who, friendly ^ mask1 ^ mask2, enemy ^ board->enpassant))
                continue;
        }
        if (piece == PAWN && (mask2 & (RANK1 | RANK8))) {
            for (j = 1; j < 5; j++) {
                k = sorted_pieces[j];
                out->moves[i].misc = 0;
                out->moves[i].promotion = k;
                out->moves[i].piece = piece;
                out->moves[i].captured = -1;
                out->moves[i].square2 = square2;
                out->moves[i].square1 = square1;

                if (mask2 & enemy) {
                    if (mask2 & board->pieces[1 - who][PAWN])
                        out->moves[i].captured = PAWN;
                    else if (mask2 & board->pieces[1 - who][KNIGHT])
                        out->moves[i].captured = KNIGHT;
                    else if (mask2 & board->pieces[1 - who][BISHOP])
                        out->moves[i].captured = BISHOP;
                    else if (mask2 & board->pieces[1 - who][ROOK])
                        out->moves[i].captured = ROOK;
                    else if (mask2 & board->pieces[1 - who][QUEEN])
                        out->moves[i].captured = QUEEN;
                }

                i++;
            }
        } else {
            out->moves[i].misc = 0;
            out->moves[i].promotion = piece;
            out->moves[i].piece = piece;
            out->moves[i].captured = -1;
            out->moves[i].square2 = square2;
            out->moves[i].square1 = square1;

            if (mask2 & enemy) {
                if (mask2 & board->pieces[1 - who][PAWN])
                    out->moves[i].captured = PAWN;
                else if (mask2 & board->pieces[1 - who][KNIGHT])
                    out->moves[i].captured = KNIGHT;
                else if (mask2 & board->pieces[1 - who][BISHOP])
                    out->moves[i].captured = BISHOP;
                else if (mask2 & board->pieces[1 - who][ROOK])
                    out->moves[i].captured = ROOK;
                else if (mask2 & board->pieces[1 - who][QUEEN])
                    out->moves[i].captured = QUEEN;
            } else if (piece == PAWN && (square2 - square1) % 8 != 0) {
                // En-passant
                out->moves[i].captured = PAWN;
                out->moves[i].misc = 0x40;
            }
            i++;
        }
    }
    out->nmoves = i;
}

static void deltaset_add_castle(struct deltaset *out, int square1, int square2) {
    int i = out->nmoves;
    out->moves[i].misc = 0x80;
    out->moves[i].enpassant = 0;
    out->moves[i].cancastle = 0;
    out->moves[i].piece = KING;
    out->moves[i].promotion = KING;
    out->moves[i].captured = -1;
    out->moves[i].square2 = square2;
    out->moves[i].square1 = square1;
    out->nmoves += 1;
}

void generate_moves(struct deltaset* mvs, struct board* board) {
    uint64_t temp, square;
    uint64_t attack, opponent_attacks;
    uint64_t friendly_occupancy, enemy_occupancy;
    uint64_t check;
    uint64_t mask;
    int check_count;

    side_t who = board->who;

    mvs->nmoves = 0;
    mvs->check = 0;
    mvs->who = who;
    mvs->my_attacks = 0;

    friendly_occupancy = board_occupancy(board, who);
    enemy_occupancy = board_occupancy(board, 1 - who);

    uint64_t king = board->pieces[who][KING];
    int kingsquare = board->kingsq[who];

    // Find pieces pinned to the king
    uint64_t pinned = 0;
    uint64_t bishop_pinners, rook_pinners, pinners;
    uint64_t xray_rook = xray_rook_attacks(kingsquare, friendly_occupancy | enemy_occupancy, friendly_occupancy);
    uint64_t xray_bishop = xray_bishop_attacks(kingsquare, friendly_occupancy | enemy_occupancy, friendly_occupancy);
    uint64_t pintemp;
    uint64_t pinmask;
    int pinsq;

    bishop_pinners = xray_bishop & (board->pieces[1-who][BISHOP] | board->pieces[1-who][QUEEN]);
    rook_pinners = xray_rook & (board->pieces[1-who][ROOK] | board->pieces[1-who][QUEEN]);
    pinners = bishop_pinners | rook_pinners;

    pinned = 0;
    bmloop(pinners, pinsq, pintemp) {
        pinned |= ray_between(kingsquare, pinsq);
    }
    mvs->pinned = friendly_occupancy & pinned;

    // Find all squares that the opponent attacks. These are squares that the king cannot move to.
    opponent_attacks = attacked_squares(board, 1-who, enemy_occupancy | (friendly_occupancy ^ king));
    mvs->opponent_attacks = opponent_attacks;

    // Find the pieces that are giving check
    check = is_in_check(board, who, friendly_occupancy, enemy_occupancy);
    check_count = (check != 0);
    mvs->check = check_count;

    if (check & (check - 1)) check_count = 2;

    // In a double check, we can only move the king
    if (check_count == 2) {
        attack = attack_set_king(kingsquare, friendly_occupancy, enemy_occupancy);
        attack = ~opponent_attacks & attack;
        deltaset_add_move(board, who, mvs, KING, kingsquare, attack, friendly_occupancy, enemy_occupancy);
        mvs->my_attacks |= attack;
        return;
    }

    // Mask represents squares that a piece must move to.
    // Usually, pieces can move anywhere, but if a king is in check,
    // pieces must either capture the piece or block the check,
    // so mask is set more narrowly.
    mask = ~0;
    if (check) {
        mask = check;
        int attacker = LSBINDEX(check);
        if (!(check & (board->pieces[1 - who][KNIGHT])))
            mask |= ray_between(attacker, kingsquare);
    }

    // The following are ordered in likelihood that moving the piece is a good move
    // Knights
    bmloop(board->pieces[who][KNIGHT], square, temp) {
        if ((1ull << square) & pinned) {
            // Knights can never escape pins, so do nothing
            continue;
        }
        else {
            attack = attack_set_knight(square, friendly_occupancy, enemy_occupancy) & mask;
            deltaset_add_move(board, who, mvs, KNIGHT, square, attack, friendly_occupancy, enemy_occupancy);
            mvs->my_attacks |= attack;
        }
    }

    // Bishops
    bmloop(board->pieces[who][BISHOP], square, temp) {
        if ((1ull << square) & pinned) {
            pinmask = ray_between(kingsquare,
                    LSBINDEX(line_between(kingsquare, square) & pinners));
            attack = attack_set_bishop(square, friendly_occupancy, enemy_occupancy) & mask & pinmask;
        } else {
            attack = attack_set_bishop(square, friendly_occupancy, enemy_occupancy) & mask;
        }
        deltaset_add_move(board, who, mvs, BISHOP, square, attack, friendly_occupancy, enemy_occupancy);
        mvs->my_attacks |= attack;
    }

    // Pawns
    bmloop(board->pieces[who][PAWN], square, temp) {
        if ((1ull << square) & pinned) {
            pinmask = ray_between(kingsquare,
                    LSBINDEX(line_between(kingsquare, square) & pinners));
            attack = attack_set_pawn[who](square, board->enpassant, friendly_occupancy, enemy_occupancy) & pinmask;
        } else {
            attack = attack_set_pawn[who](square, board->enpassant, friendly_occupancy, enemy_occupancy);
        }
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
        deltaset_add_move(board, who, mvs, PAWN, square, attack, friendly_occupancy, enemy_occupancy);
        mvs->my_attacks |= attack;
    }

    // Queens
    bmloop(board->pieces[who][QUEEN], square, temp) {
        if ((1ull << square) & pinned) {
            pinmask = ray_between(kingsquare,
                    LSBINDEX(line_between(kingsquare, square) & pinners));
            attack = attack_set_queen(square, friendly_occupancy, enemy_occupancy) & mask & pinmask;
        } else {
            attack = attack_set_queen(square, friendly_occupancy, enemy_occupancy) & mask;
        }
        deltaset_add_move(board, who, mvs, QUEEN, square, attack, friendly_occupancy, enemy_occupancy);
        mvs->my_attacks |= attack;
    }

    // Rooks
    bmloop(board->pieces[who][ROOK], square, temp) {
        if ((1ull << square) & pinned) {
            pinmask = ray_between(kingsquare,
                    LSBINDEX(line_between(kingsquare, square) & pinners));
            attack = attack_set_rook(square, friendly_occupancy, enemy_occupancy) & mask & pinmask;
        } else {
            attack = attack_set_rook(square, friendly_occupancy, enemy_occupancy) & mask;
        }
        deltaset_add_move(board, who, mvs, ROOK, square, attack, friendly_occupancy, enemy_occupancy);
        mvs->my_attacks |= attack;
    }

    // King
    attack = attack_set_king(kingsquare, friendly_occupancy, enemy_occupancy);
    attack = ~opponent_attacks & attack;
    deltaset_add_move(board, who, mvs, KING, kingsquare, attack, friendly_occupancy, enemy_occupancy);
    mvs->my_attacks |= attack;
    
    // Castling
    if (who) {
        // Black queenside
        if ((board->cancastle & CASTLE_PRIV_BQ) &&
                !(0x0e00000000000000ull & (friendly_occupancy | enemy_occupancy)) &&
                !(0x1c00000000000000ull & opponent_attacks)) {
            deltaset_add_castle(mvs, kingsquare, 58);
        }
        // Black kingside
        if ((board->cancastle & CASTLE_PRIV_BK) &&
                !(0x6000000000000000ull & (friendly_occupancy | enemy_occupancy)) &&
                !(0x7000000000000000ull & opponent_attacks)) {
            deltaset_add_castle(mvs, kingsquare, 62);
        }
    } else {
        // White queenside;
        if ((board->cancastle & CASTLE_PRIV_WQ) &&
                !(0x000000000000000eull & (friendly_occupancy | enemy_occupancy)) &&
                !(0x000000000000001cull & opponent_attacks)) {
            deltaset_add_castle(mvs, kingsquare, 2);
        }
        // White kingside
        if ((board->cancastle & CASTLE_PRIV_WK) &&
                !(0x0000000000000060ull & (friendly_occupancy | enemy_occupancy)) &&
                !(0x0000000000000070ull & opponent_attacks)) {
            deltaset_add_castle(mvs, kingsquare, 6);
        }
    }
}

void generate_captures(struct deltaset* mvs, struct board* board) {
    uint64_t temp, square;
    uint64_t attack, opponent_attacks;
    uint64_t friendly_occupancy, enemy_occupancy;
    uint64_t check;
    uint64_t mask;

    side_t who = board->who;

    friendly_occupancy = board_occupancy(board, who);
    enemy_occupancy = board_occupancy(board, 1 - who);

    check = is_in_check(board, who, friendly_occupancy, enemy_occupancy);
    if (check) {
        generate_moves(mvs, board);
        return;
    }

    mvs->nmoves = 0;
    mvs->check = 0;
    mvs->who = who;
    mvs->my_attacks = 0;

    // squares an opponent can attack if king is not present.
    uint64_t king = board->pieces[who][KING];

    int kingsquare = LSBINDEX(king);
    // Find pinned pieces
    uint64_t pinned = 0;
    uint64_t bishop_pinners, rook_pinners, pinners;
    uint64_t xray_rook = xray_rook_attacks(kingsquare, friendly_occupancy | enemy_occupancy, friendly_occupancy);
    uint64_t xray_bishop = xray_bishop_attacks(kingsquare, friendly_occupancy | enemy_occupancy, friendly_occupancy);
    uint64_t pintemp;
    uint64_t pinmask;
    int pinsq;

    bishop_pinners = xray_bishop & (board->pieces[1-who][BISHOP] | board->pieces[1-who][QUEEN]);
    rook_pinners = xray_rook & (board->pieces[1-who][ROOK] | board->pieces[1-who][QUEEN]);
    pinners = bishop_pinners | rook_pinners;

    pinned = 0;
    bmloop(pinners, pinsq, pintemp) {
        pinned |= ray_between(kingsquare, pinsq);
    }
    mvs->pinned = friendly_occupancy & pinned;

    opponent_attacks = attacked_squares(board, 1-who, enemy_occupancy | (friendly_occupancy ^ king));
    mvs->opponent_attacks = opponent_attacks;
    mvs->check = 0;

    mask = enemy_occupancy;
    int pawn_mask = mask | RANK1 | RANK8;

    // The following are ordered in likelihood that moving the piece is a good move
    // Knights
    bmloop(board->pieces[who][KNIGHT], square, temp) {
        if ((1ull << square) & pinned) {
            // Knights can never escape pins, so do nothing
            continue;
        }
        else {
            attack = attack_set_knight(square, friendly_occupancy, enemy_occupancy) & mask;
            deltaset_add_move(board, who, mvs, KNIGHT, square, attack, friendly_occupancy, enemy_occupancy);
        }
    }

    // Bishops
    bmloop(board->pieces[who][BISHOP], square, temp) {
        if ((1ull << square) & pinned) {
            bmloop(pinners, pinsq, pintemp) {
                pinmask = ray_between(kingsquare, pinsq);
                if (pinmask & (1ull << square)) break;
            }
            attack = attack_set_bishop(square, friendly_occupancy, enemy_occupancy) & mask & pinmask;
        } else {
            attack = attack_set_bishop(square, friendly_occupancy, enemy_occupancy) & mask;
        }
        deltaset_add_move(board, who, mvs, BISHOP, square, attack, friendly_occupancy, enemy_occupancy);
    }

    // Pawns
    bmloop(board->pieces[who][PAWN], square, temp) {
        if ((1ull << square) & pinned) {
            bmloop(pinners, pinsq, pintemp) {
                pinmask = ray_between(kingsquare, pinsq);
                if (pinmask & (1ull << square)) break;
            }
            attack = attack_set_pawn_capture[who](square, board->enpassant, friendly_occupancy, enemy_occupancy) & pinmask & pawn_mask;
        } else {
            attack = attack_set_pawn_capture[who](square, board->enpassant, friendly_occupancy, enemy_occupancy) & pawn_mask;
        }
        deltaset_add_move(board, who, mvs, PAWN, square, attack, friendly_occupancy, enemy_occupancy);
    }

    // Queens
    bmloop(board->pieces[who][QUEEN], square, temp) {
        if ((1ull << square) & pinned) {
            bmloop(pinners, pinsq, pintemp) {
                pinmask = ray_between(kingsquare, pinsq);
                if (pinmask & (1ull << square)) break;
            }
            attack = attack_set_queen(square, friendly_occupancy, enemy_occupancy) & mask & pinmask;
        } else {
            attack = attack_set_queen(square, friendly_occupancy, enemy_occupancy) & mask;
        }
        deltaset_add_move(board, who, mvs, QUEEN, square, attack, friendly_occupancy, enemy_occupancy);
    }

    // Rooks
    bmloop(board->pieces[who][ROOK], square, temp) {
        if ((1ull << square) & pinned) {
            bmloop(pinners, pinsq, pintemp) {
                pinmask = ray_between(kingsquare, pinsq);
                if (pinmask & (1ull << square)) break;
            }
            attack = attack_set_rook(square, friendly_occupancy, enemy_occupancy) & mask & pinmask;
        } else {
            attack = attack_set_rook(square, friendly_occupancy, enemy_occupancy) & mask;
        }
        deltaset_add_move(board, who, mvs, ROOK, square, attack, friendly_occupancy, enemy_occupancy);
    }

    // King
    attack = attack_set_king(kingsquare, friendly_occupancy, enemy_occupancy);
    attack = ~opponent_attacks & attack & mask;
    deltaset_add_move(board, who, mvs, KING, kingsquare, attack, friendly_occupancy, enemy_occupancy);
}

