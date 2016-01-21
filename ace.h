/* The ACE Chess Engine (A Chess Engine)
 */
#ifndef ACE_H
#define ACE_H

#define FLAGS_DYNAMIC_DEPTH 1
#define FLAGS_USE_OPENING_TABLE 2
#define FLAGS_UCI_MODE 4

/* Engine functions */
void engine_init(int depth, int flags);
void engine_new_game();
char* engine_new_game_from_position(char* position);
int engine_play();
int engine_ponder();
void engine_stop_search();
int engine_move(char* move);
struct board* engine_get_board();
void engine_print();
unsigned char engine_get_who();
int engine_won();

void engine_perft(int initial, int depth, int who, uint64_t* count, uint64_t* enpassants, uint64_t* captures, uint64_t* check, uint64_t* promotions, uint64_t* castles);

#endif
