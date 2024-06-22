#include "unit_test/debug.h"
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void segfault_sigaction(int signal, siginfo_t *si, void *arg)
{
	printf("Caught segfault at address %p\n", si->si_addr);
	// Catch Use-after-free, return 0
	exit(-1);
}

void test_uaf_use_reservation()
{
	void *ptr1, *ptr2;
	size_t usable_size;

	ptr1 = malloc(48);
	usable_size = malloc_usable_size(ptr1);
	
	for (int i = 0; i < (4096 / usable_size) - 1; i++)
		malloc(48); // consumes prefetched chunks
	*(int *)ptr1 = 1;

	free(ptr1);
	*(int *)ptr1 = 1;

	ptr2 = malloc(48); // same canonical as ptr2, PF since another prefetch occurs
	*(int *)ptr2 = 1;
	*(int *)ptr1 = 1; // should be detected
}

int main()
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = segfault_sigaction;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &sa, NULL);

	test_uaf_use_reservation();

	printf("This message should not be printed\n");

	// should fail
	return 0;
}
