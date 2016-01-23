#ifndef UTIL_H
#include <stdint.h>

#define UTIL_H

#define LSB(u) ((u) & -(u))
#define LSBINDEX(u) (__builtin_ctzll(u))
#define MSB(u) (0x8000000000000000ull >> __builtin_clzll(u))
#define MSBINDEX(u) (63 - __builtin_clzll(u))

uint64_t rand64(void);
void rand64_seed(uint64_t seed);

#define bitmap_count_ones(bmap) __builtin_popcountll(bmap)



#endif
