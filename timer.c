#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "timer.h"
#include "util.h"

struct timer * new_timer(time_t wtime, time_t btime, time_t winc, time_t binc, int movestogo, int who) {
    struct timer * timer = malloc(sizeof(struct timer));
    if (!who) {
        timer->time = wtime * CLOCKS_PER_SEC / 1000;
        timer->inc = winc * CLOCKS_PER_SEC / 1000;
    } else {
        timer->time = btime * CLOCKS_PER_SEC / 1000;
        timer->inc = binc * CLOCKS_PER_SEC / 1000;
    }

    timer->movestogo = movestogo;
    timer->start = 0;
    if (movestogo) {
        timer->allotted_time = timer->time / movestogo;
        timer->max_time = MIN(2 * timer->time / movestogo, timer->time / MIN(2, movestogo));
        timer->increment = 1.1;
    } else {
        timer->allotted_time = timer->time / 25;
        timer->max_time = timer->time / 12;
        timer->increment = 1.1;
    }
    timer->infinite = 0;
    return timer;
}

struct timer * new_infinite_timer() {
    struct timer* timer = new_timer(86400000, 86400000, 0, 0, 1, 0);
    timer->increment = 1;
    timer->infinite = 1;
    return timer;
}

void timer_start(struct timer* timer) {
    timer->start = clock();
}

int timer_continue(struct timer * timer) {
    if (timer->infinite) return 1;
    time_t now = clock();
    if (now > timer->start + timer->allotted_time) {
        return 0;
    }
    return 1;
}

// Award increments if there is large fluctuation,
// deduct increments if there is little fluctuation
void timer_advise(struct timer * timer, int move_changed) {
    if (move_changed)
        timer->allotted_time = MIN(timer->max_time, timer->allotted_time * timer->increment * timer->increment);
    /*
    else
        timer->allotted_time = MIN(timer->max_time, timer->allotted_time / timer->increment);
    */
}

