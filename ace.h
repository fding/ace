/* The ACE Chess Engine (A Chess Engine)
 */
#ifndef ACE_H

#include <assert.h>

#define ACE_H

#define FLAGS_DYNAMIC_DEPTH 1
#define FLAGS_USE_OPENING_TABLE 2
#define FLAGS_UCI_MODE 4

#define GAME_UNDETERMINED 0
#define GAME_DRAW 1
#define GAME_WHITE_WON 2
#define GAME_BLACK_WON 3

#define WHITE 0
#define BLACK 1

typedef uint8_t side_t;

/* Engine functions */
void engine_init(int flags);
int engine_reset_hashmap(int hashsize);


#define ACE_PARAM_CONTEMPT 1
#define ACE_PARAM_DEBUG 2
int engine_set_param(int name, int value);


void engine_new_game();
void engine_clear_state();
char* engine_new_game_from_position(char* position);
int engine_play();
int engine_ponder();
void engine_stop_search();
int engine_move(char* move);
struct board* engine_get_board();
void engine_print();
void engine_print_moves();
unsigned char engine_get_who();
int engine_won();
int engine_score();
int engine_search(char * move, int infinite_mode, int wtime, int btime, int winc, int binc, int moves_to_go);

void engine_perft(int initial, int depth, side_t who, uint64_t* count, uint64_t* enpassants, uint64_t* captures, uint64_t* check, uint64_t* promotions, uint64_t* castles, int eval, int* eval_score);

extern int debug_mode;

#endif
