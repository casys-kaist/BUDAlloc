#include "unit_test/debug.h"
#include <stdio.h>
#include <stdlib.h>

void test_calloc()
{
	size_t alloc_len = 100;
	char *ptr = (char *)calloc(1, alloc_len);
	ASSERT(ptr != NULL);
	for (size_t i = 0; i < alloc_len; i++)
		ASSERT(0 == ptr[i]);

	free(ptr);

	return;
}

void test_invalid_calloc()
{
	size_t alloc_len = 100;
	char *ptr = (char *)calloc(-1, alloc_len);
	ASSERT(ptr == NULL);
	free(ptr);

	// glibc allows this.
	alloc_len = 0;
	ptr = (char *)calloc(1, alloc_len);
	ASSERT(ptr != NULL);
	free(ptr);

	return;
}

int main()
{
	test_calloc();
	// test_invalid_calloc();
}
