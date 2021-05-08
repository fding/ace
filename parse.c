#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "moves.h"
#include "pieces.h"
#include "evaluation_parameters.h"

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
    out->pawn_hash = 0;

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

    out->kingsq[0] = LSBINDEX(out->pieces[0][KING]);
    out->kingsq[1] = LSBINDEX(out->pieces[1][KING]);
    out->material_score_mg = 0;
    out->pst_score_mg = 0;

    for (square = 0; square < 64; square++) {
        char piece = get_piece_on_square(out, square);
        if (piece != -1) {
            out->hash ^= square_hash_codes[square][piece];
            if (piece == WHITEPAWN || piece == BLACKPAWN) {
                out->pawn_hash ^= square_hash_codes[square][piece];
            }
            switch (piece) {
                case WHITEPAWN:
                    out->material_score_mg += MIDGAME_PAWN_VALUE;
                    break;
                case BLACKPAWN:
                    out->material_score_mg -= MIDGAME_PAWN_VALUE;
                    break;
                case WHITEKNIGHT:
                    out->material_score_mg += MIDGAME_KNIGHT_VALUE;
                    out->pst_score_mg += knight_table[square];
                    break;
                case BLACKKNIGHT:
                    out->material_score_mg -= MIDGAME_KNIGHT_VALUE;
                    out->pst_score_mg -= knight_table[square];
                    break;
                case WHITEBISHOP:
                    out->material_score_mg += MIDGAME_BISHOP_VALUE;
                    out->pst_score_mg += bishop_table[square];
                    break;
                case BLACKBISHOP:
                    out->material_score_mg -= MIDGAME_BISHOP_VALUE;
                    out->pst_score_mg -= bishop_table[square];
                    break;
                case WHITEROOK:
                    out->material_score_mg += MIDGAME_ROOK_VALUE;
                    out->pst_score_mg += rook_table[square];
                    break;
                case BLACKROOK:
                    out->material_score_mg -= MIDGAME_ROOK_VALUE;
                    out->pst_score_mg -= rook_table[square];
                    break;
                case WHITEQUEEN:
                    out->material_score_mg += MIDGAME_QUEEN_VALUE;
                    out->pst_score_mg += queen_table[square];
                    break;
                case BLACKQUEEN:
                    out->material_score_mg -= MIDGAME_QUEEN_VALUE;
                    out->pst_score_mg -= queen_table[square];
                    break;
            }
        }
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
                out->cancastle |= CASTLE_PRIV_WK;
                out->hash ^= castling_hash_codes[1];
                break;
            case 'Q':
                out->cancastle |= CASTLE_PRIV_WQ;
                out->hash ^= castling_hash_codes[0];
                break;
            case 'k':
                out->cancastle |= CASTLE_PRIV_BK;
                out->hash ^= castling_hash_codes[3];
                break;
            case 'q':
                out->cancastle |= CASTLE_PRIV_BQ;
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
        uint64_t mask = 1ull << (rank * 8 + file);
        if ((((mask & ~AFILE) >> 1) | ((mask & ~HFILE) << 1)) & out->pieces[out->who][PAWN]) {
            out->enpassant = 1ull << (rank * 8 + file);
            out->hash ^= enpassant_hash_codes[file];
        }
        position++;
    }

    if (*(++position) != ' ') return NULL;
    position++;
    int a, b;
    if (sscanf(position, "%d %d", &a, &b) < 2)
        return NULL;

    out->nmovesnocapture = a;
    out->nmoves = 2 * (b - 1) + out->who;

    while (*position && *(position++) != ' ');
    position++;
    while (*position && *(position++) != ' ');
    return position;
}

void calgebraic_to_move(struct board* board, char * input, struct delta* move) {
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
        if (popcnt(attacks) != 1) {
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

int algebraic_to_move(struct board* board, char* input, struct delta* move) {
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
        if (popcnt(attacks) > 1) {
            if (popcnt(attacks & (AFILE << (move->square1 % 8))) == 1) {
                *(buffer++) = 'a' + (move->square1 % 8);
            } else if (popcnt(attacks & (RANK1 << (8*(move->square1 / 8)))) == 1) {
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
