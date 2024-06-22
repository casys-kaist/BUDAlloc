#include "unit_test/debug.h"
#include "kconfig.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MIN_ALLOCATION_SIZE 8
#define MAX_ALLOCATION_SIZE 2097152
#define NUM_ALLOCATING_THREADS 5
#define NUM_FORK_LOOPS 100

static volatile bool stop = false;

void test_fork_malloc()
{
	// Write a unit test of malloc with fork
	size_t alloc_size = 100;
	char *ptr = malloc(alloc_size);

	ptr[0] = 1;
	if (fork() == 0) {
		ptr[0] = 2;
		printk("child ptr %d\n", ptr[0]);
	} else {
		if (fork() == 0) {
			ptr[0] = 3;
			printk("parent 1 ptr %d\n", ptr[0]);
		}
		printk("parent 2 ptr %d\n", ptr[0]);
	}
	free(ptr);

	return;
}

void *allocate_and_free(void *arg)
{
	while (!stop) {
		for (size_t size = MIN_ALLOCATION_SIZE; size <= MAX_ALLOCATION_SIZE; size <<= 1) {
			void *ptr;
			ptr = malloc(size);
			for(int i=0;i<1000000;i++)
				asm volatile("nop");
			free(ptr);
		}
	}
	return NULL;
}

int test_alloc_after_fork()
{
	pthread_t threads[NUM_ALLOCATING_THREADS];
	int status;

	for (int i = 0; i < NUM_ALLOCATING_THREADS; i++) {
		pthread_create(&threads[i], NULL, allocate_and_free, NULL);
	}

	for (size_t i = 0; i < NUM_FORK_LOOPS; i++) {
		pid_t pid = fork();
		if (pid == 0) {
			for (size_t size = MIN_ALLOCATION_SIZE; size <= MAX_ALLOCATION_SIZE; size <<= 1) {
				void *ptr;
				ptr = malloc(size);
				if (ptr == NULL) {
					perror("malloc");
					exit(1);
				}
				memset(ptr, 0x1, size);
				free(ptr);
			}
			_exit(10);
		}
		if (pid == -1) {
			perror("fork");
			exit(1);
		}
		waitpid(pid, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 10) {
			fprintf(stderr, "Child process did not exit successfully.\n");
			exit(1);
		}
	}

	stop = true;

	for (int i = 0; i < NUM_ALLOCATING_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	return 0;
}

int main()
{
	// test_fork_malloc();
	test_alloc_after_fork();
	malloc_stats();
}
