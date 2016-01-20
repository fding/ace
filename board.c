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

const char* piece_names = "PRNBQKprnbqk";

void board_init(struct board* out) {
    if (!board_init_from_fen(out, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"))
        exit(1);
}

int board_npieces(struct board* board, unsigned char who) {
    return bitmap_count_ones(board_occupancy(board, who));
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

int move_equal(move_t m1, move_t m2) {
    // We just need the first 6 bytes to be equal
    uint64_t m1p, m2p;
    m1p = *((uint64_t *) &m1);
    m2p = *((uint64_t *) &m2);
    return (m1p & 0x0000c0ffffffffffull) == (m2p & 0x0000c0ffffffffffull);
}

void board_to_fen(struct board* board, char* fen) {
    int i, j;
    int counter;
    for (i = 7; i >= 0; i--) {
        counter = 0;
        for (j = 0; j < 8; j++) {
            char piece = get_piece_on_square(board, 8*i+j);
            if (piece == -1) counter++;
            else {
                if (counter > 0) {
                    *(fen++) = '0' + counter;
                    counter = 0;
                }
                *(fen++) = piece_names[piece];
            }
        }
        if (counter > 0) {
            *(fen++) = '0' + counter;
            counter = 0;
        }
        *(fen++) = '/';
    }
    *(fen - 1) = ' ';

    if (board->who)
        *(fen++) = 'b';
    else
        *(fen++) = 'w';

    *(fen++) = ' ';

    int seen = 0;
    if (board->cancastle & 0x4) {
        *(fen++) = 'K';
        seen = 1;
    }
    if (board->cancastle & 0x8) {
        *(fen++) = 'Q';
        seen = 1;
    }
    if (board->cancastle & 0x1) {
        *(fen++) = 'k';
        seen = 1;
    }
    if (board->cancastle & 0x2) {
        *(fen++) = 'q';
        seen = 1;
    }
    if (!seen)
        *(fen++) = '-';

    *(fen++) = ' ';

    if (board->enpassant != 1) {
        int file = LSBINDEX(board->enpassant) % 8;
        int rank = LSBINDEX(board->enpassant) / 8;
        if (rank == 3) rank = 2;
        else if (rank == 4) rank = 5;
        *(fen++) = 'a' + file;
        *(fen++) = '1' + rank;
    } else {
        *(fen++) = '-';
    }
    *(fen++) = ' ';
    sprintf(fen, "%d %d", board->nmovesnocapture, board->nmoves/2+1);
}

char* board_init_from_fen(struct board* out, char* position) {
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
        if (rank > 7 || rank < 0 || file > 7 || file < 0) return NULL;
        square = 8 * rank + file;
        mask = 1ull << square;
        int found = 0;
        for (i = 0; i < 12; i++) {
            if (*position == piece_names[i]) {
                found = 1;
                P2BM(out, i) |= mask;
            }
        }
        if (!found) {
            if (*position != '/') {
                n = *position - '0';
                if (n < 0 || n > 9) {
                    return NULL;
                }
                file += n - 1;
            }
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

    if (*position == 0) return NULL;

    if (*(++position) != ' ') return NULL;
    position++;

    if (*position == 'w')
        out->who = 0;
    else if (*position == 'b'){
        out->who = 1;
        out->hash ^= side_hash_code;
    } else
        return NULL;

    if (*(++position) != ' ') return NULL;
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
                return NULL;
        }
        if (*(++position) == ' ') break;
    }
    if (*position == 0) return NULL;
    position++;
    if (*position == 0) return NULL;

    out->enpassant = 1;
    if (*position != '-') {
        if (*(position + 1) == 0) return NULL;
        int rank, file;
        file = *position - 'a';
        rank = *position - '1';
        if (out->who) rank += 1;
        else rank -= 1;
        out->enpassant = 1ull << (rank * 8 + file);
        out->hash ^= enpassant_hash_codes[file];
        position++;
    }

    if (*(++position) != ' ') return NULL;
    position++;
    int a, b;
    if (sscanf(position, "%d %d", &a, &b) < 2)
        return NULL;

    out->nmovesnocapture = a;
    out->nmoves = 2 * (b - 1) + out->who;

    for (square = 0; square < 64; square++) {
        char piece = get_piece_on_square(out, square);
        if (piece != -1) {
            out->hash ^= square_hash_codes[square][piece];
        }
    }

    while (*position && *(position++) != ' ');
    position++;
    while (*position && *(position++) != ' ');
    return position;
}

void calgebraic_to_move(char * input, struct board* board, struct delta* move) {
    // Real algebraic notation (Nf3, etc)
    int piece;
    int promotion = -1;
    int misc = 0;
    int rank1, rank2 , file1, file2;

    // Get rid of check and checkmate information
    for (int i = 0; i < strlen(input); i++) {
        if (input[i] == '+' || input[i] == '#') {
            input[i] = 0;
            break;
        }
    }

    if (strcmp(input, "O-O") == 0 || strcmp(input, "0-0") == 0) {
        misc = 0x80;
        promotion = KING;
        piece = KING;
        rank1 = board->who? 7 : 0;
        file1 = 4;
        rank2 = rank1;
        file2 = 6;
    }
    else if (strcmp(input, "O-O-O") == 0 || strcmp(input, "0-0-0") == 0) {
        misc = 0x80;
        promotion = KING;
        piece = KING;
        rank1 = board->who? 7 : 0;
        file1 = 4;
        rank2 = rank1;
        file2 = 2;
    }
    else if (input[0] >= 'A' && input[0] <= 'Z') {
        for (piece = 0; piece < 12; piece++) {
            if (piece_names[piece] == input[0]) break;
        }
        promotion = piece;
        input++;
        // Capture
        if (input[0] == 'x')
            input++;
        rank1 = -1;
        file1 = -1;
        if (strlen(input) > 2) {
            if (input[0] >= 'a' && input[0] <= 'h') {
                file1 = input[0] - 'a';
                input++;
            }
            if (input[0] >= '1' && input[0] <= '8') {
                rank1 = input[0] - '1';
                input++;
            }
            if (input[0] == 'x')
                input++;
        }
        file2 = input[0] - 'a';
        rank2 = input[1] - '1';
        // Locate candidate pieces
        uint64_t attacks = 0;
        switch (piece) {
            case ROOK:
                attacks = attack_set_rook(8 * rank2 + file2, 0,
                        board_occupancy(board, board->who) | board_occupancy(board, 1 - board->who));
                break;
            case KNIGHT:
                attacks = attack_set_knight(8 * rank2 + file2, 0,
                        board_occupancy(board, board->who) | board_occupancy(board, 1 - board->who));
                break;
            case BISHOP:
                attacks = attack_set_bishop(8 * rank2 + file2, 0,
                        board_occupancy(board, board->who) | board_occupancy(board, 1 - board->who));
                break;
            case QUEEN:
                attacks = attack_set_queen(8 * rank2 + file2, 0,
                        board_occupancy(board, board->who) | board_occupancy(board, 1 - board->who));
                break;
            case KING:
                attacks = attack_set_king(8 * rank2 + file2, 0,
                        board_occupancy(board, board->who) | board_occupancy(board, 1 - board->who));
                break;
        }
        assert(attacks != 0);
        attacks &= board->pieces[board->who][piece];
        if (rank1 >= 0) {
            attacks &= (RANK1 << (8*rank1));
        }
        if (file1 >= 0) {
            attacks &= (AFILE << file1);
        }
        if (bitmap_count_ones(attacks) != 1) {
            move->piece = -1;
            return;
        }

        rank1 = LSBINDEX(attacks) / 8;
        file1 = LSBINDEX(attacks) % 8;
    } else {
        piece = PAWN;
        promotion = PAWN;
        // Promotions are indicated with an = sign
        for (int i = 0; i < strlen(input); i++) {
            if (input[i] == '=') {
                for (promotion = 0; promotion < 12; promotion++) {
                    if (piece_names[promotion] == input[i + 1])
                        break;
                }
                input[i] = 0;
                break;
            }
        }
        if (strlen(input) == 2) {
            file2 = input[0] - 'a';
            rank2 = input[1] - '1';
            file1 = file2;
            rank1 = rank2 + (2 * board->who - 1);
            if (!(board->pieces[board->who][PAWN] & (1ull << (8 * rank1 + file1))))
                rank1 += 2 * board->who - 1;
        } else {
            file1 = input[0] - 'a';
            file2 = input[2] - 'a';
            rank2 = input[3] - '1';
            rank1 = rank2 + (2 * board->who - 1);
            if (!(board->pieces[board->who][PAWN] & (1ull << (8 * rank1 + file1))))
                rank1 += 2 * board->who - 1;
        }
    }

    move->square1 = rank1 * 8 + file1;
    move->square2 = rank2 * 8 + file2;
    move->cancastle = 0;
    move->enpassant = 0;
    move->piece = piece;
    move->promotion = promotion;
    move->captured = get_piece_on_square(board, move->square2) % 6;
    if (move->piece == PAWN && file1 != file2 && move->captured == -1) {
        misc |= 0x40;
        move->captured = PAWN;
    }
    move->misc = misc;
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

    if (piece1 == KING) {
        if (file1 - file2 < -1 || file1 - file2 > 1)
            move->misc |= 0x80;
    }
    
    if (input[4] != 0) {
        switch (input[4]) {
            case 'r':
                move->promotion = ROOK;
                break;
            case 'n':
                move->promotion = KNIGHT;
                break;
            case 'b':
                move->promotion = BISHOP;
                break;
            case 'q':
                move->promotion = QUEEN;
                break;
            default:
                return -1;
        }
    }

    return 0;
}

void move_to_calgebraic(struct board* board, char* buffer, struct delta* move) {
    // Castling
    if (move->misc & 0x80) {
        if (move->square2 % 8 == 2) {
            strcpy(buffer, "0-0-0");
        } else {
            strcpy(buffer, "0-0");
        }
        return;
    }

    if (move->piece != PAWN) {
        *(buffer++) = piece_names[move->piece];
        uint64_t attacks = 0;
        switch (move->piece) {
            case ROOK:
                attacks = attack_set_rook(move->square2, 0,
                        board_occupancy(board, board->who) | board_occupancy(board, 1 - board->who));
                break;
            case KNIGHT:
                attacks = attack_set_knight(move->square2, 0,
                        board_occupancy(board, board->who) | board_occupancy(board, 1 - board->who));
                break;
            case BISHOP:
                attacks = attack_set_bishop(move->square2, 0,
                        board_occupancy(board, board->who) | board_occupancy(board, 1 - board->who));
                break;
            case QUEEN:
                attacks = attack_set_queen(move->square2, 0,
                        board_occupancy(board, board->who) | board_occupancy(board, 1 - board->who));
                break;
            case KING:
                attacks = attack_set_king(move->square2, 0,
                        board_occupancy(board, board->who) | board_occupancy(board, 1 - board->who));
                break;
        }
        attacks &= board->pieces[board->who][move->piece];
        if (bitmap_count_ones(attacks) > 1) {
            if (bitmap_count_ones(attacks & (AFILE << (move->square1 % 8))) == 1) {
                *(buffer++) = 'a' + (move->square1 % 8);
            } else if (bitmap_count_ones(attacks & (RANK1 << (8*(move->square1 / 8)))) == 1) {
                *(buffer++) = '1' + (move->square1 / 8);
            } else {
                *(buffer++) = 'a' + (move->square1 % 8);
                *(buffer++) = '1' + (move->square1 / 8);
            }
        }
    } else if (move->captured != -1) {
        *(buffer++) = 'a' + (move->square1 % 8);
    }
    if (move->captured != -1) {
        *(buffer++) = 'x';
    }
    *(buffer++) = 'a' + (move->square2 % 8);
    *(buffer++) = '1' + (move->square2 / 8);
    if (move->promotion != move->piece) {
        *(buffer++) = '=';
        *(buffer++) = piece_names[move->promotion];
    }
    *(buffer++) = 0;
}

void move_to_algebraic(struct board* board, char* buffer, struct delta* move) {
    int rank1 = move->square1 / 8;
    int rank2 = move->square2 / 8;
    int file1 = move->square1 % 8;
    int file2 = move->square2 % 8;
    buffer[0] = 'a' + file1;
    buffer[1] = '1' + rank1;
    buffer[2] = 'a' + file2;
    buffer[3] = '1' + rank2;
    buffer[4] = 0;
    if (move->promotion != move->piece) {
        buffer[4] = piece_names[6 + move->promotion];
        buffer[5] = 0;
    }
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

int gives_check(struct board * board, uint64_t occupancy, move_t* move, int who) {
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

uint64_t is_in_check_slider(struct board* board, int who, uint64_t friendly_occupancy, uint64_t enemy_occupancy) {
    uint64_t king = board->pieces[who][KING];
    int square = LSBINDEX(king);
    return is_attacked_slider(board, enemy_occupancy, friendly_occupancy, 1 - who, square);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * MOVE VALIDATION CODE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int is_valid_move(struct board* board, unsigned char who, struct delta move) {
    struct deltaset mvs;
    generate_moves(&mvs, board, who);
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

int apply_move(struct board* board, unsigned char who, struct delta* move) {
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
    if (who) board->nmovesnocapture += 1;

    hupdate ^= square_hash_codes[move->square1][6 * who + move->piece];
    hupdate ^= square_hash_codes[move->square2][6 * who + move->promotion];

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
    }

    board->enpassant = 1;

    // If pawn move, reset nmovesnocapture clock, and set enpassant if applicable
    if (move->piece == PAWN) {
        board->nmovesnocapture = 0;
        int nmove = move->square2 - move->square1;
        if (nmove == 16 || nmove == -16) {
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

int reverse_move(struct board* board, unsigned char who, move_t* move) {
    board->enpassant = (1ull << move->enpassant);
    board->who = 1 - board->who;

    board->hash ^= move->hupdate;

    board->cancastle = move->cancastle;
    board->nmovesnocapture = move->misc & 0x3f;
    move->misc &= 0xc0;
    board->nmoves -= 1;

    uint64_t mask1 = (1ull << move->square1);
    uint64_t mask2 = (1ull << move->square2);
    board->pieces[who][move->promotion] ^= mask2;
    board->pieces[who][move->piece] ^= mask1;
    // If capture
    if (move->captured != -1) {
        if (move->misc & 0x40) {
            // En passant
            board->pieces[1-who][move->captured] ^= board->enpassant;
        }
        else
            board->pieces[1-who][move->captured] ^= mask2;
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

uint64_t attacked_squares(struct board* board, unsigned char who, uint64_t occ) {
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

uint64_t board_occupancy(struct board* board, unsigned char who) {
    return board->pieces[who][PAWN] | board->pieces[who][KNIGHT] | board->pieces[who][BISHOP] |
        board->pieces[who][ROOK] | board->pieces[who][QUEEN] | board->pieces[who][KING];
}

static void deltaset_add_move(struct board* board, unsigned char who, struct deltaset * out,
        int piece, int square1, uint64_t attacks, uint64_t friendly, uint64_t enemy) {
    uint64_t temp;
    uint64_t mask1, mask2;
    int square2;
    mask1 = 1ull << square1;
    int i = out->nmoves;
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
            for (k = ROOK; k < KING; k++) {
                out->moves[i].misc = 0;
                out->moves[i].promotion = k;
                out->moves[i].piece = piece;
                out->moves[i].captured = -1;
                out->moves[i].square2 = square2;
                out->moves[i].square1 = square1;

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

                i++;
            }
        } else {
            out->moves[i].misc = 0;
            out->moves[i].promotion = piece;
            out->moves[i].piece = piece;
            out->moves[i].captured = -1;
            out->moves[i].square2 = square2;
            out->moves[i].square1 = square1;

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
            // En-passant
            else if (piece == PAWN && (square2 - square1) % 8 != 0) {
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

void generate_moves(struct deltaset* mvs, struct board* board, unsigned char who) {
    uint64_t temp, square;
    uint64_t attack, opponent_attacks;
    uint64_t friendly_occupancy, enemy_occupancy;
    uint64_t check;
    uint64_t mask;
    int check_count;

    mvs->nmoves = 0;
    mvs->check = 0;
    mvs->who = who;
    mvs->my_attacks = 0;

    friendly_occupancy = board_occupancy(board, who);
    enemy_occupancy = board_occupancy(board, 1 - who);

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

    check = is_in_check(board, who, friendly_occupancy, enemy_occupancy);
    check_count = (check != 0);
    mvs->check = (check != 0);

    if (check & (check - 1)) check_count = 2;

    // In a double check, we can only move the king
    if (check_count == 2) {
        attack = attack_set_king(kingsquare, friendly_occupancy, enemy_occupancy);
        attack = ~opponent_attacks & attack;
        deltaset_add_move(board, who, mvs, KING, kingsquare, attack, friendly_occupancy, enemy_occupancy);
        mvs->my_attacks |= attack;
        return;
    }

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
            bmloop(pinners, pinsq, pintemp) {
                pinmask = ray_between(kingsquare, pinsq);
                if (pinmask & (1ull << square)) break;
            }
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
            bmloop(pinners, pinsq, pintemp) {
                pinmask = ray_between(kingsquare, pinsq);
                if (pinmask & (1ull << square)) break;
            }
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
            bmloop(pinners, pinsq, pintemp) {
                pinmask = ray_between(kingsquare, pinsq);
                if (pinmask & (1ull << square)) break;
            }
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
            bmloop(pinners, pinsq, pintemp) {
                pinmask = ray_between(kingsquare, pinsq);
                if (pinmask & (1ull << square)) break;
            }
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
        if ((board->cancastle & 0x02) &&
                !(0x0e00000000000000ull & (friendly_occupancy | enemy_occupancy)) &&
                !(0x1c00000000000000ull & opponent_attacks)) {
            deltaset_add_castle(mvs, kingsquare, 58);
        }
        // Black kingside
        if ((board->cancastle & 0x01) &&
                !(0x6000000000000000ull & (friendly_occupancy | enemy_occupancy)) &&
                !(0x7000000000000000ull & opponent_attacks)) {
            deltaset_add_castle(mvs, kingsquare, 62);
        }
    } else {
        // White queenside;
        if ((board->cancastle & 0x08) &&
                !(0x000000000000000eull & (friendly_occupancy | enemy_occupancy)) &&
                !(0x000000000000001cull & opponent_attacks)) {
            deltaset_add_castle(mvs, kingsquare, 2);
        }
        // White kingside
        if ((board->cancastle & 0x04) &&
                !(0x0000000000000060ull & (friendly_occupancy | enemy_occupancy)) &&
                !(0x0000000000000070ull & opponent_attacks)) {
            deltaset_add_castle(mvs, kingsquare, 6);
        }
    }
}

void generate_captures(struct deltaset* mvs, struct board* board, unsigned char who) {
    uint64_t temp, square;
    uint64_t attack, opponent_attacks;
    uint64_t friendly_occupancy, enemy_occupancy;
    uint64_t check;
    uint64_t mask;

    friendly_occupancy = board_occupancy(board, who);
    enemy_occupancy = board_occupancy(board, 1 - who);

    check = is_in_check(board, who, friendly_occupancy, enemy_occupancy);
    if (check) {
        generate_moves(mvs, board, who);
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
            bmloop(pinners, pinsq, pintemp) {
                pinmask = ray_between(kingsquare, pinsq);
                if (pinmask & (1ull << square)) break;
            }
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
            bmloop(pinners, pinsq, pintemp) {
                pinmask = ray_between(kingsquare, pinsq);
                if (pinmask & (1ull << square)) break;
            }
            attack = attack_set_pawn_capture[who](square, board->enpassant, friendly_occupancy, enemy_occupancy) & pinmask;
        } else {
            attack = attack_set_pawn_capture[who](square, board->enpassant, friendly_occupancy, enemy_occupancy);
        }
        deltaset_add_move(board, who, mvs, PAWN, square, attack, friendly_occupancy, enemy_occupancy);
        mvs->my_attacks |= attack;
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
        mvs->my_attacks |= attack;
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
        mvs->my_attacks |= attack;
    }

    // King
    attack = attack_set_king(kingsquare, friendly_occupancy, enemy_occupancy);
    attack = ~opponent_attacks & attack & mask;
    deltaset_add_move(board, who, mvs, KING, kingsquare, attack, friendly_occupancy, enemy_occupancy);
    mvs->my_attacks |= attack;
}

