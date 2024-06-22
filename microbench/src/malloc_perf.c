#include "../lib/time_stat.h"
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define IO_SIZE (256)
#define LENGTH (1UL * 1024 * 1024 * 1024)
#define PROTECTION (PROT_READ | PROT_WRITE)

#define ADDR (void *)(0x0UL)
#define FLAGS (MAP_ANONYMOUS | MAP_PRIVATE)
#define RAMDOM_SEED 0

#define SECURE_CHECK 1

struct time_stats ts;
void *alloc_list[LENGTH / IO_SIZE];
char buffer[IO_SIZE];
bool init_malloc = false;

static inline uint64_t get_random_val(uint64_t min, uint64_t max)
{
	uint64_t num;
	num = rand();
	num = (num << 32) | rand();
	num = ((num + min) % max);
	return num;
}

static inline void knuth_shuffle(uint64_t *alloc_list)
{
	srand(RAMDOM_SEED);

	for (uint64_t i = 0; i < LENGTH / IO_SIZE - 1; i++) {
		uint64_t array_idx = get_random_val(i, LENGTH / IO_SIZE - 1);
		uint64_t temp = alloc_list[array_idx];
		alloc_list[array_idx] = alloc_list[i];
		alloc_list[i] = temp;
	}
}

static inline int read_io_size(void *addr, char byte)
{
	// printf("0x%lx | 0x%lx | 0x%lx\n", addr, index, addr + index);
	memcpy(buffer, addr, IO_SIZE);
#ifdef SECURE_CHECK
	if (buffer[0] != byte) {
		printf("Mismatch at %lu [%d]\n", (unsigned long)addr, byte);
		return -1;
	}
#endif

	return 0;
}

static inline void write_io_size(void *addr, char byte)
{
	memset(addr, byte, IO_SIZE);
}

static double malloc_bytes(void)
{
	uint64_t i;
	double sum = 0.0;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		alloc_list[i / IO_SIZE] = malloc(IO_SIZE);
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "malloc");

	init_malloc = true;

	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static double calloc_bytes(void)
{
	uint64_t i;
	double sum = 0.0;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		alloc_list[i / IO_SIZE] = calloc(1, IO_SIZE);
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "calloc");

	init_malloc = true;

	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static double free_bytes(void)
{
	uint64_t i;
	double sum = 0.0;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		free(alloc_list[i / IO_SIZE]);
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "free");

	init_malloc = false;

	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static void check_malloc(void)
{
	if (!init_malloc) {
		perror("alloc_list is not initialized");
		exit(-1);
	}
}

static double write_bytes_seq(void)
{
	uint64_t i;
	double sum = 0.0;

	check_malloc();

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		write_io_size(alloc_list[i / IO_SIZE], i);
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "sequential write");
	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static double write_bytes_rnd(void)
{
	uint64_t i;
	double sum = 0.0;

	check_malloc();

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		write_io_size(alloc_list[i / IO_SIZE], i);
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "random write");
	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static double read_bytes_seq(void)
{
	uint64_t i;
	double sum = 0.0;

	check_malloc();

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		if (read_io_size(alloc_list[i / IO_SIZE], i)) {
			return -1;
		}
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "sequential read");
	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static double read_bytes_rnd(void)
{
	uint64_t i;
	double sum = 0.0;

	check_malloc();

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		if (read_io_size(alloc_list[i / IO_SIZE], i)) {
			return -1;
		}
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "random read");
	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static double write_read_bytes(void)
{
	char *buffer[4096 / IO_SIZE];
	uint64_t i;
	double sum = 0.0;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += 4096) {
		for (int j = 0; j < 4096 / IO_SIZE; j++) {
			buffer[j] = malloc(IO_SIZE);
			write_io_size(buffer[j], i);
			if (read_io_size(buffer[j], i)) {
				return -1;
			}
		}
		for (int j = 0; j < 4096 / IO_SIZE; j++)
			free(buffer[j]);
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "write and read");
	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static double realloc_extend(void)
{
	char *buffer;
	uint64_t i;
	double sum = 0.0;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	buffer = realloc(NULL, IO_SIZE);
	for (i = 0; i < LENGTH; i += 4096) {
		for (int j = 0; j < 4096 / IO_SIZE; j++) {
			buffer = realloc(buffer, IO_SIZE);
			write_io_size(buffer, i);
			if (read_io_size(buffer, i)) {
				return -1;
			}
		}
	}
	free(buffer);
	time_stats_stop(&ts);
	time_stats_print(&ts, "realloc extend");
	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static double realloc_shrink(void)
{
	char *buffer;
	uint64_t i;
	double sum = 0.0;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	buffer = realloc(NULL, LENGTH);
	for (i = 0; i < LENGTH; i += 4096) {
		for (int j = 0; j < 4096 / IO_SIZE; j++) {
			buffer = realloc(buffer, LENGTH - i);
			write_io_size(buffer + LENGTH - IO_SIZE - i, i);
			if (read_io_size(buffer + LENGTH - IO_SIZE - i, i)) {
				return -1;
			}
		}
	}
	free(buffer);
	time_stats_stop(&ts);
	time_stats_print(&ts, "realloc shrink");
	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static double realloc_extend_mix(void)
{
	char *buffer;
	char *wedge_buffer = NULL;
	uint64_t i;
	double sum = 0.0;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	buffer = realloc(NULL, IO_SIZE);
	for (i = 0; i < LENGTH; i += 4096) {
		for (int j = 0; j < 4096 / IO_SIZE; j++) {
			buffer = realloc(buffer, IO_SIZE);
			if (wedge_buffer)
				free(wedge_buffer);
			wedge_buffer = malloc(IO_SIZE);
			write_io_size(buffer, i);
			if (read_io_size(buffer, i)) {
				return -1;
			}
		}
	}
	free(buffer);
	time_stats_stop(&ts);
	time_stats_print(&ts, "realloc extend mix");
	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static double realloc_shrink_mix(void)
{
	char *buffer;
	char *wedge_buffer = NULL;
	uint64_t i;
	double sum = 0.0;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	buffer = realloc(NULL, LENGTH);
	for (i = 0; i < LENGTH; i += 4096) {
		for (int j = 0; j < 4096 / IO_SIZE; j++) {
			buffer = realloc(buffer, LENGTH - i);
			if (wedge_buffer)
				free(wedge_buffer);
			wedge_buffer = malloc(IO_SIZE);
			write_io_size(buffer + LENGTH - IO_SIZE - i, i);
			if (read_io_size(buffer + LENGTH - IO_SIZE - i, i)) {
				return -1;
			}
		}
	}
	free(buffer);
	time_stats_stop(&ts);
	time_stats_print(&ts, "realloc shrink mix");
	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	return sum;
}

static double gen_high_fragmentation(void)
{
	void **buffer = mmap(NULL, (LENGTH / IO_SIZE) * sizeof(void *), PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	uint64_t i;
	double sum = 0.0;
	size_t chunk_size = 4096;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += 4096) {
		malloc(chunk_size);
		buffer[i / 4096] = malloc(chunk_size);
		malloc(chunk_size);
	}
	for (i = 0; i < LENGTH; i += 4096) {
		free(buffer[i / 4096]);
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "high fragmentation");
	sum = time_stats_get_sum(&ts);
	time_stats_destroy(&ts);

	munmap(buffer, (LENGTH / IO_SIZE) * sizeof(void *));

	return sum;
}

int main(int argc, char *argv[])
{
	char buffer[1024] = "";
	FILE *file;
	int int_stat;
	double sum = 0.0;

	printf("MALLOC TEST // IO_SIZE [%dB] // TOTAL_LEN [%ldMB]\n", IO_SIZE, LENGTH / (1024 * 1024));

	sum += gen_high_fragmentation();
	sum += malloc_bytes();
	sum += write_bytes_seq();
	sum += read_bytes_seq();
	sum += free_bytes();
	sum += realloc_extend();
	sum += realloc_shrink();
	sum += realloc_extend_mix();
	sum += realloc_shrink_mix();
	sum += calloc_bytes();
	sum += write_bytes_seq();
	sum += read_bytes_seq();
	sum += free_bytes();
	sum += write_read_bytes();

	printf("Total time: %.3f msec (%.2f usec)\n", sum * 1000.0, sum * 1000000.0);

	file = fopen("/proc/self/status", "r");

	while (fscanf(file, " %1023s", buffer) == 1) {
		if (strcmp(buffer, "VmRSS:") == 0) {
			fscanf(file, " %d", &int_stat);
			printf("VmRSS: %ld\n", int_stat * 1024L);
		}
		if (strcmp(buffer, "VmHWM:") == 0) {
			fscanf(file, " %d", &int_stat);
			printf("VmPeak: %ld\n", int_stat * 1024L);
		}
	}

	fclose(file);
	munmap(0x0, 1024);

	return 0;
}
