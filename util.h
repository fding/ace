#ifndef UTIL_H
#include <stdint.h>

#include <stdio.h>

#define UTIL_H

#define LSB(u) ((u) & -(u))
#define LSBINDEX(u) (__builtin_ctzll(u))
#define MSB(u) (0x8000000000000000ull >> __builtin_clzll(u))
#define MSBINDEX(u) (63 - __builtin_clzll(u))


#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

// Count the number of one bits in a 64 bit integer
#define popcnt(bmap) __builtin_popcountll(bmap)

#ifdef DEBUG
#define DPRINTF(...) printf(__VA_ARGS__)
#define SQPRINTF(f, sq, ...) printf(f, 'a' + ((sq) % 8), '1' + ((sq) / 8), __VA_ARGS__)
#else
#define DPRINTF(...)
#define SQPRINTF(f, sq, ...)
#endif

uint64_t rand64(void);
void rand64_seed(uint64_t seed);

#endif
