#include <stdio.h>

/* need these for getrusage version of get_time
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
*/

/* need for times() version of get_time
#include <sys/times.h>
#include <unistd.h>
*/

// for get_time_of_day
#include <sys/time.h>

double get_time(void)
//some code taken from Victor Shoup's NTL library
{
    /*
    //getrusage() version
    struct rusage used;

    getrusage(RUSAGE_SELF, &used);
    return (used.ru_utime.tv_sec + used.ru_stime.tv_sec +
	(used.ru_utime.tv_usec + used.ru_stime.tv_usec) / 1e6);
    */

    /* times() version
    static clock_t last_clock = 0;
    static double acc = 0;
    struct tms used;

    clock_t this_clock;
    double delta;

    times(&used);
    this_clock = used.tms_utime + used.tms_stime;

    if (last_clock == 0) {
	delta = 0;
    } else {
	delta = (this_clock - last_clock)/((double)sysconf(_SC_CLK_TCK));
	if (delta < 0) delta = 0;
    }

    acc += delta;
    last_clock = this_clock;

    return acc;
    */

    static struct timeval last_tv, tv;
    static int first = 1;
    static double res = 0;

    if (first) {
	gettimeofday(&last_tv, NULL);
	first = 0;
	return 0;
    } else {
	gettimeofday(&tv, NULL);
	res += tv.tv_sec - last_tv.tv_sec;
	res += (tv.tv_usec - last_tv.tv_usec) / 1000000.0;
	last_tv = tv;

	return res;
    }
}
