#include "util.h"
/* The state must be seeded so that it is not everywhere zero. */
int p = 0;

uint64_t s[16] = {
    0x123456789abcdef0ull,
    0x2eab0093287c1d11ull,
    0x18ae739cb0aa9301ull,
    0xa31a1b1582123a11ull,
    0x819a0a9916273eacull,
    0x74578b20199a8374ull,
    0x5738295d930ff847ull,
    0xf38d7c9dead03911ull,
    0x0aab930847292413ull,
    0xc136480012984021ull,
    0xe920490d89190cc1ull,
    0xc361161034920591ull,
    0x92078ca398d93013ull,
    0x6b9348b734812d98ull,
    0x31239b8930a0e923ull,
    0x412b9c09285f5901ull,
};

uint64_t rand64(void) {
    const uint64_t s0 = s[p];
    uint64_t s1 = s[p = (p + 1) & 15];
    s1 ^= s1 << 31; // a
    s[p] = s1 ^ s0 ^ (s1 >> 11) ^ (s0 >> 30); // b,c
    return s[p] * 1181783497276652981ull;
}

void rand64_seed(uint64_t seed) {
    s[0] = 0x123456789abcdef0ull ^ seed;
    p = 0;
    rand64();
}

static uint64_t rand64u(void) {
    static uint64_t next = 0x1820381947542809;
 
    next = next * 1103515245 + 12345;
    return next;
}

