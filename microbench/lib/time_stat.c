#include "time_stat.h"
#include <math.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

void time_stats_init(struct time_stats *ts, int n)
{
	ts->time_v = (double *)malloc(sizeof(double) * n);
	ts->n = n;
	ts->count = 0;
}

void time_stats_destroy(struct time_stats *ts)
{
	free(ts->time_v);
}

inline void time_stats_start(struct time_stats *ts)
{
	clock_gettime(CLOCK_MONOTONIC, &ts->time_start);
}

inline void time_stats_stop(struct time_stats *ts)
{
	struct timespec end;
	clock_gettime(CLOCK_MONOTONIC, &end);
	double start_sec =
		(double)(ts->time_start.tv_sec * 1000000000.0 + (double)ts->time_start.tv_nsec) / 1000000000.0;
	double end_sec = (double)(end.tv_sec * 1000000000.0 + (double)end.tv_nsec) / 1000000000.0;

	double sec = end_sec - start_sec;

	ts->time_v[ts->count++] = sec;
}

inline void time_stats_pause(struct time_stats *ts)
{
	struct timespec end;
	clock_gettime(CLOCK_MONOTONIC, &end);
	double start_sec =
		(double)(ts->time_start.tv_sec * 1000000000.0 + (double)ts->time_start.tv_nsec) / 1000000000.0;
	double end_sec = (double)(end.tv_sec * 1000000000.0 + (double)end.tv_nsec) / 1000000000.0;

	double sec = end_sec - start_sec;

	ts->time_v[ts->count] += sec;
}

double time_stats_get_avg(struct time_stats *ts)
{
	double sum = 0.0;
	for (int i = 0; i < ts->n; i++) {
		sum += ts->time_v[i];
	}
	return (double)sum / ts->n;
}

static int compare_time(const void *a, const void *b)
{
	if ((*(double *)a) > (*(double *)b)) {
		return 1;
	} else if (*(double *)a == *(double *)b) {
		return 0;
	} else {
		return -1;
	}
}

double time_stats_get_sum(struct time_stats *ts)
{
	double sum = 0.0;

	for (int i = 0; i < ts->count; i++) {
		sum += ts->time_v[i];
	}

	return sum;
}

void time_stats_print(struct time_stats *ts, const char *msg)
{
	double sum = 0.0;
	double min = 0.0, max = 0.0, lat_50 = 0.0, lat_99 = 0.0, lat_99_9 = 0.0, lat_99_99, lat_99_999 = 0.0;
	int _50, _99, _99_9, _99_99, _99_999;

	_50 = (int)((float)ts->count * 0.5);
	_99 = (int)((float)ts->count * 0.99);
	_99_9 = (int)((float)ts->count * 0.999);
	_99_99 = (int)((float)ts->count * 0.9999);
	_99_999 = (int)((float)ts->count * 0.99999);

	qsort(ts->time_v, ts->count, sizeof(double), compare_time);

	for (int i = 0; i < ts->count; i++) {
		if (i == _50)
			lat_50 = ts->time_v[i];
		else if (i == _99)
			lat_99 = ts->time_v[i];
		else if (i == _99_9)
			lat_99_9 = ts->time_v[i];
		else if (i == _99_99)
			lat_99_99 = ts->time_v[i];
		else if (i == _99_999)
			lat_99_999 = ts->time_v[i];

		sum += ts->time_v[i];

		if (ts->time_v[i] > max) {
			max = ts->time_v[i];
		}

		if (ts->time_v[i] < min || min == 0.0) {
			min = ts->time_v[i];
		}
	}
	double avg = sum / ts->count;

	double sum1 = 0.0;
	for (int i = 0; i < ts->count; i++) {
		sum1 += pow((ts->time_v[i] - avg), 2);
	}

	double variance = sum1 / ts->count;
	double std = sqrt(variance);

	if (ts->count >= 2) {
		printf("%s (samples: %d)\n", msg, ts->count);
		printf("\tavg: %.3f msec (%.2f usec)\n", avg * 1000.0, avg * 1000000.0);
		printf("\tmin: %.3f msec (%.2f usec)\n", min * 1000.0, min * 1000000.0);
		printf("\tmax: %.3f msec (%.2f usec)\n", max * 1000.0, max * 1000000.0);
		printf("\tstd: %.3f msec (%.2f usec)\n", std * 1000.0, std * 1000000.0);
		if (ts->count >= 10)
			printf("\t50 percentile    : %.3f msec (%.2f usec)\n", lat_50 * 1000.0, lat_50 * 1000000.0);
		if (ts->count >= 100)
			printf("\t99 percentile    : %.3f msec (%.2f usec)\n", lat_99 * 1000.0, lat_99 * 1000000.0);
		if (ts->count >= 1000)
			printf("\t99.9 percentile  : %.3f msec (%.2f usec)\n", lat_99_9 * 1000.0, lat_99_9 * 1000000.0);
		if (ts->count >= 10000)
			printf("\t99.99 percentile : %.3f msec (%.2f usec)\n", lat_99_99 * 1000.0,
			       lat_99_99 * 1000000.0);
		if (ts->count >= 100000)
			printf("\t99.999 percentile: %.3f msec (%.2f usec)\n", lat_99_999 * 1000.0,
			       lat_99_999 * 1000000.0);
	} else {
		printf("%s: %.3f msec (%.2f usec)\n", msg, avg * 1000.0, avg * 1000000.0);
	}
}
