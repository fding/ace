#include <stdio.h>
#include <stdlib.h>
#include "board.h"

int main(int argc, char* argv[]) {
    char *buffer = malloc(1024);
    FILE * file = fopen(argv[1], "r");
    size_t n = 1024;
    struct board board;
    move_t move;
    engine_init(0, 0);
    n = 0;
    char * cursor;

    while (getline(&buffer, &n, file) > 0) {
        board_init(&board);
        // For now, forget about trick lines
        cursor = buffer;
        printf("%s\n", cursor);
        if (*cursor != '3') {
            continue;
        }
        cursor += 2;
        char * token;
        char who = 0;
        token = strtok(cursor, " ");
        while (token) {
            printf("%llx ", board.hash);
            algebraic_to_move(token, &board, &move);
            opening_table_update(board.hash, move, 0);
            apply_move(&board, who, &move);
            who = 1-who;
            token = strtok(NULL, " ");
        }
        printf("Done\n");
    }
    save_opening_table("openings.acebase");
}
