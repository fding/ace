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
#include <locale.h>

static struct option long_options[] = {
    {"white",  required_argument, 0, 'w'},
    {"black",  required_argument, 0, 'b'},
    {"wtime",  required_argument, 0, 1},
    {"btime",  required_argument, 0, 2},
    {"winc",  required_argument, 0, 3},
    {"binc",  required_argument, 0, 4},
    {"movestogo",  required_argument, 0, 5},
    {"starting",  required_argument, 0, 's'},
    {0, 0, 0, 0}
};

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "en_US.UTF-8");
    char position[256] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    int c;
    int whitecomp, blackcomp;
    
    int wstarttime, bstarttime, wtime, btime, winc, binc, reset_period, movestogo;
    wstarttime = 60000 * 5;
    bstarttime = 60000 * 5;
    winc = 100;
    binc = 100;
    reset_period = 0;

    int depth = 7;
    whitecomp = 0;
    blackcomp = 0;
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, "w:b:s:d:", long_options, &option_index);
  
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
                    fprintf(stderr, "option --white should be human or computer");
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
                    fprintf (stderr, "option --white should be human or computer");
                    abort();
                }
                break;
            case 'd':
                depth = atoi(optarg);
                break;
            case 's':
                strcpy(position, optarg);
                break;
            case '?':
                break;
            case 1:
                wstarttime = atoi(optarg);
                break;
            case 2:
                bstarttime = atoi(optarg);
                break;
            case 3:
                winc = atoi(optarg);
                break;
            case 4:
                binc = atoi(optarg);
                break;
            case 5:
                reset_period = atoi(optarg);
                break;
  
            default:
                abort ();
          }
    }

    char buffer[8];
    fprintf(stderr, "Running ACE, with ");
    if (whitecomp) fprintf(stderr, "white player as computer and ");
    else fprintf(stderr, "white player as human and ");
    if (blackcomp) fprintf(stderr, " black player as computer. \n");
    else fprintf(stderr, " black player as human. \n");

    engine_init(depth, FLAGS_DYNAMIC_DEPTH | FLAGS_USE_OPENING_TABLE);

    engine_new_game_from_position(position);
    int should_play[2] = {whitecomp, blackcomp};


    movestogo = reset_period;
    wtime = wstarttime;
    btime = bstarttime;
    while (!engine_won()) {
        if (movestogo == 0 && reset_period != 0) {
            movestogo = reset_period;
            wtime = wstarttime;
            btime = bstarttime;
        }
        if (engine_get_who()) {
            btime += binc;
            if (reset_period != 0)
                movestogo -= 1;
        } else {
            wtime += winc;
        }
        clock_t start = clock();
        fprintf(stderr, "\n\n");
        engine_print();
        fflush(stderr);
        if (should_play[engine_get_who()]) {
            char move[8];
            engine_search(move, 0, wtime, btime, winc, binc, movestogo);
            printf("%s\n", move);
            fflush(stdout);
            engine_move(move);
        }
        else {
            while (1) {
                if (scanf("%s", buffer) < 1) {
                    fprintf(stderr, "EOF");
                    abort();
                }
                if (strcmp(buffer, "exit") == 0)
                    exit(0);
                if (!engine_move(buffer)) {
                    break;
                }
                fprintf(stderr, "Invalid move (%s)!\n", buffer);
            }
        }
        int elapsed = (clock() - start) * 1000 / CLOCKS_PER_SEC;
        if (1 - engine_get_who()) {
            btime -= elapsed;
        } else {
            wtime -= elapsed;
        }
        fprintf(stderr, "White has %d ms, Black has %d ms\n", wtime, btime);
    }

    engine_print();
    fflush(stderr);
    if (engine_won() == 2) {
        fprintf(stderr, "White won\n");
        return 2;
    } else if (engine_won() == 3) {
        fprintf(stderr, "Black won\n");
        return 1;
    } else {
        fprintf(stderr, "Draw\n");
        return 0;
    }
}
