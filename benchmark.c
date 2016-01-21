/* Chess Engine
 * Protocol:
 * W/B (if engine is White or Black)
 * n: number of moves to play in advance
 * 2*n lines of moves
 * white's move if engine is Black
 * Engine outputs move to standard out
 *
 * Components:
 * board.c: manages the board, parser for moves, move output
 * suggest.c: suggests a move to take
 */

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "ace.h"

int main(int argc, char* argv[]) {
    int depth = 6;
    int i = 0;

    engine_init(depth, 0);
    clock_t start = clock();
    engine_new_game();
    for (i = 0; i < 20; i++) {
        printf("Move %d\n", i);
        engine_play();
    }
    clock_t end = clock();
    printf("Time: %lu milliseconds\n", (end - start) * 1000 / CLOCKS_PER_SEC);
}
