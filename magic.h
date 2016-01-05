#ifndef MAGIC_H
#define MAGIC_H
#include <stdint.h>

struct magic {
    uint64_t* table;
    uint64_t mask;
    uint64_t magic;
    int shift;
};

extern struct magic bishop_magics[64];
extern struct magic rook_magics[64];
extern uint64_t magic_table[107648];

void initialize_magics(void);

#endif
