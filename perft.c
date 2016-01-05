#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

#include "ace.h"



/* Positions are obtained from https://chessprogramming.wikispaces.com/Perft+Results */
char* positions[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    /* Position 0. Correct answers:
     * Depth       Nodes  Captures   E.p.  Castles  Promotions   Checks
     *     1          20         0      0        0           0        0
     *     2         400         0      0        0           0        0
     *     3        8902        34      0        0           0       12
     *     4      197281      1576      0        0           0      469
     *     5     4865609     82719    258        0           0    27351
     *     6   119060324   2812008   5248        0           0   809099
     */
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    /* Position 1. Correct answers:
     * Depth       Nodes  Captures   E.p.  Castles  Promotions   Checks
     *     1          48         8      0        2           0        0
     *     2        2039       351      1       91           0        3
     *     3       97862     17102     45     3162           0      993
     *     4     4085603    757163   1929   128013       15172    25523
     *     5   193690690  35043416  73365  4993637        8392  3309887
     */
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    /* Position 2. Correct answers:
     * Depth       Nodes  Captures   E.p.  Castles  Promotions   Checks
     *     1          14         1      0        0           0        2
     *     2         191        14      0        0           0       10
     *     3        2812       209      2        0           0      267
     *     4       43238      3348    123        0           0     1680
     *     5      674624     52051   1165        0           0    52950
     *     6    11030083    940350  33325        0        7552   452473
     */
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    /* Position 3. Correct answers:
     * Depth       Nodes  Captures   E.p.  Castles  Promotions   Checks
     *     1           6         0      0        0           0        0
     *     2         264        87      0        6          48       10
     *     3        9467      1021      4        0         120       38
     *     4      422333    131393      0     7795       60032    15492
     *     5    15833292   2046173   6512        0      329464   200568
     *     6   706045033 210369132    212 10882006    81102984 26973664
     */
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    /* Position 4. Correct answers:
     * Depth       Nodes
     *     1          44
     *     2        1486
     *     3       62379
     *     4     2103487
     *     5    89941194
     */
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    /* Position 5. Correct answers:
     * Depth       Nodes
     *     1          46
     *     2        2079
     *     3       89890
     *     4     3894594
     *     5   164075551
     */
    "Kqk5/8/8/8/8/8/8/8 w - - 0 10"
    /* Position 6. Correct answers
     * Depth       Nodes
     *     1           1
     */
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

    engine_init_from_position(position, depth, 0);
    engine_print();
    clock_t start = clock();
    uint64_t count, enpassants, captures, check, promotions, castles;
    count = 0;
    enpassants = 0;
    captures = 0;
    check = 0;
    castles = 0;
    promotions = 0;
    engine_perft(depth, engine_get_who(), &count, &enpassants, &captures, &check, &promotions, &castles);
    clock_t end = clock();
    printf("Perft: %llu nodes in %.2f s (%.2f moves/sec)\n"
            "%llu captures, %llu enpassants, %llu checks, %llu promotions, %llu castles\n",
            count,
            (end - start) * 1.0/ CLOCKS_PER_SEC,
            count/(((float) (end - start)) / CLOCKS_PER_SEC),
            captures, enpassants, check, promotions, castles
            );
}
