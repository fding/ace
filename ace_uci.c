#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ace.h"

int main() {
    char *buffer = malloc(4046);
    size_t n;
    memset(buffer, 0, 4096);
    FILE * f = fopen("log.txt", "a");
    while (getline(&buffer, &n, stdin) > 0) {
        fprintf(f, "%s", buffer);
        fflush(f);
        n = strlen(buffer);
        if (n > 4096) exit(1);
        buffer[n - 1] = 0;
        char * token;
        token = strtok(buffer, " ");
        while (token) {
            if (strcmp(token, "uci") == 0) {
                printf("id name ACE\n");
                printf("id author D. Ding\n\n");
                printf("uciok\n");
                break;
            }
            else if (strcmp(token, "debug") == 0) {
                // Not implemented
                break;
            }
            else if (strcmp(token, "isready") == 0) {
                printf("readyok\n");
                break;
            }
            else if (strcmp(token, "setoption") == 0) {
                // Not implemented
                break;
            }
            else if (strcmp(token, "register") == 0) {
                // Not implemented
                break;
            }
            else if (strcmp(token, "ucinewgame") == 0) {
                engine_init(7, FLAGS_UCI_MODE | FLAGS_DYNAMIC_DEPTH | FLAGS_USE_OPENING_TABLE);
                break;
            }
            else if (strcmp(token, "position") == 0) {
                char *pos = token + 9;
                while (*pos && *pos == ' ') pos++;
                if (!pos) break;
                char temp = *(pos + 8);
                token = strtok(NULL, " ");
                if (!token) break;
                pos = token;
                if (strcmp(token, "startpos") == 0) {
                    pos = pos + 8;
                    engine_init(7, FLAGS_UCI_MODE | FLAGS_DYNAMIC_DEPTH | FLAGS_USE_OPENING_TABLE);
                    *pos = temp;
                } else {
                    pos = token + 4;
                    while (*pos && *pos == ' ') pos++;
                    if (!(*pos)) break;
                    pos = engine_init_from_position(pos, 7, FLAGS_UCI_MODE | FLAGS_DYNAMIC_DEPTH | FLAGS_USE_OPENING_TABLE);
                }
                token = strtok(pos, " ");
                if (!token) break;
                if (strcmp(token, "moves") != 0) break;
                while ((token = strtok(NULL, " "))) {
                    if (engine_move(token))
                        fprintf(stderr, "Bad move: %s\n", token);
                    engine_print();
                }
                break;
            }
            else if (strcmp(token, "go") == 0) {
                engine_print();
                token = strtok(NULL, " ");
                if (token != NULL && strcmp(token, "ponder") == 0) break;
                engine_play();
                break;
            }
            else if (strcmp(token, "eath") == 0) {
                break;
            }
            else if (strcmp(token, "stop") == 0) {
                break;
            }
            else if (strcmp(token, "ponderhit") == 0) {
                break;
            }
            else if (strcmp(token, "quit") == 0) {
                exit(0);
                break;
            }

            token = strtok(NULL, " ");
        }

        fflush(stdout);
    }
}
