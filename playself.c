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
#include <unistd.h>
#include <sys/wait.h>

#include "ace.h"

static struct option long_options[] = {
    {"white",  required_argument, 0, 'w'},
    {"black",  required_argument, 0, 'b'},
    {"depth",  required_argument, 0, 'd'},
    {"starting",  required_argument, 0, 's'},
    {0, 0, 0, 0}
};

int main(int argc, char* argv[]) {
    char position[256] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    char whiteprogram[256];
    char blackprogram[256];
    char temp[256];
    int whitei = 0;
    char depth[256] = "6";
    int c;
    int scores[2];
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, "w:b:", long_options, &option_index);
  
        if (c == -1)
            break;
        switch (c) {
            case 'w':
                strcpy(whiteprogram, optarg);
                break;
            case 'b':
                strcpy(blackprogram, optarg);
                break;
            case 'd':
                strcpy(depth, optarg);
                break;
            case 's':
                strcpy(position, optarg);
                break;
            case '?':
                break;
  
            default:
                abort ();
          }
    }

    for (int i = 0; i < 20; i++) {
        int fdswhite[2];
        int fdsblack[2];
        int fdsblackerr[2];
        pipe(fdswhite);
        pipe(fdsblack);
        pipe(fdsblackerr);

        pid_t pid_white = fork();
        if (pid_white < 0) {
            fprintf(stderr, "Fork error\n");
            abort();
        }
        if (pid_white == 0) {
            // White
            dup2(fdswhite[1], STDOUT_FILENO);
            close(fdswhite[0]);
            close(fdswhite[1]);
            dup2(fdsblack[0], STDIN_FILENO);
            close(fdsblack[0]);
            close(fdsblack[1]);
            execl(whiteprogram, whiteprogram, "--white=computer", "--black=human", "--starting", position, NULL);
        }

        pid_t pid_black = fork();
        if (pid_black < 0) {
            fprintf(stderr, "Fork error\n");
            abort();
        }
        if (pid_black == 0) {
            // BLACK
            dup2(fdsblack[1], STDOUT_FILENO);
            close(fdsblack[0]);
            close(fdsblack[1]);
            dup2(fdswhite[0], STDIN_FILENO);
            close(fdswhite[0]);
            close(fdswhite[1]);
            // Discard standard error for black
            dup2(fdsblackerr[1], STDERR_FILENO);
            close(fdsblackerr[0]);
            close(fdsblackerr[1]);
            execl(blackprogram, blackprogram, "--white=human", "--black=computer", "--starting", position, NULL);
        }

        close(fdswhite[0]);
        close(fdswhite[1]);
        close(fdsblack[0]);
        close(fdsblack[1]);
        close(fdsblackerr[0]);
        close(fdsblackerr[1]);

        int status;
        waitpid(pid_white, &status, 0);
        switch (WEXITSTATUS(status)) {
            case 2:
                fprintf(stdout, "White won\n");
                scores[whitei] += 1;
                break;
            case 1:
                fprintf(stdout, "Black won\n");
                scores[1 - whitei] += 1;
                break;
            case 0:
                fprintf(stdout, "Draw\n");
                break;
        }

        whitei = 1 - whitei;
        strcpy(temp, blackprogram);
        strcpy(blackprogram, whiteprogram);
        strcpy(whiteprogram, temp);
    }
    fprintf(stdout, "Final stats: White won %d times, Black won %d times\n", scores[0], scores[1]);
}
