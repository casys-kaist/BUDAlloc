#include "unit_test/debug.h"
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

void test_memalign()
{
	for (size_t i = 0; i <= 12; i++) {
		char *ptr = memalign(1 << i, 4096);
		ASSERT(ptr != NULL);
		free(ptr);
	}

	return;
}

void test_memalign_multiple()
{
	for (size_t i = 0; i <= 12; i++) {
		for (size_t alignment = 1 << i; alignment < (1U << (i + 1)); alignment += (1 << i)) {
			char *ptr = memalign(alignment, 100);
			ASSERT(ptr != NULL);
			ASSERT(100U <= malloc_usable_size(ptr));
			ASSERT(0U == (uint64_t)ptr % ((1U << i)));
			free(ptr);
		}
	}

	return;
}

void test_memalign_non_power2()
{
	void *ptr;
	for (size_t align = 0; align <= 256; align++) {
		ptr = memalign(align, 1024);
		ASSERT(ptr != NULL);
		free(ptr);
	}
}

int main()
{
	test_memalign();
	test_memalign_multiple();
	test_memalign_non_power2();
}
