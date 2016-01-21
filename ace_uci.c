#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ace.h"

#include <pthread.h>
#include <semaphore.h>

sem_t available_threads;
pthread_t search_thread;
void* launch_search_thread(void * argument) {
    (void) argument;
    engine_play();
    sem_post(&available_threads);
}
void* launch_ponder_thread(void * argument) {
    (void) argument;
    engine_ponder();
    sem_post(&available_threads);
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);
    char *buffer = malloc(4046);
    sem_init(&available_threads, 0, 1);
    size_t n;
    memset(buffer, 0, 4096);
    FILE * f = fopen("log.txt", "a");

    engine_init(7, FLAGS_UCI_MODE | FLAGS_DYNAMIC_DEPTH | FLAGS_USE_OPENING_TABLE);
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
                engine_new_game();
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
                    if (!(*pos)) break;
                    pos = engine_new_game_from_position(pos);
                }
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
                token = strtok(NULL, " ");
                if (token != NULL && strcmp(token, "ponder") == 0) {
                    engine_stop_search();
                    pthread_create(&search_thread, NULL, launch_ponder_thread, NULL);
                    pthread_detach(search_thread);
                    break;
                }
                engine_stop_search();
                sem_wait(&available_threads);
                pthread_create(&search_thread, NULL, launch_search_thread, NULL);
                pthread_detach(search_thread);
                break;
            }
            else if (strcmp(token, "eath") == 0) {
                break;
            }
            else if (strcmp(token, "stop") == 0) {
                engine_stop_search();
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
