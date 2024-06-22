#include "kconfig.h"
#include "lib/list.h"
#include "lib/stddef.h"
#include "lib/types.h"
#include "memory.h"
#include <stddef.h>

/*
Specific for mimalloc
Size: 0x0, Usable Size: 0x8		-> idx : 0
Size: 0x9, Usable Size: 0x10	-> idx : 1
Size: 0x11, Usable Size: 0x20	-> idx : 2
Size: 0x21, Usable Size: 0x30	-> idx : 3
Size: 0x31, Usable Size: 0x40	-> idx : 4
Size: 0x41, Usable Size: 0x50	-> idx : 5
Size: 0x51, Usable Size: 0x60	-> idx : 6
Size: 0x61, Usable Size: 0x70	-> idx : 7
Size: 0x71, Usable Size: 0x80	-> idx : 8
Size: 0x81, Usable Size: 0xa0	-> idx : 9
Size: 0xa1, Usable Size: 0xc0	-> idx : 10
Size: 0xc1, Usable Size: 0xe0	-> idx : 11
Size: 0xe1, Usable Size: 0x100	-> idx : 12
Size: 0x101, Usable Size: 0x140	-> idx : 13
Size: 0x141, Usable Size: 0x180	-> idx : 14
Size: 0x181, Usable Size: 0x1c0	-> idx : 15
Size: 0x1c1, Usable Size: 0x200	-> idx : 16
Size: 0x201, Usable Size: 0x280	-> idx : 17
Size: 0x281, Usable Size: 0x300	-> idx : 18
Size: 0x301, Usable Size: 0x380	-> idx : 19
Size: 0x381, Usable Size: 0x400	-> idx : 20
Size: 0x401, Usable Size: 0x500	-> idx : 21
Size: 0x501, Usable Size: 0x600	-> idx : 22
Size: 0x601, Usable Size: 0x700	-> idx : 23
Size: 0x701, Usable Size: 0x800	-> idx : 24
Size: 0x801, Usable Size: 0xa00	-> idx : 25
Size: 0xa01, Usable Size: 0xc00	-> idx : 26
Size: 0xc01, Usable Size: 0xe00	-> idx : 27
Size: 0xe01, Usable Size: 0x1000	-> idx : 28 - end of reservation index

Size: 0x1001, Usable Size: 0x1400 -> idx : 29
Size: 0x1401, Usable Size: 0x1800 -> idx : 30
Size: 0x1801, Usable Size: 0x1c00 -> idx : 31
Size: 0x1c01, Usable Size: 0x2000 -> idx : 32
Size: 0x2001, Usable Size: 0x2800 -> idx : 33
Size: 0x2801, Usable Size: 0x3000 -> idx : 34
Size: 0x3001, Usable Size: 0x3800 -> idx : 35
Size: 0x3801, Usable Size: 0x4000 -> idx : 36
Size: 0x4001, Usable Size: 0x5000 37
Size: 0x5001, Usable Size: 0x6000 38
Size: 0x6001, Usable Size: 0x7000 39
Size: 0x7001, Usable Size: 0x8000 40
Size: 0x8001, Usable Size: 0xa000 41
Size: 0xa001, Usable Size: 0xc000 42
Size: 0xc001, Usable Size: 0xe000 43
Size: 0xe001, Usable Size: 0x10000 44
*/

#ifdef CONFIG_ADOPTIVE_BATCHED_FREE
#ifndef CONFIG_BATCHED_FREE
#error "CONFIG_BATCHED_FREE must be enabled to use CONFIG_ADOPTIVE_BATCHED_FREE"
#endif
#endif

static inline size_t size_to_usable_size(size_t size)
{
	if (size <= 0x8)
		return 0x8;
	if (size <= 0x80)
		return (size + 0xf) & ~0xf;
	if (size <= 0x100)
		return (size + 0x1f) & ~0x1f;
	if (size <= 0x200)
		return (size + 0x3f) & ~0x3f;
	if (size <= 0x400)
		return (size + 0x7f) & ~0x7f;
	if (size <= 0x800)
		return (size + 0xff) & ~0xff;
	if (size <= 0x1000)
		return (size + 0x1ff) & ~0x1ff;
	// after reservation
	if (size <= 0x2000)
		return (size + 0x3ff) & ~0x3ff;
	if (size <= 0x4000)
		return (size + 0x7ff) & ~0x7ff;
	if (size <= 0x8000)
		return (size + 0xfff) & ~0xfff;
	if (size <= 0x10000)
		return (size + 0x1fff) & ~0x1fff;
	return (size_t)-1;
}

static inline int size_to_reservation_index(size_t size)
{
	if (size <= 0x8)
		return 0;
	if (size <= 0x80)
		return (size + 0xf) >> 4;
	if (size <= 0x100)
		return 4 + ((size + 0x1f) >> 5);
	if (size <= 0x200)
		return 8 + ((size + 0x3f) >> 6);
	if (size <= 0x400)
		return 12 + ((size + 0x7f) >> 7);
	if (size <= 0x800)
		return 16 + ((size + 0xff) >> 8);
	if (size <= 0x1000)
		return 20 + ((size + 0x1ff) >> 9);
	// after reservation
	if (size <= 0x2000)
		return 24 + ((size + 0x3ff) >> 10);
	if (size <= 0x4000)
		return 28 + ((size + 0x7ff) >> 11);
	if (size <= 0x8000)
		return 32 + ((size + 0xfff) >> 12);
	if (size <= 0x10000)
		return 36 + ((size + 0x3ff) >> 13);
	return -1;
}

static inline size_t index_to_max_usable_size(int index)
{
	if (index == 0)
		return 0x8;
	if (index <= 8)
		return index << 4;
	if (index <= 12)
		return (index - 4) << 5;
	if (index <= 16)
		return (index - 8) << 6;
	if (index <= 20)
		return (index - 12) << 7;
	if (index <= 24)
		return (index - 16) << 8;
	if (index <= 28)
		return (index - 20) << 9;
	// after small reservation
	if (index <= 32)
		return (index - 24) << 10;
	if (index <= 36)
		return (index - 28) << 11;
	if (index <= 40)
		return (index - 32) << 12;
	if (index <= 44)
		return (index - 36) << 13;
	return (size_t)-1;
}

#define MIN_ALLOC_SIZE 0x8
#define MAX_RESERVATION_SIZE 0x1400
#define SMALL_RESERVATION_IDX 14
#define MAX_RESERVATION_IDX 29 

#define PAGE_META_MASK (~(uint64_t)0x7)
#define PAGE_META_ALL 0x7
#define PAGE_META_HEAD 0x4
// #define PAGE_META_LARGE 0x2
#ifdef CONFIG_TRACK_PREFETCH_HIT
#ifdef PAGE_META_LARGE
#undef PAGE_META_LARGE
#endif
#define PAGE_META_PREFETCHED 0x2
#endif
#define PAGE_META_MAPPED 0x1

// Default options for the ota allocator.
// ALIAS_BASE should be larger than CANONICAL_BASE.
#define CANONICAL_BASE 	(void *)0x000100000000
#define CANONICAL_MAX 	(void *)0x100000000000
#define CANON_SPACE_LEN ((uint64_t)CANONICAL_MAX - (uint64_t)CANONICAL_BASE)
#define ALIAS_BASE 		(void *)0x100100000000
#define ALIAS_MAX 		(void *)0x200000000000
#define ALIAS_SPACE_LEN ((uint64_t)ALIAS_MAX - (uint64_t)ALIAS_BASE)

// Inside the ota.c
void ota_init(void);

// Declare standard malloc API functions
// Inside the alloc_alias.c
#define PREALLOC(func) ota_##func
void *PREALLOC(malloc)(size_t size);
void *PREALLOC(realloc)(void *ptr, size_t size);
void *PREALLOC(reallocarray)(void *ptr, size_t nmemb, size_t size);
void *PREALLOC(calloc)(size_t nmemb, size_t size);
void PREALLOC(free)(void *ptr);
void *PREALLOC(memalign)(size_t alignment, size_t size);
int PREALLOC(posix_memalign)(void **ptr, size_t alignment, size_t size);
void *PREALLOC(aligned_alloc)(size_t alignment, size_t size);
size_t PREALLOC(malloc_usable_size)(const void *ptr);

// Inside the alloc_canonical.c
void *canonical_malloc(size_t, size_t *);
void *canonical_memalign(size_t size, size_t alignment, size_t *allocsize_ptr);
void *canonical_calloc(size_t number, size_t *alloc_size_ptr);
void canonical_free(void *canonical_addr);
void canonical_delayed_free(void *canonical_addr);
void canonical_flush_delayed_free(bool force, unsigned long long heartbeat, void* arg);
void *canonical_realloc(void *, size_t);
size_t canonical_size(void *canonical_addr);
size_t canonical_alloc_page(size_t size, void *allocs[]);
size_t canonical_calloc_page(size_t size, void *allocs[]);

// libc malloc API functions
// Inside the alloc_entry.c
void *(*libc_malloc)(size_t);
void (*libc_free)(void *);
void *(*libc_realloc)(void *, size_t);
void *(*libc_reallocarray)(void *, size_t, size_t);
void *(*libc_calloc)(size_t, size_t);
void *(*libc_aligned_alloc)(size_t, size_t);
void *(*libc_memalign)(size_t, size_t);
int (*libc_posix_memalign)(void **, size_t, size_t);
size_t (*libc_malloc_usable_size)(const void *ptr);
