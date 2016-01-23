#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include <assert.h>

int main(int argc, char* argv[]) {
    char *buffer = malloc(1024);
    char line[1024];
    FILE * file = fopen(argv[1], "r");
    size_t n = 1024;
    struct board board;
    move_t move;
    engine_init(0);
    n = 0;
    char * cursor;

    while (getline(&buffer, &n, file) > 0) {
        strcpy(line, buffer);
        board_init(&board);
        // For now, forget about trick lines
        cursor = buffer;
        if (*cursor != '1') {
            continue;
        }
        cursor += 2;
        char * token;
        char who = 0;
        token = strtok(cursor, " ");
        while (token) {
            char * c = token;
            if (*c >= '1' && *c <= '9') {
                token = strtok(NULL, " ");
                continue;
            }
            while (*c) {
                if (*c == '?' || *c == '!' || *c == '\n') {
                    *c = 0;
                    break;
                }
                c++;
            }
            calgebraic_to_move(&board, token, &move);
            if (move.piece == -1) {
                printf("Ambiguous move (%s) in opening: %s\n", token, line);
                break;
            }
            if (!is_valid_move(&board, who, move)) {
                printf("Invalid move (%s) in opening: %s\n", token, line);
                break;
            }
            opening_table_update(board.hash, move, 0);
            apply_move(&board, &move);
            who = 1-who;
            token = strtok(NULL, " ");
        }
    }
    save_opening_table("openings.acebase");
}
