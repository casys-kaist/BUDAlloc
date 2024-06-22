#define _GNU_SOURCE
#include "boot.h"
#include "console.h"
#include "debug.h"
#include "kmalloc.h"
#include "kthread.h"
#include "lib/stddef.h"
#include "memory.h"
#include "ota.h"
#include "plat.h"
#include "syscall.h"
#include "vmem.h"
#include <string.h>
#include <dlfcn.h>

/** #define DEBUG_MALLOC 1 */
/** #define LIBC_MALLOC 1 */

static int init_libc_done = 0;
static int init_kernel_done = 0;

static void init_mm();
static int (*main_orig)(int, char **, char **);

static int dup_stderr;

int __attribute__((visibility("default")))
__libc_start_main(int (*main)(int, char **, char **), int argc, char **argv, int (*init)(int, char **, char **),
		  void (*fini)(void), void (*rtld_fini)(void), void *stack_end)
{
	/* Save the real main function address */
	main_orig = main;

	/* Find the real __libc_start_main()... */
	typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");
	init_mm();
#ifdef CONFIG_TRACK_PREFETCH_HIT
	dup_stderr = plat_dup(2);
#endif
	/* ... and call it with our custom main function */
	return orig(main_orig, argc, argv, init, fini, rtld_fini, stack_end);
}
#ifdef CONFIG_TRACK_PREFETCH_HIT
void __attribute__((destructor)) exit_unload()
{
	struct mm_struct *mm = mm_get_active();
	plat_dup2(dup_stderr, 2);
	fprintf(stderr, "mm info : (total, on_demand, hit, n_prefetches) : 0x%llx, 0x%llx, 0x%llx, 0x%llx\n",
		mm->total_cnt, mm->on_demand_cnt, mm->hit_cnt, mm->n_prefetches);
	fprintf(stderr, "coverage : %llu/%llu, accuracy : %llu/%llu\n", mm->hit_cnt, mm->total_cnt, mm->hit_cnt,
		mm->n_prefetches);
	fprintf(stderr, "coverage : %d.%d%%, accuracy : %d.%d%%\n",
		(uint64_t)((double)mm->hit_cnt / mm->total_cnt * 100),
		(uint64_t)((double)mm->hit_cnt / mm->total_cnt * 100000) % 1000,
		(uint64_t)((double)mm->hit_cnt / mm->n_prefetches * 100),
		(uint64_t)((double)mm->hit_cnt / mm->n_prefetches * 100000) % 1000);
	plat_close(dup_stderr);
	plat_close(2);
}
#endif
static void init_mm()
{
	if (init_libc_done)
		return;

	libc_malloc = dlsym(RTLD_NEXT, "malloc");
	libc_free = dlsym(RTLD_NEXT, "free");
	libc_realloc = dlsym(RTLD_NEXT, "realloc");
	libc_reallocarray = dlsym(RTLD_NEXT, "reallocarray");
	libc_calloc = dlsym(RTLD_NEXT, "calloc");
	libc_malloc_usable_size = dlsym(RTLD_NEXT, "malloc_usable_size");
	libc_aligned_alloc = dlsym(RTLD_NEXT, "aligned_alloc");
	libc_memalign = dlsym(RTLD_NEXT, "memalign");
	libc_posix_memalign = dlsym(RTLD_NEXT, "posix_memalign");

	if (!libc_malloc || !libc_free || !libc_realloc || !libc_reallocarray || !libc_calloc ||
	    !libc_malloc_usable_size || !libc_aligned_alloc || !libc_memalign || !libc_posix_memalign)
		TODO("UNREACHABLE");

	init_libc_done = 1;
#ifndef LIBC_MALLOC
	kernel_entry();
	init_kernel_done = 1;
#endif
}

void __attribute__((visibility("default"))) * malloc(size_t size)
{
	void *addr;

	if (!init_libc_done) {
		addr = (void *)kcalloc(1, size);
		return addr;
	}

#ifdef LIBC_MALLOC
	addr = libc_malloc(size);
#else
	if (init_kernel_done) {
		addr = ota_malloc(size);
	} else
		addr = libc_malloc(size);
#endif

	return addr;
}

void __attribute__((visibility("default"))) free(void *ptr)
{
	if (!init_libc_done) {
		kfree(ptr);
		return;
	}

#ifdef LIBC_MALLOC
	libc_free(ptr);
#else
	if (init_kernel_done)
		ota_free(ptr);
	else
		libc_free(ptr);
#endif
}

void __attribute__((visibility("default"))) * realloc(void *ptr, size_t size)
{
	ASSERT(init_libc_done);
#ifdef LIBC_MALLOC
	return libc_realloc(ptr, size);
#else
	if (init_kernel_done)
		return ota_realloc(ptr, size);
	else
		return libc_realloc(ptr, size);
#endif
}

void __attribute__((visibility("default"))) * reallocarray(void *ptr, size_t nmemb, size_t size)
{
	ASSERT(init_libc_done);
#ifdef LIBC_MALLOC
	return libc_reallocarray(ptr, nmemb, size);
#else
	if (init_libc_done)
		return ota_reallocarray(ptr, nmemb, size);
	else
		return libc_reallocarray(ptr, nmemb, size);
#endif
}

void __attribute__((visibility("default"))) * calloc(size_t nmemb, size_t size)
{
	// dlysm internally uses calloc.
	// However, dlysm could accept the NULL return value of the calloc.
	// Thus, we have to use kcalloc in the calloc.
	if (!init_libc_done) {
		return kcalloc(nmemb, size);
	}

#ifdef LIBC_MALLOC
	return libc_calloc(nmemb, size);
#else
	if (init_kernel_done)
		return ota_calloc(nmemb, size);
	else
		return libc_calloc(nmemb, size);
#endif
}

void __attribute__((visibility("default"))) * memalign(size_t alignment, size_t size)
{
	if (!init_libc_done)
		return NULL;

#ifdef LIBC_MALLOC
	return libc_memalign(alignment, size);
#else
	if (init_kernel_done)
		return ota_memalign(alignment, size);
	else
		return libc_memalign(alignment, size);
#endif
}

int __attribute__((visibility("default"))) posix_memalign(void **ptr, size_t alignment, size_t size)
{
	int ret;

	if (!init_libc_done)
		return EINVAL;

#ifdef LIBC_MALLOC
	return libc_posix_memalign(ptr, alignment, size);
#else
	if (init_kernel_done) {
		ret = ota_posix_memalign(ptr, alignment, size);
		memset(*ptr, 0, size);
		return ret;
	} else
		return libc_posix_memalign(ptr, alignment, size);
#endif
}

void __attribute__((visibility("default"))) * aligned_alloc(size_t alignment, size_t size)
{
	if (!init_libc_done)
		return NULL;

#ifdef LIBC_MALLOC
	return libc_aligned_alloc(alignment, size);
#else
	if (init_kernel_done)
		return ota_aligned_alloc(alignment, size);
	else
		return libc_aligned_alloc(alignment, size);
#endif
}

size_t __attribute__((visibility("default"))) malloc_usable_size(const void *ptr)
{
	ASSERT(init_libc_done);
#ifdef LIBC_MALLOC
	return libc_malloc_usable_size(ptr);
#else
	if (init_kernel_done)
		return ota_malloc_usable_size(ptr);
	else
		return libc_malloc_usable_size(ptr);
#endif
}

void __attribute__((visibility("default"))) malloc_stats(void)
{
	ota_malloc_stats();
}

/** char __attribute__((visibility("default"))) * strdup(const char *s) */
/** { */
/** #ifdef LIBC_MALLOC */
/**     return strdup(s); */
/** #else */
/**     return ota_strdup(s); */
/** #endif */
/** } */
/**  */
/** char __attribute__((visibility("default"))) * strndup(const char *s, size_t n) */
/** { */
/** #ifdef LIBC_MALLOC */
/**     return strndup(s, n); */
/** #else */
/**     return ota_strndup(s, n); */
/** #endif */
/** } */
