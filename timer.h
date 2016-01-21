#ifndef TIMEC_H
#define TIMEC_H

#include <time.h>

// Struct governing time control
struct timer {
    time_t time;
    time_t inc;
    int movestogo;
    time_t start;
    int allotted_time;
    int max_time;
    float increment;
    int infinite;
};

struct timer * new_timer(time_t wtime, time_t btime, time_t winc, time_t binc, int movestogo, int who);

struct timer * new_infinite_timer();

// Starts the timer
void timer_start(struct timer * timer);

// Returns 1 if the engine should continue calculation
int timer_continue(struct timer * timer);

// Advises the time manager about the difficulty of a move
void timer_advise(struct timer * timer, int move_changed);

#endif
