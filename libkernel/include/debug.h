#ifndef __LIB_DEBUG_H
#define __LIB_DEBUG_H

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

#ifdef CONFIG_DEBUG_ALLOC
#define debug_printk(fmt, args...) printk(fmt, ##args)
#else
#define debug_printk(fmt, args...) ((void)0)
#endif

/* Halts the OS, printing the source file name, line number, and
 * function name, plus a user-specific message. */
#define PANIC(...) debug_panic(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define TODO(...) debug_panic(__FILE__, __LINE__, __func__, "todo!")

void debug_panic(const char *file, int line, const char *function, const char *message, ...)
	PRINTF_FORMAT(4, 5) NO_RETURN;
void debug_backtrace(void);

int printk(const char *fmt, ...) __printf(1, 2);

#ifdef CONFIG_MEMORY_DEBUG
typedef struct profiling_struct {
	int mallocCount; /* Count of malloc calls, including indirect ones like realloc and calloc */
	int reallocCount; /* Count of realloc calls */
	int reallocarrayCount; /* Count of reallocarray calls */
	int callocCount; /* Count of calloc calls */
	int freeCount; /* Count of free calls, including indirect ones through realloc */
	int memAlignCount; /* Count of posix_memalign calls */
	int posixAlignCount; /* Count of posix_memalign calls */
	int allocAlignCount; /* Count of align_alloc calls */
	struct {
		long long user_to_alias;
		long long alias_to_canon;
		long long canon_to_mi;
	} total;
	struct {
		long long user_to_alias;
		long long alias_to_canon;
		long long canon_to_mi;
	} current;
	struct {
		long long user_to_alias;
		long long alias_to_canon;
		long long canon_to_mi;
	} max;
} profile_t;

/* define global var */
profile_t profile;

/* define helper function */
void ota_malloc_stats(void);

/* define memory accouting macros */
#define DEBUG_DEC_COUNT(field_name) __atomic_fetch_sub(&profile.field_name, 1, __ATOMIC_RELAXED)
#define DEBUG_INC_COUNT(field_name) __atomic_fetch_add(&profile.field_name, 1, __ATOMIC_RELAXED)
#define DEBUG_DEC_VAL(field_name, value) __atomic_fetch_sub(&profile.field_name, value, __ATOMIC_RELAXED)
#define DEBUG_INC_VAL(field_name, value) __atomic_fetch_add(&profile.field_name, value, __ATOMIC_RELAXED)
#define DEBUG_CMP_INC_VAL(tar_field_name, src_field_name)                                                              \
	do {                                                                                                           \
		bool success = false;                                                                                  \
		do {                                                                                                   \
			long long current_value = __atomic_load_n(&profile.tar_field_name, __ATOMIC_RELAXED);          \
			long long new_value = __atomic_load_n(&profile.src_field_name, __ATOMIC_RELAXED);              \
			if ((new_value) > (current_value)) {                                                           \
				success = __atomic_compare_exchange_n(&profile.tar_field_name, &(current_value),       \
								      (new_value), false, __ATOMIC_RELAXED,            \
								      __ATOMIC_RELAXED);                               \
			} else {                                                                                       \
				success = true;                                                                        \
			}                                                                                              \
		} while (!success);                                                                                    \
	} while (0)

#else
#define DEBUG_INC_COUNT(field_name)
#define DEBUG_DEC_COUNT(field_name)
#define DEBUG_INC_VAL(field_name, value)
#define DEBUG_DEC_VAL(field_name, value)
#define DEBUG_CMP_INC_VAL(tar_field_name, src_field_name)
static void ota_malloc_stats(void)
{
	printk("Memory Debug is not enabled\n");
};
#endif // end of #ifdef CONFIG_MEMORY_DEBUG
#endif // end of #ifndef __LIB_DEBUG_H

/* This is outside the header guard so that debug.h may be
 * included multiple times with different settings of NDEBUG. */
#undef ASSERT
#undef UNREACHABLE

#ifndef CONFIG_NDEBUG
#define ASSERT(CONDITION)                                                                                              \
	if ((CONDITION)) {                                                                                             \
	} else {                                                                                                       \
		PANIC("assertion %s failed.", #CONDITION);                                                             \
	}
#define UNREACHABLE() PANIC("executed an unreachable statement");
#else
#define ASSERT(CONDITION) ((void)0);
#define UNREACHABLE() for (;;)
#endif /* lib/debug.h */
