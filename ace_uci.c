#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ace.h"

#include <pthread.h>
#include <semaphore.h>

struct timec {
    int wtime;
    int btime;
    int winc;
    int binc;
    int moves_to_go;
};

sem_t available_threads;
pthread_t search_thread;
int board_initialized = 0;
int debug_mode = 0;

void * launch_search_thread(void * argument) {
    struct timec* arg = (struct timec *) argument;
    char buffer[8];
    engine_search(buffer, 0, arg->wtime, arg->btime, arg->winc, arg->binc, arg->moves_to_go);
    printf("bestmove %s\n", buffer);
    fflush(stdout);
    sem_post(&available_threads);
    return NULL;
}

void * launch_ponder_thread(void * argument) {
    (void) argument;
    char buffer[8];
    engine_search(buffer, 1, 0, 0, 0, 0, 0);
    printf("bestmove %s\n", buffer);
    fflush(stdout);
    sem_post(&available_threads);
    return NULL;
}

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);
    char *buffer = malloc(4096);
    assert(buffer);
    sem_init(&available_threads, 0, 1);
    size_t n;
    memset(buffer, 0, 4096);
    FILE * f = fopen("log.txt", "a");

    engine_init(FLAGS_UCI_MODE | FLAGS_DYNAMIC_DEPTH | FLAGS_USE_OPENING_TABLE);
    engine_reset_hashmap(1 << 26);
    while (getline(&buffer, &n, stdin) > 0) {
        fprintf(f, "%s", buffer);
        fflush(f);
        n = strlen(buffer);
        if (n > 4096) {
            fprintf(stderr, "Line too long\n");
            exit(1);
        }
        buffer[n - 1] = 0;
        char * token;
        token = strtok(buffer, " ");
        while (token) {
            if (strcmp(token, "uci") == 0) {
                printf("id name ACE\n");
                printf("id author D. Ding\n\n");
                printf("option name Hash type spin default 1024 min 1 max 8096\n");
                printf("option name Ponder type check default true\n");
                printf("option name OwnBook type check default true\n");
                printf("option name Contempt type spin default 0 min -100 max 100\n");
                printf("uciok\n");
                break;
            }
            else if (strcmp(token, "debug") == 0) {
                // Not implemented
                token = strtok(NULL, " ");
                if (token && strcmp(token, "on") == 0) {
                    engine_set_param(ACE_PARAM_DEBUG, 1);
                    if (debug_mode)
                        printf("info string debug mode on\n");
                } else {
                    engine_set_param(ACE_PARAM_DEBUG, 0);
                }
                break;
            }
            else if (strcmp(token, "isready") == 0) {
                printf("readyok\n");
                break;
            }
            else if (strcmp(token, "setoption") == 0) {
                token = strtok(NULL, " ");
                if (!token || strcmp(token, "name")) break;
                token = strtok(NULL, " ");
                if (strcmp(token, "Hash") == 0) {
                    token = strtok(NULL, " ");
                    if (!token || strcmp(token, "value")) break;
                    token = strtok(NULL, " ");
                    if (!token) break;
                    engine_reset_hashmap(atoi(token));
                } else if (strcmp(token, "Contempt") == 0) {
                    token = strtok(NULL, " ");
                    if (!token || strcmp(token, "value")) break;
                    token = strtok(NULL, " ");
                    if (!token) break;
                    engine_set_param(ACE_PARAM_CONTEMPT, atoi(token));
                }
                break;
            }
            else if (strcmp(token, "register") == 0) {
                // Not implemented
                break;
            }
            else if (strcmp(token, "ucinewgame") == 0) {
                engine_clear_state();
                engine_new_game();
                board_initialized = 1;
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
                    engine_new_game();
                    *pos = temp;
                } else {
                    pos = token + 4;
                    while (*pos && *pos == ' ') pos++;
                    char * old_fen = pos;
                    if (!(*pos)) break;
                    pos = engine_new_game_from_position(pos);
                    if (!pos) {
                        printf("info string Invalid board fen: %s\n", old_fen);
                        break;
                    }
                }
                board_initialized = 1;
                token = strtok(pos, " ");
                if (!token) break;
                if (strcmp(token, "moves") != 0) break;
                while ((token = strtok(NULL, " "))) {
                    if (engine_move(token))
                        fprintf(stderr, "Bad move: %s\n", token);
                }
                break;
            }
            else if (strcmp(token, "go") == 0) {
                if (!board_initialized) {
                    printf("info string board not initialized\n");
                    break;
                }
                token = strtok(NULL, " ");
                if (token != NULL && strcmp(token, "ponder") == 0) {
                    engine_stop_search();
                    sem_wait(&available_threads);
                    pthread_create(&search_thread, NULL, launch_ponder_thread, NULL);
                    pthread_detach(search_thread);
                    break;
                } else if (token != NULL && strcmp(token, "infinite") == 0) {
                    engine_stop_search();
                    sem_wait(&available_threads);
                    pthread_create(&search_thread, NULL, launch_ponder_thread, NULL);
                    pthread_detach(search_thread);
                    break;
                }

                struct timec timec;
                timec.wtime = 8000;
                timec.btime = 8000;
                timec.winc = 0;
                timec.binc = 0;
                timec.moves_to_go = 0;

                while (token != NULL) {
                    if (strcmp(token, "wtime") == 0) {
                        token = strtok(NULL, " ");
                        if (token) {
                            timec.wtime = atoi(token);
                        }
                    }
                    else if (strcmp(token, "btime") == 0) {
                        token = strtok(NULL, " ");
                        if (token) {
                            timec.btime = atoi(token);
                        }
                    }
                    else if (strcmp(token, "winc") == 0) {
                        token = strtok(NULL, " ");
                        if (token) {
                            timec.winc = atoi(token);
                        }
                    }
                    else if (strcmp(token, "binc") == 0) {
                        token = strtok(NULL, " ");
                        if (token) {
                            timec.binc = atoi(token);
                        }
                    }
                    else if (strcmp(token, "movestogo") == 0) {
                        token = strtok(NULL, " ");
                        if (token) {
                            timec.moves_to_go = atoi(token);
                        }
                    }
                    token = strtok(NULL, " ");
                }
                engine_stop_search();
                sem_wait(&available_threads);
                pthread_create(&search_thread, NULL, launch_search_thread, &timec);
                pthread_detach(search_thread);
                break;
            }
            else if (strcmp(token, "eath") == 0) {
                break;
            }
            else if (strcmp(token, "stop") == 0) {
                if (board_initialized)
                    engine_stop_search();
                else
                    printf("info string board not initialized\n");
                break;
            }
            else if (strcmp(token, "ponderhit") == 0) {
                break;
            }
            else if (strcmp(token, "quit") == 0) {
                return 0;
                break;
            } else if (strcmp(token, "score") == 0) {
                if (board_initialized) {
                    int score = engine_score();
                    printf("info string score cp %d\n", score);
                } else {
                    printf("info string board not initialized\n");
                }
            }

            token = strtok(NULL, " ");
        }

        fflush(stdout);
    }
    return 0;
}
