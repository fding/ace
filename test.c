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

#include "board.h"
#include <stdio.h>

int main() {
    char buffer[8];
    engine_init(0);
    while (1) {
        if (scanf("%s", buffer) < 1) break;
        if (engine_move(buffer)) {
            engine_print();
            return -1;
        }
    }
    engine_print();
}
