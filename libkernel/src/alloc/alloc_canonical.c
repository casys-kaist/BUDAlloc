#include "debug.h"
#include "lib/compiler.h"
#include "lib/list.h"
#include "lib/stddef.h"
#include "lib/types.h"
#include "lib/hashtable.h"
#include "mimalloc/include/mimalloc.h"
#include "mimalloc/types.h"
#include "ota.h"
#include "string.h"
#include "kthread.h"
#include "vmem.h"
#include <memory.h>

static uint64_t canon_addr_high = 0;
static uint64_t ALLOC_LEN_PER_ARENA = ((uint64_t)PAGE_SIZE * 1024 * 1024 * 64); // 128GB

static void dump_shadow_stat(int slot_size);

#define ALIGN_SIZE(SIZE) (SIZE <= 8 ? 8 : ((SIZE + FIFTEEN64) & ~FIFTEEN64))
#define GET_BIN(SIZE) (SIZE <= 8 ? 0 : SIZE <= 304 ? BIN_COUNT - (SIZE >> 4) : PAGE_SIZE / SIZE)
#define ALIGN_TO(VALUE, ALIGNMENT) ((VALUE + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

void *canonical_malloc(size_t size, size_t *allocsize_ptr)
{
	void *ret_addr;
	size_t size_expected = size_to_usable_size(size);

	ret_addr = size_expected == -1 ? mi_malloc(size) : mi_malloc(size_expected);
	*allocsize_ptr = size_expected == -1 ? mi_malloc_size(ret_addr) : size_expected;
	ASSERT(*allocsize_ptr == mi_malloc_size(ret_addr));

	DEBUG_INC_VAL(total.alias_to_canon, *allocsize_ptr);
	DEBUG_INC_VAL(current.alias_to_canon, *allocsize_ptr);
	DEBUG_CMP_INC_VAL(max.alias_to_canon, current.alias_to_canon);

	return ret_addr;
}

void *canonical_memalign(size_t size, size_t alignment, size_t *allocsize_ptr)
{
	void *ret_addr;
	ret_addr = mi_malloc_aligned(size, alignment);
	*allocsize_ptr = mi_malloc_size(ret_addr);

	DEBUG_INC_VAL(total.alias_to_canon, *allocsize_ptr);
	DEBUG_INC_VAL(current.alias_to_canon, *allocsize_ptr);
	DEBUG_CMP_INC_VAL(max.alias_to_canon, current.alias_to_canon);

	return ret_addr;
}

void *canonical_calloc(size_t size, size_t *allocsize_ptr)
{
	void *ret_addr;
	size_t size_expected = size_to_usable_size(size);

	ret_addr = size_expected == -1 ? mi_calloc(1, size) : mi_calloc(1, size_expected);
	*allocsize_ptr = size_expected == -1 ? mi_malloc_size(ret_addr) : size_expected;
	ASSERT(*allocsize_ptr == mi_malloc_size(ret_addr));

	DEBUG_INC_VAL(total.alias_to_canon, *allocsize_ptr);
	DEBUG_INC_VAL(current.alias_to_canon, *allocsize_ptr);
	DEBUG_CMP_INC_VAL(max.alias_to_canon, current.alias_to_canon);

	return ret_addr;
}

void canonical_free(void *canonical_addr)
{
	DEBUG_DEC_VAL(current.alias_to_canon, mi_malloc_size(canonical_addr));
	mi_free(canonical_addr);
}

#ifdef CONFIG_CANONICAL_HASH_SORT
static __TLS struct hlist_head free_hash[1 << (CONFIG_CANONICAL_HASH_SORT_BUCKET_SIZE)];
static __TLS bool init_hash = false;
static __TLS size_t count = 0;

struct hash_entry {
	struct hlist_node hlist;
};

void canonical_delayed_free(void *canonical_addr)
{
	struct hash_entry *entry;

	if (unlikely(!init_hash)) {
		hash_init(free_hash);
		init_hash = true;
	}

#ifdef CONFIG_MEMORY_DEBUG
	ASSERT(size_to_reservation_index(mi_malloc_size(canonical_addr)) != 0);
#endif
	entry = (struct hash_entry *)canonical_addr;
	hash_add(free_hash, &entry->hlist, (u64)canonical_addr & PAGE_MASK);
	count++;
	if (count >= CONFIG_CANONICAL_HASH_SORT_DELAYED_SIZE) {
		canonical_flush_delayed_free(false, 0, NULL);
		count = 0;
	}
}

void canonical_flush_delayed_free(bool force, unsigned long long index, void *arg)
{
	int bkt;
	struct hash_entry *cur;
	struct hlist_node *temp;

	hash_for_each_safe (free_hash, bkt, temp, cur, hlist) {
		mi_free(cur);
	}
	hash_init(free_hash);
}
#endif

// This function returns always new address aligned with PAGE_SIZE.
// Buf must contains at least PAGE_SIZE / size entries.
size_t canonical_alloc_page(size_t size, void *allocs[])
{
	size_t cnt;
	cnt = mi_malloc_page(size, allocs);
	ASSERT(cnt);

	size_t i = 0;
	size_t actual_size;

	while (allocs[i] != NULL) {
		actual_size = mi_malloc_size(allocs[i]);
		DEBUG_INC_VAL(total.alias_to_canon, actual_size);
		DEBUG_INC_VAL(current.alias_to_canon, actual_size);
		DEBUG_CMP_INC_VAL(max.alias_to_canon, current.alias_to_canon);
	}

	return cnt;
}

// This function returns always new address aligned with PAGE_SIZE.
// Buf must contains at least PAGE_SIZE / size entries.
size_t canonical_calloc_page(size_t size, void *allocs[])
{
	size_t cnt;
	cnt = mi_calloc_page(size, allocs);
	ASSERT(cnt);

	size_t i = 0;
	size_t actual_size;

	while (allocs[i] != NULL) {
		actual_size = mi_malloc_size(allocs[i]);
		DEBUG_INC_VAL(total.alias_to_canon, actual_size);
		DEBUG_INC_VAL(current.alias_to_canon, actual_size);
		DEBUG_CMP_INC_VAL(max.alias_to_canon, current.alias_to_canon);
	}

	return cnt;
}

void *canonical_realloc(void *ptr, size_t size)
{
	void *ret;

	size_t original_size;
	original_size = mi_malloc_size(ptr);
	DEBUG_DEC_VAL(current.alias_to_canon, original_size);

	ret = mi_realloc(ptr, size);

	size_t new_size;
	new_size = mi_malloc_size(ret);
	DEBUG_INC_VAL(total.alias_to_canon, new_size);
	DEBUG_INC_VAL(current.alias_to_canon, new_size);
	DEBUG_CMP_INC_VAL(max.alias_to_canon, current.alias_to_canon);

	return ret;
}

size_t canonical_size(void *canonical_addr)
{
	size_t size;
	size = mi_malloc_size(canonical_addr);
	if (size == 0) {
		PANIC("null!\n : 0x%lx", (unsigned long)canonical_addr);
		return 0;
	}
	return size;
}
