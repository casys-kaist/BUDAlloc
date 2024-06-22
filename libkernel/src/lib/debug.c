#include "debug.h"
#include "console.h"
#include "lib/stdarg.h"
#include "lib/types.h"

int printk(const char *fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vprintf(fmt, ap);
	va_end(ap);

	return ret;
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
#ifdef UNIT_TEST
	plat_exit(1);
#endif
	exit(-1);
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

#ifdef CONFIG_MEMORY_DEBUG
#include "vmem.h"
extern profile_t profile;

void ota_malloc_stats(void)
{
	// Update current page fault status
	struct mm_struct *mm = mm_get_active();
	__atomic_store_n(&profile.total.canon_to_mi, mm->stats.total_alloc, __ATOMIC_RELAXED);
	__atomic_store_n(&profile.current.canon_to_mi, mm->stats.current_alloc, __ATOMIC_RELAXED);
	__atomic_store_n(&profile.max.canon_to_mi, mm->stats.max_alloc, __ATOMIC_RELAXED);

	// Print memory allocation status
	printf("malloc statistics:\n");
	printf("%-30s %d\n", "mallocCount", profile.mallocCount);
	printf("%-30s %d\n", "reallocCount", profile.reallocCount);
	printf("%-30s %d\n", "reallocarrayCount", profile.reallocarrayCount);
	printf("%-30s %d\n", "callocCount", profile.callocCount);
	printf("%-30s %d\n", "freeCount", profile.freeCount);
	printf("%-30s %d\n", "memAlignCount", profile.memAlignCount);
	printf("%-30s %d\n", "posixAlignCount", profile.posixAlignCount);
	printf("%-30s %d\n", "allocAlignCount", profile.allocAlignCount);
	printf("[user to alias]\n");
	printf("%-30s 0x%llx\n", "total", profile.total.user_to_alias);
	printf("%-30s 0x%llx\n", "current", profile.current.user_to_alias);
	printf("%-30s 0x%llx\n", "max", profile.max.user_to_alias);
	printf("[alias to canonical]\n");
	printf("%-30s 0x%llx\n", "total", profile.total.alias_to_canon);
	printf("%-30s 0x%llx\n", "current", profile.current.alias_to_canon);
	printf("%-30s 0x%llx\n", "max", profile.max.alias_to_canon);
	printf("[canonical to mimalloc]\n");
	printf("%-30s 0x%llx\n", "total", profile.total.canon_to_mi);
	printf("%-30s 0x%llx\n", "current", profile.current.canon_to_mi);
	printf("%-30s 0x%llx\n", "max", profile.max.canon_to_mi);
}
#endif
