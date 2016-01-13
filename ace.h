/* The ACE Chess Engine (A Chess Engine)
 */
#ifndef ACE_H
#define ACE_H

#define FLAGS_DYNAMIC_DEPTH 1
#define FLAGS_USE_OPENING_TABLE 2
#define FLAGS_UCI_MODE 4

/* Engine functions */
void engine_init(int depth, char flags);
char* engine_init_from_position(char* position, int depth, char flags);
int engine_play();
int engine_move(char* move);
struct board* engine_get_board();
void engine_print();
unsigned char engine_get_who();
int engine_won();

void engine_perft(int initial, int depth, int who, uint64_t* count, uint64_t* enpassants, uint64_t* captures, uint64_t* check, uint64_t* promotions, uint64_t* castles);

#endif
