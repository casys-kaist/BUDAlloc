#include "../lib/time_stat.h"
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wait.h>

struct time_stats ts;
extern char **environ;

double time_stats_get_max(struct time_stats *ts)
{
	double max = 0.0;
	for (int i = 0; i < ts->count; i++) {
		if (ts->time_v[i] > max)
			max = ts->time_v[i];
	}
	return max;
}

double time_stats_get_min(struct time_stats *ts)
{
	double min = 0.0;
	for (int i = 0; i < ts->count; i++) {
		if (ts->time_v[i] < min || min == 0.0)
			min = ts->time_v[i];
	}
	return min;
}

int main(int argc, char *argv[])
{
	int do_wait = 0;

	for (int i = 0; i < argc; i++) {
		if (strncmp("--wait", argv[i], 6) == 0)
			do_wait = 1;
		if (strncmp("--sample", argv[i], 8) == 0) {
			return 0;
		}
	}

	char *args[] = { argv[0], "--sample", NULL };
	int count = 1000;
	double fork_avg = 0.0, vfork_avg = 0.0, avg_diff = 0.0, fork_max = 0.0, fork_min = 0.0, vfork_min = 0.0,
	       vfork_max = 0.0, min_diff = 0.0, max_diff = 0.0;

	printf("---------------------------------------------------\n");
	printf("FORK vs VFORK MICROBENCH\n");
	printf("---------------------------------------------------\n");
	time_stats_init(&ts, 1000);
	while (count > 0) {
		time_stats_start(&ts);
		if (fork() == 0)
			execve(args[0], args, environ);
		else {
			if (do_wait)
				wait((int *)0);
			time_stats_stop(&ts);
			count--;
		}
	}
	time_stats_print(&ts, "FORK");
	fork_avg = time_stats_get_avg(&ts);
	fork_max = time_stats_get_max(&ts);
	fork_min = time_stats_get_min(&ts);
	printf("---------------------------------------------------\n");
	count = 1000;
	time_stats_init(&ts, 1000);
	while (count > 0) {
		time_stats_start(&ts);
		if (vfork() == 0)
			execve(args[0], args, environ);
		else {
			if (do_wait)
				wait((int *)0);
			time_stats_stop(&ts);
		}
		count--;
	}
	time_stats_print(&ts, "VFORK");
	vfork_avg = time_stats_get_avg(&ts);
	vfork_min = time_stats_get_min(&ts);
	vfork_max = time_stats_get_max(&ts);
	max_diff = fork_max - vfork_min;
	min_diff = fork_min - vfork_max;
	avg_diff = fork_avg - vfork_avg;
	printf("---------------------------------------------------\n");
	printf("FORK vs VFORK TIME DIFFERENCE\n");
	printf("\tavg: %.3f msec (%.2f usec)\n", avg_diff * 1000.0, avg_diff * 1000000.0);
	printf("\tmax: %.3f msec (%.2f usec)\n", max_diff * 1000.0, max_diff * 1000000.0);
	printf("\tmin: %.3f msec (%.2f usec)\n", min_diff * 1000.0, min_diff * 1000000.0);
	printf("---------------------------------------------------\n");
	return 0;
}
