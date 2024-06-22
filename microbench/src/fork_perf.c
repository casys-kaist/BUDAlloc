#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct timeval timeval1, timeval2, timeval_result;
struct timezone mytimezone = { 1, 1 };

int timeval_subtract(result, x, y) struct timeval *result, *x, *y;
{
	/* Perform the carry for the later subtraction by updating Y. */
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
       `tv_usec' is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

int main(int argc, char *argv[], char *env[])
{
	int number_of_forks;
	int size_bytes;
	int i;

	if (strcmp(argv[1], "-q") == 0) {
		exit(0);
	}

	if (argc != 3) {
		fprintf(stderr, "Usage: \%s number-of-forks xx kbytes for cow xx\n", argv[0]);
		fprintf(stderr, "Fork and exec number-of-forks process.\n");
		exit(1);
	}

	if (sscanf(argv[1], "%i", &number_of_forks) == 0) {
		fprintf(stderr, "The \"number-of-forks\" argument "
				"is not a valid integer.\n");
		exit(2);
	}

	if (sscanf(argv[2], "%i", &size_bytes) == 0) {
		fprintf(stderr, "The \"number-of-forks\" argument "
				"is not a valid integer.\n");
		exit(2);
	}

	gettimeofday(&timeval1, &mytimezone);
	char *data = malloc(size_bytes * 1024);
	memset(data, 0, size_bytes * 1024);

	for (i = 0; i < number_of_forks; i++) {
		if (fork()) {
			wait(NULL);
		} else {
			execlp(argv[0], argv[0], "-q", NULL);
		}
	}

	gettimeofday(&timeval2, &mytimezone);

	timeval_subtract(&timeval_result, &timeval2, &timeval1);

	/** printf("Forked, executed and destroyed %i processes in %lu.%lu seconds.\n", number_of_forks, */
		   /** timeval_result.tv_sec, timeval_result.tv_usec); */
	printf("%ld\n", (timeval_result.tv_sec * 1000000) + timeval_result.tv_usec);
}
