#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_BACKTRACE_LEVEL 16

/* GCC lets us add "attributes" to functions, function
 * parameters, etc. to indicate their properties.
 * See the GCC manual for details. */
#define UNUSED __attribute__((unused))
#define NO_RETURN __attribute__((noreturn))
#define NO_INLINE __attribute__((noinline))
#define PRINTF_FORMAT(FMT, FIRST) __attribute__((format(printf, FMT, FIRST)))
#define MAX_BACKTRACE_LEVEL 16

#ifndef __printf
#define __printf(fmt, args) __attribute__((format(printf, (fmt), (args))))
#endif

/* Halts the OS, printing the source file name, line number, and
 * function name, plus a user-specific message. */
#define PANIC(...) debug_panic(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define TODO(...) debug_panic(__FILE__, __LINE__, __func__, "todo!")

void debug_panic(const char *file, int line, const char *function, const char *message, ...)
    PRINTF_FORMAT(4, 5) NO_RETURN;
void debug_backtrace(void);
int printk(const char *fmt, ...) __printf(1, 2);

/* This is outside the header guard so that debug.h may be
 * included multiple times with different settings of NDEBUG. */
#undef ASSERT
#undef UNREACHABLE

#ifndef NDEBUG
#define ASSERT(CONDITION) \
    if ((CONDITION)) { \
    } else { \
        PANIC("assertion %s failed.", #CONDITION); \
    }
#define UNREACHABLE() PANIC("executed an unreachable statement");
#else
#define ASSERT(CONDITION) ((void)0)
#define UNREACHABLE() for (;;)
#endif

int printk(const char *fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vprintf(fmt, ap);
	va_end(ap);

	return ret;
}

void debug_backtrace(void)
{
	void **frame;

	printk("Call stack:");
	int level = 0;
	for (frame = __builtin_frame_address(0); frame != NULL && frame[0] != NULL && (uint64_t)frame[0] >= 4096;
	     frame = frame[0]) {
		printk(" %p", frame[1]);
		level++;
		if (level == MAX_BACKTRACE_LEVEL) {
			printk("\nBacktrace stack is now over %d\nPlease check stack pointer for overflowing",
			       MAX_BACKTRACE_LEVEL);
			break;
		}
	}
	printk(".\n");
}

void debug_panic(const char *file, int line, const char *function, const char *message, ...)
{
	static int level;
	va_list args;

	level++;
	if (level == 1) {
		printf("Kernel PANIC at %s:%d in %s(): ", file, line, function);

		va_start(args, message);
		printf(message, va_arg(args, char *));
		printf("\n");
		va_end(args);

		debug_backtrace();
	} else if (level == 2)
		printf("Kernel PANIC recursion at %s:%d in %s().\n", file, line, function);
	else {
		/* Don't print anything: that's probably why we recursed. */
	}
	exit(-1);
}
