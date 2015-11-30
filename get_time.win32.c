#include <stdio.h>
#include <time.h>

double get_time(void)
//some code taken from Victor Shoup's NTL library
{
    static clock_t last_clock = 0;
    static double acc = 0;

    clock_t this_clock;
    double delta;

    this_clock = clock();

    if (last_clock == 0) {
	delta = 0;
    } else {
	delta = (this_clock - last_clock)/((double)CLOCKS_PER_SEC);
	if (delta < 0) delta = 0;
    }

    acc += delta;
    last_clock = this_clock;

    return acc;
}
