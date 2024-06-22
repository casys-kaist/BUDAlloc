#include "unit_test/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 4096
#define MIN_ALLOCATION_SIZE 8
#define MAX_ALLOCATION_SIZE 2097152

int test_malloc_huge()
{
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

	return 0;
}

void test_free_multiple()
{
	size_t alloc_size = 128;
	int hole_idx = 10;
	char *ptrs[100];

	for (int i = 0; i < 10; i++) {
		ptrs[0] = malloc(alloc_size);
		for (int j = 1; j < PAGE_SIZE / alloc_size; j++) {
			ptrs[j] = malloc(alloc_size);
			*ptrs[j] = 'a';
		}
		*ptrs[0] = 'a';

		for (int j = 0; j < PAGE_SIZE / alloc_size; j++) {
			ASSERT(*ptrs[j] == 'a');
			free(ptrs[j]);
		}
	}
}

void test_malloc_multiple()
{
	char *ptr;
	int alloc_size = 100;
	int step = 100;

	for (int i = 0; i < 1000; i++) {
		ptr = malloc(alloc_size);
		ASSERT(ptr != NULL);
		*ptr = 'a';
		ASSERT(*ptr == 'a');
		free(ptr);
	}

	return;
}

void test_malloc()
{
	void *ptr = malloc(100);
	ASSERT(ptr != NULL);
	free(ptr);
}

int main()
{
	test_malloc();
	test_malloc_multiple();
	test_free_multiple();
	test_malloc_huge();

	return 0;
}
