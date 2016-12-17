#ifndef MOVES_H
#define MOVES_H
#include <stdint.h>

#define NORTH 0
#define SOUTH 1
#define EAST 2
#define WEST 3
#define NORTHEAST 4
#define NORTHWEST 5
#define SOUTHEAST 6
#define SOUTHWEST 7

extern char castle_priv[64];
extern uint64_t (*attack_set_pawn[2])(int, uint64_t, uint64_t, uint64_t) ;
extern uint64_t (*attack_set_pawn_capture[2])(int, uint64_t, uint64_t, uint64_t);
extern uint64_t (*attack_set_pawn_multiple_capture[2])(uint64_t, uint64_t, uint64_t);

extern uint64_t (*attack_set_pawn_multiple_capture_movement[2])(uint64_t);

extern uint64_t (*attack_set_pawn_multiple_capture_left[2])(uint64_t, uint64_t, uint64_t);
extern uint64_t (*attack_set_pawn_multiple_capture_right[2])(uint64_t, uint64_t, uint64_t);

extern uint64_t (*attack_set_pawn_multiple_advance_one[2])(uint64_t, uint64_t, uint64_t);
extern uint64_t (*attack_set_pawn_multiple_advance_two[2])(uint64_t, uint64_t, uint64_t);



uint64_t attack_set_pawn_black(int square, uint64_t enpassant, uint64_t friendly_occupancy, uint64_t enemy_occupancy);
uint64_t attack_set_pawn_white(int square, uint64_t enpassant, uint64_t friendly_occupancy, uint64_t enemy_occupancy);


uint64_t ray_attacks_positive(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy, int direction);
uint64_t ray_attacks_negative(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy, int direction);

uint64_t attack_set_knight(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy);
uint64_t attack_set_bishop(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy);
uint64_t attack_set_rook(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy);
uint64_t attack_set_king(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy);
uint64_t attack_set_queen(int square, uint64_t friendly_occupancy, uint64_t enemy_occupancy);

void initialize_move_tables();
uint64_t xray_rook_attacks(int square, uint64_t occ, uint64_t blockers);
uint64_t xray_bishop_attacks(int square, uint64_t occ, uint64_t blockers);

uint64_t ray_between(int square1, int square2);
uint64_t line_between(int square1, int square2);


#endif
