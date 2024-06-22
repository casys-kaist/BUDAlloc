#include "../lib/time_stat.h"
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define FILE_NAME "hugepagefile"
#define IO_SIZE (4096)
#define LENGTH (1UL * 1024 * 1024 * 1024)
#define PROTECTION (PROT_READ | PROT_WRITE)

#define ADDR (void *)(0x0UL)
#define FLAGS (MAP_ANONYMOUS | MAP_PRIVATE)
#define RAMDOM_SEED 0

struct time_stats ts;
uint64_t io_list[LENGTH / IO_SIZE];
char buffer[IO_SIZE];

static inline uint64_t get_random_val(uint64_t min, uint64_t max)
{
	uint64_t num;
	num = rand();
	num = (num << 32) | rand();
	num = ((num + min) % max);
	return num;
}

static inline void knuth_shuffle(uint64_t *io_list)
{
	srand(RAMDOM_SEED);
	for (uint64_t i = 0; i < LENGTH; i += IO_SIZE) {
		io_list[i / IO_SIZE] = i;
	}

	for (uint64_t i = 0; i < LENGTH / IO_SIZE - 1; i++) {
		uint64_t array_idx = get_random_val(i, LENGTH / IO_SIZE - 1);
		uint64_t temp = io_list[array_idx];
		io_list[array_idx] = io_list[i];
		io_list[i] = temp;
	}
}

static inline int read_io_size(char *addr, uint64_t index)
{
	// printf("0x%lx | 0x%lx | 0x%lx\n", addr, index, addr + index);
	memcpy(buffer, (void *)((uint64_t)addr + (uint64_t)index), IO_SIZE);
	if (buffer[0] != (char)index) {
		printf("Mismatch at %lu [%d]\n", index + (uint64_t)addr, buffer[index]);
		return -1;
	}

	return 0;
}

static inline void write_io_size(char *addr, uint64_t index)
{
	memset((void *)((uint64_t)addr + (uint64_t)index), (char)index, IO_SIZE);
}

static void write_bytes_seq(char *addr)
{
	uint64_t i;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		write_io_size(addr, i);
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "sequential write");
}

static int read_bytes_seq(char *addr)
{
	uint64_t i;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		if (read_io_size(addr, i)) {
			return -1;
		}
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "sequential read");
	return 0;
}

static void write_bytes_rnd(char *addr)
{
	uint64_t i;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		write_io_size(addr, io_list[i / IO_SIZE]);
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "random write");
}

static int read_bytes_rnd(char *addr)
{
	uint64_t i;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		if (read_io_size(addr, io_list[i / IO_SIZE])) {
			return -1;
		}
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "random read");
	return 0;
}

static int madvise_rnd(char *addr, int advise)
{
	uint64_t i;

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	for (i = 0; i < LENGTH; i += IO_SIZE) {
		if (madvise((void *)((uint64_t)addr + io_list[i / IO_SIZE]), IO_SIZE, advise)) {
			return -1;
		}
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "madvise dontneed");
	return 0;
}

static int seq_munmap(char *addr)
{
	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	if (munmap(addr, LENGTH)) {
		perror("munmap");
		return -1;
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "munmap");
	return 0;
}

int main(int argc, char *argv[])
{
	char *addr;
	int fd, ret;
	int flags = FLAGS;
	int do_rand = 0;
	int is_file_mapped = 0;

	for (int i = 0; i < argc; i++) {
		if (strncmp("--huge", argv[i], 6) == 0) {
			flags = FLAGS | MAP_HUGETLB;
		}
		if (strncmp("--random", argv[i], 8) == 0) {
			do_rand = 1;
			knuth_shuffle(io_list);
		}
		if (strncmp("--file_mmap", argv[i], 11) == 0) {
			is_file_mapped = 1;
		}
	}

	printf("MMAP TEST // IO_SIZE [%dB] // TOTAL_LEN [%ldMB] // HUGE_ENALBED [%s] // FILE_MAPPED [%s]\n", IO_SIZE,
	       LENGTH / (1024 * 1024), flags & MAP_HUGETLB ? "true" : "false", is_file_mapped ? "true" : "false");

	time_stats_init(&ts, 1);
	time_stats_start(&ts);
	if (is_file_mapped) {
		fd = open(FILE_NAME, O_CREAT | O_RDWR, 0755);
		if (fd < 0) {
			perror("Open failed");
			exit(1);
		}

		addr = mmap(ADDR, LENGTH, PROTECTION, flags, fd, 0);
		if (addr == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}
	} else {
		addr = (char *)mmap(NULL, LENGTH, PROTECTION, flags, -1, 0);
		if (addr == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}
	}
	time_stats_stop(&ts);
	time_stats_print(&ts, "mmap");

	write_bytes_seq(addr);
	if (!do_rand) {
		ret = read_bytes_seq(addr);
		if (ret) {
			printf("mmap failed...\n");
			return -1;
		}
	} else {
		ret = read_bytes_rnd(addr);
		if (ret) {
			printf("mmap failed...\n");
			return -1;
		}
	}

	madvise_rnd(addr, MADV_DONTNEED);

	seq_munmap(addr);
	if (is_file_mapped)
		close(fd);

	return ret;
}
