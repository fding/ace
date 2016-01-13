#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

#include "ace.h"

/* Positions are obtained from https://chessprogramming.wikispaces.com/Perft+Results */
char* positions[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    // From the immortal game
    "rnb1kb1r/p2p1ppp/2p2n2/1B3Nq1/4PpP1/3P4/PPP4P/RNBQ1KR1 b kq - 0 11",
    // Another one
    "r1bk2nr/p2p1pNp/n2B1Q2/1p1NP2P/6P1/3P4/P1P1K3/q5b1 b - - 0 22",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "1K1Q4/8/2n5/8/8/8/8/7k w - - 0 1",
};

static struct option long_options[] = {
    {"starting",  required_argument, 0, 's'},
    {"depth",  required_argument, 0, 'd'},
    {0, 0, 0, 0}
};

int main(int argc, char* argv[]) {
    int c;
    int depth;
    char position[256] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, "s:d:", long_options, &option_index);
  
        if (c == -1)
            break;
        switch (c) {
            case 's':
                strcpy(position, optarg);
                break;
            case 'd':
                depth = atoi(optarg) - 1;
            case '?':
                break;
  
            default:
                abort ();
          }
    }
    engine_init_from_position(position, 0, 0);
    int score = engine_score();
    engine_print();
    printf("Score: %d\n", score);
}
