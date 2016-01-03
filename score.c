#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "ace.h"

/* Positions are obtained from https://chessprogramming.wikispaces.com/Perft+Results */
char* positions[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r1bqkbnr/pppppppp/2n5/8/8/2N5/PPPPPPPP/R1BQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "1K1Q4/8/2n5/8/8/8/8/7k w - - 0 1",
};

int main(int argc, char* argv[]) {
    int position_i = atoi(argv[1]);
    char buffer[8];
    engine_init_from_position(positions[position_i], 0);
    int score = engine_score();
    printf("Score: %d\n", score);
}
