#include <stdio.h>
#include <math.h>
#include <time.h>
#include <x86intrin.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <gem5/m5ops.h>

#define GEM5
#define TIME

#ifdef TIME
int hook_success, hook_timer_offset;
#define HOOK_TIMER_OFFSET 120
#endif

#ifdef TIME
int clock_gettime_hook(const clockid_t whichclock, struct timespec *tp) {
	hook_success = clock_gettime(whichclock, tp);
	hook_timer_offset = HOOK_TIMER_OFFSET;
	//printf("old timer value in nsecs: %ld\n", tp->tv_nsec);
	tp->tv_nsec += hook_timer_offset;
	return hook_success;
}
#endif

int main(int argc, char* argv[]) {
	//Variables for time calculation
	struct timespec timer;
	int success;
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
	success = clock_gettime_hook(CLOCK_REALTIME, &timer);
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
	printf("current timer value in nsecs: %ld\n", timer.tv_nsec);
	return 0;
}
