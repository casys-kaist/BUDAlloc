#include "unit_test/debug.h"
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_realloc_multiple()
{
	char *ptr = (char *)malloc(100);
	for (int i = 0; i < 100; i++) {
		ptr = (char *)realloc(ptr, 200 * (i + 1));
		ASSERT(ptr != NULL);
		ptr[i] = 67;
	}
	for (size_t i = 0; i < 100; i++)
		ASSERT(67 == ptr[i]);
	free(ptr);
}

void test_realloc_small()
{
	char *ptr = (char *)malloc(100);
	ASSERT(ptr != NULL);
	memset(ptr, 67, 100);
	ptr = (char *)realloc(ptr, 200);
	ASSERT(ptr != NULL);
	for (size_t i = 0; i < 100; i++) {
		ASSERT(67 == ptr[i]);
	}
	free(ptr);
}

void test_realloc_large()
{
	char *ptr = (char *)malloc(8192);
	ASSERT(ptr != NULL);
	memset(ptr, 67, 200);
	ptr = (char *)realloc(ptr, 100);
	ASSERT(ptr != NULL);
	for (size_t i = 0; i < 100; i++) {
		ASSERT(67 == ptr[i]);
	}
	free(ptr);
}

void test_realloc_zero()
{
	char *ptr = (char *)realloc(NULL, 100);
	ASSERT(ptr != NULL);
	ASSERT(malloc_usable_size(ptr) >= 100);
	ptr = realloc(ptr, 0);
	ASSERT(ptr == NULL);
	ASSERT(malloc_usable_size(ptr) == 0);
}

int main()
{
	test_realloc_small();
	test_realloc_large();
	test_realloc_zero();
	test_realloc_multiple();
}
