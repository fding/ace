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

#include "ace.h"

static struct option long_options[] = {
    {"white",  required_argument, 0, 'w'},
    {"black",  required_argument, 0, 'b'},
    {"depth",  required_argument, 0, 'd'},
    {0, 0, 0, 0}
};

int main(int argc, char* argv[]) {
    int c;
    int whitecomp, blackcomp;
    int depth = 6;
    whitecomp = 0;
    blackcomp = 0;
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, "w:b:", long_options, &option_index);
  
        if (c == -1)
            break;
        switch (c) {
            case 'w':
                if (strcmp(optarg, "human") == 0)
                    whitecomp = 0;
                else if (strcmp(optarg, "h") == 0)
                    whitecomp = 0;
                else if (strcmp(optarg, "computer") == 0)
                    whitecomp = 1;
                else if (strcmp(optarg, "c") == 0)
                    whitecomp = 1;
                else if (strcmp(optarg, "engine") == 0)
                    whitecomp = 1;
                else {
                    printf ("option --white should be human or computer");
                    abort();
                }
                break;
            case 'b':
                if (strcmp(optarg, "human") == 0)
                    blackcomp = 0;
                else if (strcmp(optarg, "h") == 0)
                    blackcomp = 0;
                else if (strcmp(optarg, "computer") == 0)
                    blackcomp = 1;
                else if (strcmp(optarg, "c") == 0)
                    blackcomp = 1;
                else if (strcmp(optarg, "engine") == 0)
                    blackcomp = 1;
                else {
                    printf ("option --white should be human or computer");
                    abort();
                }
                break;
            case 'd':
                depth = atoi(optarg);
                break;
            case '?':
                break;
  
            default:
                abort ();
          }
    }

    char buffer[8];
    engine_init(depth, FLAGS_DYNAMIC_DEPTH | FLAGS_USE_OPENING_TABLE);
    while (!engine_won()) {
        engine_print();
        if (whitecomp)
            engine_play();
        else {
            while (1) {
                scanf("%s", buffer);
                if (!engine_move(buffer)) {
                    break;
                }
                printf("Invalid move!\n");
            }
        }
        if (engine_won())  {
            break;
        }
        engine_print();

        if (blackcomp)
            engine_play();
        else {
            while (1) {
                scanf("%s", buffer);
                if (!engine_move(buffer)) {
                    break;
                }
                printf("Invalid move!\n");
            }
        }
    }

    engine_print();
    if (engine_won() == 2) {
        printf("White won\n");
    } else if (engine_won() == 3) {
        printf("Black won\n");
    } else {
        printf("Draw\n");
    }
}
