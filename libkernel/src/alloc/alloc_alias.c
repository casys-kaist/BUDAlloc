#include "debug.h"
#include "kmalloc.h"
#include "lib/compiler.h"
#include "lib/list.h"
#include "lib/stddef.h"
#include "lib/trie.h"
#include "lib/types.h"
#include "memory.h"
#include "mimalloc.h"
#include "ota.h"
#include "plat.h"
#include "sbpf/bpf.h"
#include "spin_lock.h"
#include "rwlock.h"
#include "string.h"
#include "syscall.h"
#include "vmem.h"
#include "kthread.h"
#include "console.h"
#include <errno.h>
#include <linux/bpf.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

struct reservation_info {
	int cur_idx;
	int reuse_idx;
	int cur_len;
	void **aliases;
};
// uint64_t kmalloc_len = 0;

/* Per-thread variables. */
static __TLS struct reservation_info malloc_reservations[MAX_RESERVATION_IDX + 1];
static __TLS struct reservation_info calloc_reservations[MAX_RESERVATION_IDX + 1];
static __TLS int init_thread_done = 0;
#ifdef CONFIG_BATCHED_FREE
static __TLS size_t thread_index = -1;
static __TLS unsigned int prev_version = 0;
#endif

#ifdef CONFIG_ALIAS_CACHE
struct alias_range {
#if CONFIG_ALIAS_CACHE == 1
	struct list_head list;
#endif
	uint64_t start; // 0 for not initialized, 1 for can be reused
	uint64_t end;
};
#if CONFIG_ALIAS_CACHE == 1
static __TLS struct list_head alias_cache;
#else
#define CACHE_SIZE 8
static __TLS struct alias_range alias_cache[CACHE_SIZE];
#endif
#endif

/* Defer canonical free is critical to block metadata corruptions. */
#define DEFER_CANON_MAX 1024

static void thread_init(void)
{
#ifdef CONFIG_BATCHED_FREE
	static int thread_idx = -1;
	static int thread_idx_ = 0;
	size_t local_thread_index;
	struct mm_per_thread *mpt;
#endif
	struct reservation_info *mr = malloc_reservations;
	struct reservation_info *cr = calloc_reservations;
	size_t len;

#if CONFIG_ALIAS_CACHE == 1
	INIT_LIST_HEAD(&alias_cache);
#elif CONFIG_ALIAS_CACHE == 2
	memset(alias_cache, 0, sizeof(struct alias_range) * CACHE_SIZE);
#endif
	for (int i = 0; i <= MAX_RESERVATION_IDX; i++) {
		len = index_to_max_usable_size(i);
		// Hard coding 16 for now
		int cur_len = len <= PAGE_SIZE ? PAGE_SIZE / len : 16;
		mr[i].cur_len = cur_len;
		mr[i].cur_idx = cur_len;
		mr[i].reuse_idx = 0;
		mr[i].aliases = kcalloc(mr[i].cur_len, sizeof(void *));

		cr[i].cur_len = cur_len;
		cr[i].cur_idx = cur_len;
		cr[i].reuse_idx = 0;
		cr[i].aliases = kcalloc(cr[i].cur_len, sizeof(void *));
	}

#ifdef CONFIG_BATCHED_FREE
	spin_lock(&mm_get_active()->mm_lock);
	if (thread_idx >= MAX_PER_THREAD_NUM - 1) {
		// We have to round the thread index to 0.
#ifndef CONFIG_NDEBUG
		printk("thread number overflowed: %d\n", ++thread_idx);
#endif
		thread_index = thread_idx_;
		thread_idx_ = (thread_idx_ + 1) % MAX_PER_THREAD_NUM;
		spin_unlock(&mm_get_active()->mm_lock);
		init_thread_done = 1;
		return;
	}
	thread_index = local_thread_index = (++thread_idx) % MAX_PER_THREAD_NUM;
	mpt = &mm_get_active()->perthread[local_thread_index];
	mpt->free_alias_buf = plat_mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	mpt->buf_empty = true;
	mpt->buf_index = 0;
	mpt->buf_version = 0;
	mpt->buf_cnt = 0;
	mpt->defer_canon = kmalloc(sizeof(struct defer_canon));
#ifdef CONFIG_CANONICAL_HASH_SORT
	mpt->defer_canon->defer_canonical_buf_small = plat_mmap(
		NULL, DEFER_CANON_MAX * sizeof(void *), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
#endif
	mpt->defer_canon->defer_canonical_buf_large = plat_mmap(
		NULL, DEFER_CANON_MAX * sizeof(void *), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
#ifdef CONFIG_ADOPTIVE_BATCHED_FREE
	mpt->window_size = 1;
#else
	mpt->window_size = CONFIG_BATCHED_FREE_MAX_PAGES;
#endif
	spin_lock_init((spinlock_t *)&mpt->buf_lock);
	mm_get_active()->max_perthread_num = thread_idx + 1;
	spin_unlock(&mm_get_active()->mm_lock);
#endif
	init_thread_done = 1;
}

void ota_init(void)
{
	void *alias_base = ALIAS_BASE;

	do_mmap(&alias_base, ALIAS_SPACE_LEN, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0, 0);
	plat_mmap(alias_base, ALIAS_SPACE_LEN, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | 0x400000, -1, 0);
	plat_mmap(CANONICAL_BASE, CANON_SPACE_LEN, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | 0x400000, -1,
		  0);
	ASSERT(alias_base == ALIAS_BASE);
	ASSERT((void *)VADDR_BASE == CANONICAL_BASE);
	ASSERT(!init_thread_done);
	thread_init();
}

#ifdef CONFIG_BATCHED_FREE
static inline void try_deferred_free(struct mm_per_thread *mpt, unsigned int prev_version)
{
	if (prev_version == mpt->buf_version && !mpt->buf_empty) // Check batch free occur
		return;
#ifdef CONFIG_CANONICAL_HASH_SORT
	for (int i = 0; i < mpt->defer_canon->defer_canonical_idx_small; i++) {
		canonical_delayed_free(mpt->defer_canon->defer_canonical_buf_small[i]);
	}
	mpt->defer_canon->defer_canonical_idx_small = 0;
#endif
	for (int i = 0; i < mpt->defer_canon->defer_canonical_idx_large; i++) {
		canonical_free(mpt->defer_canon->defer_canonical_buf_large[i]);
	}
	mpt->defer_canon->defer_canonical_idx_large = 0;
}
#endif

static uint64_t get_new_huge_aliases(uint64_t n_huge)
{
	static uint64_t alias_addr_high = (uint64_t)ALIAS_BASE;
	uint64_t ret;
	ret = __atomic_fetch_add(&alias_addr_high, n_huge << (PAGE_SHIFT + 9), __ATOMIC_SEQ_CST);
	ASSERT(ret + (n_huge << (PAGE_SHIFT + 9)) <= (uint64_t)ALIAS_MAX);
	return ret;
}

#if CONFIG_ALIAS_CACHE == 1
static void *alloc_alias(uint64_t n_alias)
{
	uint64_t start, huge_end, alloc_end;
	uint64_t alloc_len, n_huges;
	struct alias_range *cur, *next;
	struct list_head *local_alias_cache = &alias_cache;
	alloc_len = n_alias << PAGE_SHIFT;

	// Traverse alias cache list
	list_for_each_entry_safe (cur, next, local_alias_cache, list) {
		if (alloc_len <= cur->end - cur->start) {
			start = cur->start;
			alloc_end = start + alloc_len;
			if (alloc_end < cur->end) {
				cur->start = alloc_end;
			} else {
				// consume all
				list_del(&cur->list);
				// __atomic_fetch_sub(&kmalloc_len, 1, __ATOMIC_RELAXED);
				kfree(cur);
			}
			return (void *)start;
		}
	}

	// allocate new huge aliases
	n_huges = (n_alias + 511) >> 9;
	start = get_new_huge_aliases(n_huges);
	huge_end = start + (n_huges << (PAGE_SHIFT + 9));

	// Try merge
	cur = list_first_entry_or_null(local_alias_cache, struct alias_range, list);
	if (cur && start == cur->end) {
		// merge
		cur->end = huge_end;

		// alloc
		start = cur->start;
		alloc_end = start + alloc_len;
		cur->start = alloc_end;
		ASSERT(alloc_end < huge_end);
		return (void *)start;
	}

	// allocate alias from huge start
	alloc_end = start + alloc_len;
	if (alloc_end < huge_end) {
		cur = kmalloc(sizeof(struct alias_range));
		INIT_LIST_HEAD(&cur->list);
		cur->start = alloc_end;
		cur->end = huge_end;
		list_add(&cur->list, local_alias_cache);
		// uint64_t b = __atomic_fetch_add(&kmalloc_len, 1, __ATOMIC_RELAXED);
		// if (b > 1000) {
		// 	printk("n nodes : %d\n", b);
		// }
	}
	// else : all huge allocated
	return (void *)start;
}
#elif CONFIG_ALIAS_CACHE == 2
#ifdef CONFIG_MIN_REMAIN_ALIASES
static void *alloc_alias(uint64_t n_alias, size_t *n_consume)
#else
static void *alloc_alias(uint64_t n_alias)
#endif
{
#ifdef CONFIG_MIN_REMAIN_ALIASES
	size_t n_remain;
#endif
	uint64_t start, huge_end, alloc_end;
	uint64_t alloc_len, n_huges;
	struct alias_range *local_alias_cache = alias_cache;

	alloc_len = n_alias << PAGE_SHIFT;

	// Traverse alias cache list
	for (size_t i = 0; i < CACHE_SIZE; i++) {
		if (local_alias_cache[i].start + alloc_len <= local_alias_cache[i].end) {
			start = local_alias_cache[i].start;
			alloc_end = start + alloc_len;
			if (alloc_end < local_alias_cache[i].end) {
#ifdef CONFIG_MIN_REMAIN_ALIASES
				n_remain = (local_alias_cache[i].end - alloc_end) >> PAGE_SHIFT;
				if (n_remain < CONFIG_MIN_REMAIN_ALIASES) {
					*n_consume = n_remain;
					local_alias_cache[i].start = 0; // index i can be reused
					local_alias_cache[i].end = 0; // index i can be reused
					return (void *)start;
				}
#endif
				local_alias_cache[i].start = alloc_end;
			} else {
				// consume all
				local_alias_cache[i].start = 0; // index i can be reused
				local_alias_cache[i].end = 0; // index i can be reused
			}
			return (void *)start;
		}
	}

	// allocate new huge aliases
	n_huges = (n_alias + 511) >> 9;
	start = get_new_huge_aliases(n_huges);
	huge_end = start + (n_huges << (PAGE_SHIFT + 9));

	// Try merge
	if (start == local_alias_cache[0].end) {
		// merge
		local_alias_cache[0].end = huge_end;
		// alloc
		start = local_alias_cache[0].start;
		alloc_end = start + alloc_len;
		local_alias_cache[0].start = alloc_end;
		ASSERT(alloc_end < huge_end);
		return (void *)start;
	}

	// allocate alias from huge start
	alloc_end = start + alloc_len;
	if (alloc_end < huge_end) {
		for (size_t i = 0; i < CACHE_SIZE; i++) {
			if (local_alias_cache[i].end == 0) {
				ASSERT(local_alias_cache[i].start == 0);
				local_alias_cache[i].start = alloc_end;
				local_alias_cache[i].end = huge_end;
				return (void *)start;
			}
		}
		// discard caches
		struct user_trie_iter iter;
		void **p_entry;
		void *trie_entry;
		for (size_t i = 0; i < CACHE_SIZE; i++) {
			if (local_alias_cache[i].end != 0) {
				ASSERT(local_alias_cache[i].start != 0);
				// printk("remove range : [0x%lx, 0x%lx]\n", local_alias_cache[i].start, local_alias_cache[i].end);
				trie_remove_range(mm_get_active()->vma_trie, local_alias_cache[i].start,
						  local_alias_cache[i].end);
			}
		}
		memset(local_alias_cache, 0, sizeof(struct alias_range) * (CACHE_SIZE));
		local_alias_cache[0].start = alloc_end;
		local_alias_cache[0].end = huge_end;
	}
	// else : all huge allocated
	return (void *)start;
}
#else
static void *alloc_alias(uint64_t n_alias)
{
	static uint64_t alias_addr_high = (uint64_t)ALIAS_BASE;
	uint64_t ret;
	ret = __atomic_fetch_add(&alias_addr_high, n_alias << PAGE_SHIFT, __ATOMIC_SEQ_CST);
	return (void *)ret;
}
#endif

static inline void *get_canonical(const void **p_trie_entry, size_t *allocsize_ptr)
{
	const void *trie_entry;
	void *canon_addr;
	int index;
	size_t size;
	if (!p_trie_entry)
		return NULL;

	trie_entry = *p_trie_entry;
	if (trie_entry == NULL) {
		return NULL;
	}
	ASSERT((uint64_t)trie_entry & PAGE_META_HEAD);
	canon_addr = (void *)TRIE_GET_CANON(trie_entry);
	index = TRIE_GET_META(trie_entry) & 63;
	size = index_to_max_usable_size(index);
	if (size == (size_t)-1) {
		size = canonical_size(canon_addr);
	}
	*allocsize_ptr = size;
	return canon_addr;
}

static inline void *__attribute__((always_inline)) do_reservations(size_t size, bool zeroing)
{
	struct reservation_info *r_info;
	void **p_trie_entry;
	void *canon_addr, *entry;
	uint64_t cur_alias_page;
	size_t alloc_size;
	uint32_t alloc_idx, n_aliases, total_n_aliases, pg_off;
	int reserve_len;
	struct user_trie_iter iter;

	alloc_size = size_to_usable_size(size);
	alloc_idx = size_to_reservation_index(size);
	ASSERT(0 <= alloc_idx && alloc_idx <= 32);
	r_info = zeroing ? &calloc_reservations[alloc_idx] : &malloc_reservations[alloc_idx];

	if (r_info->cur_idx == r_info->cur_len) {
		// ! Size is assumed to be equal to the alloc_size.
		reserve_len = r_info->cur_len;
		total_n_aliases = 0;
		for (int i = 0; i < reserve_len; i++) {
			if (i < r_info->reuse_idx) {
				entry = r_info->aliases[i]; // if batch free, it includes flush bit, buf version, tid
			} else {
				entry = zeroing ? canonical_calloc(size, &alloc_size) :
						  canonical_malloc(size, &alloc_size);
			}
			pg_off = ((uint64_t)entry) & (PAGE_SIZE - 1);
			n_aliases = (pg_off + alloc_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
			total_n_aliases += n_aliases;
			ASSERT(n_aliases <= 3 && ((uint64_t)entry & 0x7) == 0);
			r_info->aliases[i] = (void *)((uint64_t)entry | n_aliases);
		}

// TODO : make it split alloc_alias if too many alias range caches exist
#ifdef CONFIG_MIN_REMAIN_ALIASES
		size_t n_consume = 0;
		cur_alias_page = (uint64_t)alloc_alias(total_n_aliases, &n_consume);
#else
		cur_alias_page = (uint64_t)alloc_alias(total_n_aliases);
#endif

		p_trie_entry = trie_alloc_iter_init(&iter, mm_get_active()->vma_trie, cur_alias_page);
		for (int i = 0; i < reserve_len; i++) {
			entry = (void *)(((uint64_t)r_info->aliases[i] & ~0x3ULL) |
					 (uint64_t)alloc_idx << TRIE_META_SHIFT); // flush/version/tid/alloc_idx
			n_aliases = (uint64_t)r_info->aliases[i] & 0x3ULL;
			pg_off = (uint64_t)entry & (PAGE_SIZE - 1);
			// For saving malloc_usable_size and head mark
			*p_trie_entry = (void *)((uint64_t)entry | PAGE_META_HEAD);
			r_info->aliases[i] = (void *)(cur_alias_page + pg_off);
			entry = (void *)((uint64_t)entry & PAGE_MASK);

			for (int j = 1; j < n_aliases; j++) {
				cur_alias_page += PAGE_SIZE;
				entry = (void *)((uint64_t)entry + PAGE_SIZE);
				p_trie_entry = trie_alloc_iter_next(&iter);
				*p_trie_entry = entry;
			}

			if (i < reserve_len - 1) {
				cur_alias_page += PAGE_SIZE;
				p_trie_entry = trie_alloc_iter_next(&iter);
			}
		}

#ifdef CONFIG_MIN_REMAIN_ALIASES
		ASSERT(iter.indices[0] + 1 + n_consume == 512 ||
		       n_consume == 0); // it should reach huge page end or no consume
		// for (size_t i = 0; i < n_consume; i++) {
		// 	trie_iter_dec_cnt(&iter);
		// }
		// Batch above
		ATOMIC_STORE_RELEASE(&iter.nodes[2]->trie_entry[iter.indices[1]],
				     iter.nodes[2]->trie_entry[iter.indices[1]] - TRIE_META_ONE * n_consume);
		ASSERT((TRIE_GET_META(iter.nodes[2]->trie_entry[iter.indices[1]]) & 0x3ff) >
		       0); // other allocations alive
#endif

		trie_alloc_iter_finish(&iter);
		r_info->cur_idx = 0;
		r_info->reuse_idx = 0;
	}

	return r_info->aliases[r_info->cur_idx++];
}

static inline void *__ota_malloc(size_t size, bool zeroing)
{
	void **p_trie_entry;
	void *canon_addr;
	uint64_t canon_page;
	uint64_t cur_alias_page;
	size_t alloc_size;
	uint32_t alloc_idx, n_aliases, pg_off;
	struct user_trie_iter iter;
	if (size >= SIZE_MAX - PAGE_SIZE)
		return NULL;

	DEBUG_INC_VAL(total.user_to_alias, size);
	DEBUG_INC_VAL(current.user_to_alias, size);
	DEBUG_CMP_INC_VAL(max.user_to_alias, current.user_to_alias);

	if (size <= MAX_RESERVATION_SIZE) {
		return do_reservations(size, zeroing);
	}

	// LARGE : > 8192

	if (zeroing)
		canon_addr = canonical_calloc(size, &alloc_size);
	else
		canon_addr = canonical_malloc(size, &alloc_size);

	pg_off = (uint64_t)canon_addr & (PAGE_SIZE - 1);
	n_aliases = (pg_off + alloc_size + PAGE_SIZE - 1) >> PAGE_SHIFT;

#ifdef CONFIG_MIN_REMAIN_ALIASES
	size_t n_consume = 0;
	cur_alias_page = (uint64_t)alloc_alias(n_aliases, &n_consume);
#else
	cur_alias_page = (uint64_t)alloc_alias(n_aliases);
#endif

	alloc_idx = size_to_reservation_index(alloc_size);
	if (alloc_idx == -1)
		alloc_idx = TRIE_INDEX_LARGE;

	p_trie_entry = trie_alloc_iter_init(&iter, mm_get_active()->vma_trie, cur_alias_page);
	*p_trie_entry = (void *)((uint64_t)alloc_idx << TRIE_META_SHIFT | (uint64_t)canon_addr | PAGE_META_HEAD);
	canon_page = (uint64_t)canon_addr & PAGE_MASK;
	for (int i = 0; i < n_aliases - 1; i++) {
		canon_page += PAGE_SIZE;
		p_trie_entry = trie_alloc_iter_next(&iter);
		*p_trie_entry = (void *)((uint64_t)alloc_idx << TRIE_META_SHIFT | canon_page);
	}

#ifdef CONFIG_MIN_REMAIN_ALIASES
	ASSERT(iter.indices[0] + n_consume + 1 == 512 || n_consume == 0); // it should reach huge page end or no consume
	// for (size_t i = 0; i < n_consume; i++) {
	// 	trie_iter_dec_cnt(&iter);
	// }
	// Batch above
	ATOMIC_STORE_RELEASE(&iter.nodes[2]->trie_entry[iter.indices[1]],
			     iter.nodes[2]->trie_entry[iter.indices[1]] - TRIE_META_ONE * n_consume);
	ASSERT((TRIE_GET_META(iter.nodes[2]->trie_entry[iter.indices[1]]) & 0x3ff) > 0); // other allocations alive
#endif
	trie_alloc_iter_finish(&iter);
	return (void *)((uint64_t)cur_alias_page + pg_off);
}

void *ota_malloc(size_t size)
{
	void *ptr;
	rwlock_read_lock(&mm_get_active()->fork_lock);
	if (unlikely(!init_thread_done))
		thread_init();

	ptr = __ota_malloc(size, false);

	DEBUG_INC_COUNT(mallocCount);
	rwlock_read_unlock(&mm_get_active()->fork_lock);

	debug_printk("malloc: %p size %d\n", ptr, size);
	return ptr;
}

#ifdef CONFIG_ADOPTIVE_BATCHED_FREE
void inline adaptive_window_resize(struct mm_per_thread *mpt)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);

	int64_t cur = t.tv_sec * 1000 + t.tv_nsec / 1000000;
	int multiplier = (cur - mpt->msecs) / 100;
	if (multiplier) {
		mpt->msecs = cur;
		if (mpt->buf_cnt > (mpt->window_size / 4 + 1)) {
			mpt->window_size = (mpt->window_size << 2);
			mpt->window_size = mpt->window_size > CONFIG_BATCHED_FREE_MAX_PAGES ?
						   CONFIG_BATCHED_FREE_MAX_PAGES :
						   mpt->window_size;
		} else if (mpt->window_size != mpt->buf_cnt) {
			mpt->window_size = mpt->window_size >> 1;
			mpt->window_size = mpt->window_size < 1 ? 1 : mpt->window_size;
		}
	}
}
#endif

void static inline __ota_free(void *ptr)
{
	uint64_t alias_page_idx;
	size_t alloc_size;
	int index;
	uint64_t pg_off;
	int padding;
	uint64_t alias_start_page;
	void *canon_addr;
	struct mm_struct *mm = mm_get_active();
	struct contiguous_alias *cur_free_aliases;
	uint32_t n_alias;
	struct user_trie_iter iter;
	void **p_entry;
	void *trie_entry;

	if (ptr == NULL)
		return;
	p_entry = trie_iter_init(&iter, mm->vma_trie, (uint64_t)ptr);
	if (!p_entry) {
		goto RET;
	}

	trie_entry = *p_entry;
	if (trie_entry == NULL) {
		trie_iter_finish(&iter);
		PANIC("double free");
	}

	ASSERT((uint64_t)trie_entry & PAGE_META_HEAD);
	canon_addr = (void *)TRIE_GET_CANON(trie_entry);
	index = TRIE_GET_META(trie_entry) & 63;
	alloc_size = index_to_max_usable_size(index);
	if (alloc_size == (size_t)-1) {
		alloc_size = canonical_size(canon_addr);
	}

	ASSERT(alloc_size != 0);
	pg_off = (uint64_t)canon_addr & (PAGE_SIZE - 1);
	ASSERT(pg_off == ((uint64_t)ptr & (PAGE_SIZE - 1)));
	n_alias = (pg_off + alloc_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	alias_start_page = PAGE_ALIGN_DOWN((uint64_t)ptr);

	// For head
	*p_entry = NULL;
	trie_iter_dec_cnt(&iter);

	// For rest of aliases
	for (int i = 1; i < n_alias; i++) {
		p_entry = trie_iter_next(&iter);
		ASSERT(p_entry != NULL && *p_entry != NULL);
		*p_entry = NULL; // Do not need to be atomic
		trie_iter_dec_cnt(&iter);
	}

	trie_iter_finish(&iter);

#ifdef CONFIG_BATCHED_FREE
	size_t local_thread_index = thread_index;
	unsigned int *p_prev_version = &prev_version;
	unsigned int current_version; // the buffer version where the freed obj are coming in
	struct mm_per_thread *mpt = &mm->perthread[local_thread_index];
	void *flush_args[3] = { (void *)3, mm, (void *)local_thread_index }; // flush args
	spin_lock((spinlock_t *)&mpt->buf_lock);
	try_deferred_free(mpt, *p_prev_version);

	ASSERT(mpt->buf_index >= 0 && mpt->buf_index < FREE_ALIAS_BUF_SIZE);
	cur_free_aliases = &(*mpt->free_alias_buf)[mpt->buf_index];
	mpt->buf_cnt += n_alias;
#ifdef CONFIG_ADOPTIVE_BATCHED_FREE
	adaptive_window_resize(mpt);
#endif

	current_version = mpt->buf_version;

	if (cur_free_aliases->start == 0) {
		ASSERT(mpt->buf_index == 0);
		cur_free_aliases->start = alias_start_page;
		cur_free_aliases->len = PAGE_SIZE * n_alias;
		mpt->buf_empty = false;
		goto job_added_succ;
	}

	unsigned int max_match = CONFIG_BATCHED_FREE_MAX_MATCH; // TODO: further optimize
	// increses user time but may reduce kernel radix lookup

	if (mpt->buf_index < max_match - 1) {
		max_match = mpt->buf_index + 1;
	}

	while (max_match > 0) {
		if (cur_free_aliases->start + cur_free_aliases->len == alias_start_page) {
			cur_free_aliases->len += PAGE_SIZE * n_alias;
			goto job_added_succ;
		} else if (alias_start_page + PAGE_SIZE * n_alias == cur_free_aliases->start) {
			cur_free_aliases->start = alias_start_page;
			cur_free_aliases->len += PAGE_SIZE * n_alias;
			goto job_added_succ;
		}
		max_match -= 1;
		cur_free_aliases -= 1;
	}

	mpt->buf_index += 1;
	if (mpt->buf_index == FREE_ALIAS_BUF_SIZE) {
		ASSERT(!mpt->buf_empty);
		sbpf_call_function(page_ops_bpf, &flush_args[0], sizeof(void *) * 3);
		ASSERT(mpt->buf_empty == true && mpt->buf_index == 0 && mpt->buf_cnt == 0 &&
		       mpt->buf_version == (current_version + 1) % N_VERSIONS);
#ifdef CONFIG_CANONICAL_HASH_SORT
		for (int i = 0; i < mpt->defer_canon->defer_canonical_idx_small; i++) {
			canonical_delayed_free(mpt->defer_canon->defer_canonical_buf_small[i]);
		}
		mpt->defer_canon->defer_canonical_idx_small = 0;
#endif
		for (int i = 0; i < mpt->defer_canon->defer_canonical_idx_large; i++) {
			canonical_free(mpt->defer_canon->defer_canonical_buf_large[i]);
		}
		mpt->defer_canon->defer_canonical_idx_large = 0;
		current_version =
			mpt->buf_version; // mpt changed before free address is transfer to buffer. thus change current version
	}
	cur_free_aliases = &(*mpt->free_alias_buf)[mpt->buf_index];
	cur_free_aliases->start = alias_start_page;
	cur_free_aliases->len = PAGE_SIZE * n_alias;

job_added_succ:
	if (mpt->buf_cnt > mpt->window_size) {
		sbpf_call_function(page_ops_bpf, &flush_args[0], sizeof(void *) * 3);
		ASSERT(mpt->buf_empty == true && mpt->buf_index == 0 && mpt->buf_cnt == 0 &&
		       mpt->buf_version == (current_version + 1) % N_VERSIONS);
		// freeing alias become unmapped. the buffer is prev curret_version that is one less than current version
	}

	// Reusing freed canonical address for small objects
#ifdef CONFIG_CANONICAL_HASH_SORT
	if (index == 0 || (SMALL_RESERVATION_IDX < index && alloc_size <= MAX_RESERVATION_SIZE)) {
#else
	if (alloc_size <= MAX_RESERVATION_SIZE) {
#endif
		struct reservation_info *r_info = &malloc_reservations[index];
		if (r_info->reuse_idx < r_info->cur_idx) {
			ASSERT(((uint64_t)canon_addr & 0x7) == 0); // alignment 0x8
			r_info->aliases[r_info->reuse_idx++] = (void *)WITH_FLUSH(
				WITH_BUF_VER(WITH_TID(canon_addr, local_thread_index), current_version));
			goto done;
		}
	}
	/* Flush the deferred free list if it becomes full. */
	// Small object
#ifdef CONFIG_CANONICAL_HASH_SORT
	if (mpt->defer_canon->defer_canonical_idx_small == DEFER_CANON_MAX ||
	    mpt->defer_canon->defer_canonical_idx_large == DEFER_CANON_MAX) {
#else
	if (mpt->defer_canon->defer_canonical_idx_large == DEFER_CANON_MAX) {
#endif
		if (!mpt->buf_empty) {
			sbpf_call_function(page_ops_bpf, &flush_args[0], sizeof(void *) * 3);
			ASSERT(mpt->buf_empty == true && mpt->buf_index == 0 && mpt->buf_cnt == 0 &&
			       mpt->buf_version == current_version + 1);
		}
#ifdef CONFIG_CANONICAL_HASH_SORT
		for (int i = 0; i < mpt->defer_canon->defer_canonical_idx_small; i++)
			canonical_delayed_free(mpt->defer_canon->defer_canonical_buf_small[i]);
#endif
		for (int i = 0; i < mpt->defer_canon->defer_canonical_idx_large; i++)
			canonical_free(mpt->defer_canon->defer_canonical_buf_large[i]);

#ifdef CONFIG_CANONICAL_HASH_SORT
		// There should be index != 0, since 8 bytes (index == 0) cannot hold the hash entry.
		if (index <= SMALL_RESERVATION_IDX && index != 0)
			canonical_delayed_free(canon_addr);
		else
			canonical_free(canon_addr);

		mpt->defer_canon->defer_canonical_idx_small = 0;
		mpt->defer_canon->defer_canonical_idx_large = 0;
#else
		canonical_free(canon_addr);
#endif
	} else {
#ifdef CONFIG_CANONICAL_HASH_SORT
		ASSERT(mpt->defer_canon->defer_canonical_idx_small < DEFER_CANON_MAX);
		ASSERT(mpt->defer_canon->defer_canonical_idx_large < DEFER_CANON_MAX);
		// There should be index != 0, since 8 bytes (index == 0) cannot hold the hash entry.
		if (index <= SMALL_RESERVATION_IDX && index != 0) {
			mpt->defer_canon->defer_canonical_buf_small[mpt->defer_canon->defer_canonical_idx_small++] =
				canon_addr;
		} else {
			mpt->defer_canon->defer_canonical_buf_large[mpt->defer_canon->defer_canonical_idx_large++] =
				canon_addr;
		}
#else
		ASSERT(mpt->defer_canonical_idx_large < DEFER_CANON_MAX);
		mpt->defer_canon->defer_canonical_buf_large[mpt->defer_canon->defer_canonical_idx_large++] = canon_addr;
#endif
	}

done:
	*p_prev_version = mpt->buf_version;
	spin_unlock((spinlock_t *)&mpt->buf_lock);

#else /* CONFIG_BATCHED_FREE */
#ifdef CONFIG_MEMORY_DEBUG
	void *bpf_args[4] = { (void *)PTE_UNMAP, (void *)alias_start_page,
			      (void *)(alias_start_page + PAGE_SIZE * n_alias), (void *)mm_get_active() };
	sbpf_call_function(page_ops_bpf, &bpf_args[0], sizeof(void *) * 4);
#else
	void *bpf_args[3] = { (void *)PTE_UNMAP, (void *)alias_start_page,
			      (void *)(alias_start_page + PAGE_SIZE * n_alias) };
	sbpf_call_function(page_ops_bpf, &bpf_args[0], sizeof(void *) * 3);
#endif
	if (alloc_size <= MAX_RESERVATION_SIZE) {
		struct reservation_info *r_info = &malloc_reservations[index];
		if (r_info->reuse_idx < r_info->cur_idx) {
			r_info->aliases[r_info->reuse_idx++] = canon_addr;
		} else {
			canonical_free(canon_addr);
		}
	} else {
		canonical_free(canon_addr);
	}
#endif /* end of CONFIG_BATCHED_FREE */

	DEBUG_DEC_VAL(current.user_to_alias, 1); /* Temporary, should be fixed */
	debug_printk("free: alloc_size %ld alias_addr 0x%lx cacnonical addr 0x%lx page_offset %ld\n", alloc_size, ptr,
		     canon_addr, pg_off);
RET:
	return;
}

void ota_free(void *ptr)
{
	rwlock_read_lock(&mm_get_active()->fork_lock);
	if (unlikely(!init_thread_done))
		thread_init();

	__ota_free(ptr);

	DEBUG_INC_COUNT(freeCount);
	rwlock_read_unlock(&mm_get_active()->fork_lock);
}

void *ota_calloc(size_t nmemb, size_t size)
{
	void *ptr;
	rwlock_read_lock(&mm_get_active()->fork_lock);
	if (unlikely(!init_thread_done))
		thread_init();
	ptr = __ota_malloc(nmemb * size, true);

	DEBUG_INC_COUNT(callocCount);
	rwlock_read_unlock(&mm_get_active()->fork_lock);

	debug_printk("calloc: %p size %d\n", ptr, nmemb * size);
	return ptr;
}

size_t static inline __ota_malloc_usable_size(const void *ptr)
{
	int padding;
	void *alias_addr;
	void *canon_addr;
	size_t size;
	struct user_trie_iter iter;
	struct mm_struct *mm = mm_get_active();
	void **p_entry;

	p_entry = trie_iter_init(&iter, mm->vma_trie, (uint64_t)ptr);
	if (!p_entry) {
		size = 0;
		goto invalid_ptr;
	}
	canon_addr = get_canonical((const void **)p_entry, &size);
	trie_iter_finish(&iter);

	if (canon_addr == NULL) {
		size = 0;
		goto invalid_ptr;
	}

invalid_ptr:
#ifdef CONFIG_DEBUG_ALLOC
	if (ptr == NULL)
		printk("[malloc_usable_size!] Null pointer\n");
#endif

	return size;
}

size_t ota_malloc_usable_size(const void *ptr)
{
	size_t size;
	rwlock_read_lock(&mm_get_active()->fork_lock);
	if (unlikely(!init_thread_done))
		thread_init();
	size = __ota_malloc_usable_size(ptr);
	rwlock_read_unlock(&mm_get_active()->fork_lock);

	debug_printk("malloc_usable_size: %p size %zu\n", ptr, size);
	return size;
}

// static void *__ota_realloc(void *ptr, size_t size)
// {
// 	void *alias_start_page;
// 	struct insert_page_aux aux;
// 	int idx;
// 	size_t pre_usable_sz;
// 	void *canon_addr = get_canonical(ptr, &pre_usable_sz);
// 	void *new_canon_addr = canonical_realloc(canon_addr, size);

// 	ASSERT((canon_addr && pre_usable_sz != 0) || new_canon_addr);

// 	uint64_t pre_alias = (uint64_t)ptr & ~(PAGE_SIZE - 1);
// 	size_t pre_pgoff = (uint64_t)canon_addr & (PAGE_SIZE - 1);
// 	size_t pre_num_pages = (pre_usable_sz + pre_pgoff + PAGE_SIZE - 1) / PAGE_SIZE;
// 	uint64_t pre_alias_end = pre_alias + PAGE_SIZE * pre_num_pages;

// 	size_t post_usable_sz = canonical_size(new_canon_addr);
// 	size_t post_pgoff = (uint64_t)new_canon_addr & (PAGE_SIZE - 1);
// 	size_t post_num_pages = (post_usable_sz + post_pgoff + PAGE_SIZE - 1) / PAGE_SIZE;

// 	if (canon_addr == new_canon_addr && pre_num_pages >= post_num_pages) {
// 		if (pre_num_pages > post_num_pages) {
// #ifdef CONFIG_BATCHED_FREE
// 			trie_insert_range(mm_get_active()->vma_trie, (uint64_t)pre_alias + PAGE_SIZE * post_num_pages,
// 					  (uint64_t)(pre_alias + (pre_num_pages - post_num_pages) * PAGE_SIZE), false,
// 					  &__trie_remove_page, NULL);
// #else
// 			void *bpf_args[3] = { (void *)PTE_UNMAP, (void *)pre_alias + PAGE_SIZE * post_num_pages,
// 					      (void *)(pre_alias + (pre_num_pages - post_num_pages) * PAGE_SIZE) };
// 			sbpf_call_function(page_ops_bpf, &bpf_args[0], sizeof(void *) * 3);

// #endif
// 		}
// 		return ptr;
// 	}

// 	alias_start_page = alloc_alias(post_num_pages);

// 	// Free previous alias page.
// 	// Temporary disable this. Don't not why spec bench is not working.
// 	trie_insert_range(mm_get_active()->vma_trie, (uint64_t)pre_alias, (uint64_t)(pre_alias_end), false,
// 			  &__trie_remove_page, NULL);
// #ifndef CONFIG_BATCHED_FREE
// 	void *bpf_args[3] = { (void *)PTE_UNMAP, (void *)pre_alias, (void *)pre_alias_end };
// 	sbpf_call_function(page_ops_bpf, &bpf_args[0], sizeof(void *) * 3);
// #endif

// 	// Make (Update) new alias to canonical mappings.
// 	idx = size_to_reservation_index(post_usable_sz);
// 	if (idx == -1)
// 		idx = TRIE_INDEX_LARGE;
// 	aux.canonical = (((uint64_t)idx) << TRIE_META_SHIFT) | (uint64_t)new_canon_addr | PAGE_META_HEAD;
// 	aux.alloc_size = post_usable_sz;

// 	trie_insert_range(mm_get_active()->vma_trie, (uint64_t)alias_start_page,
// 			  (uint64_t)(alias_start_page + PAGE_SIZE * post_num_pages), true, &__trie_insert_page, &aux);

// 	return alias_start_page + post_pgoff;
// }

// Note that, this allocation policy could be optimized.
// To simplify the implementations, just use the copy.
void *ota_realloc(void *ptr, size_t size)
{
	void *ret;
	size_t prev_alloc_size;
	rwlock_read_lock(&mm_get_active()->fork_lock);
	if (unlikely(!init_thread_done))
		thread_init();

	// Realloc with ptr == NULL is equal to malloc(size).
	// Realloc with size == 0 is equal to free(ptr).
	if (ptr == NULL) {
		ret = __ota_malloc(size, false);
		rwlock_read_unlock(&mm_get_active()->fork_lock);
		goto DONE;
	} else if (size == 0) {
		__ota_free(ptr);
		rwlock_read_unlock(&mm_get_active()->fork_lock);
		ret = NULL;
		goto DONE;
	}

	// Legacy...
	prev_alloc_size = __ota_malloc_usable_size(ptr);

	if (prev_alloc_size >= size) {
		return ptr; // now temporarilly use this
	}
	ret = __ota_malloc(size, false);

	prev_alloc_size = prev_alloc_size < size ? prev_alloc_size : size;
	rwlock_read_unlock(&mm_get_active()->fork_lock);

	memcpy(ret, ptr, prev_alloc_size);

	rwlock_read_lock(&mm_get_active()->fork_lock);
	__ota_free(ptr);
	rwlock_read_unlock(&mm_get_active()->fork_lock);
DONE:
	DEBUG_INC_COUNT(reallocCount);
	debug_printk("realloc: %p -> %p\n", ptr, ret);
	// ret = __ota_realloc(ptr, size);
	return ret;
}

static inline void *__ota_reallocarray(void *ptr, size_t nmemb, size_t size)
{
	if (nmemb && size > SIZE_MAX / nmemb) {
		errno = ENOMEM;
		return NULL;
	}

	return ota_realloc(ptr, nmemb * size);
}

void *ota_reallocarray(void *ptr, size_t nmemb, size_t size)
{
	DEBUG_INC_COUNT(reallocarrayCount);

	return __ota_reallocarray(ptr, nmemb, size);
}

static inline void *__ota_memalign(size_t alignment, size_t size)
{
	void **p_trie_entry;
	int align_order = __builtin_ctz(alignment);
	uint64_t canon_page;
	void *canon_addr;
	uint64_t cur_alias_page;
	size_t alloc_size;
	uint32_t n_aliases, pg_off;
	int alloc_idx;
	struct user_trie_iter iter;

	if (size <= 0 || size >= SIZE_MAX - PAGE_SIZE)
		return NULL;
	else if ((1 << align_order) != alignment)
		alignment = (uint32_t)1 << (32 - __builtin_clz(alignment - 1));
	else if (align_order > PAGE_SHIFT)
		return NULL;

	if (alignment < MIN_ALLOC_SIZE) {
		canon_addr = canonical_malloc(size, &alloc_size);
	} else {
		canon_addr = canonical_memalign(size, alignment, &alloc_size);
	}

	pg_off = (uint64_t)canon_addr & (PAGE_SIZE - 1);
	n_aliases = (pg_off + alloc_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
#ifdef CONFIG_MIN_REMAIN_ALIASES
	size_t n_consume = 0;
	cur_alias_page = (uint64_t)alloc_alias(n_aliases, &n_consume);
#else
	cur_alias_page = (uint64_t)alloc_alias(n_aliases);
#endif

	alloc_idx = size_to_reservation_index(alloc_size);
	if (alloc_idx == -1)
		alloc_idx = TRIE_INDEX_LARGE;

	p_trie_entry = trie_alloc_iter_init(&iter, mm_get_active()->vma_trie, cur_alias_page);
	*p_trie_entry = (void *)((uint64_t)alloc_idx << TRIE_META_SHIFT | (uint64_t)canon_addr | PAGE_META_HEAD);
	canon_page = (uint64_t)canon_addr & PAGE_MASK;
	for (int i = 0; i < n_aliases - 1; i++) {
		canon_page += PAGE_SIZE;
		p_trie_entry = trie_alloc_iter_next(&iter);
		*p_trie_entry = (void *)((uint64_t)alloc_idx << TRIE_META_SHIFT | canon_page);
	}
#ifdef CONFIG_MIN_REMAIN_ALIASES
	ASSERT(iter.indices[0] + 1 + n_consume == 512 || n_consume == 0); // it should reach huge page end or no consume
	// for (size_t i = 0; i < n_consume; i++) {
	// 	trie_iter_dec_cnt(&iter);
	// }
	// Batch above
	ATOMIC_STORE_RELEASE(&iter.nodes[2]->trie_entry[iter.indices[1]],
			     iter.nodes[2]->trie_entry[iter.indices[1]] - TRIE_META_ONE * n_consume);
	ASSERT((TRIE_GET_META(iter.nodes[2]->trie_entry[iter.indices[1]]) & 0x3ff) > 0); // other allocations alive
#endif
	trie_alloc_iter_finish(&iter);

	debug_printk("[memalign] size %ld alloc_size %ld alignment %ld alloc_addr 0x%lx\n", size, alloc_size, alignment,
		     (uint64_t)cur_alias_page + pg_off);

	return (void *)(cur_alias_page + pg_off);
}

void *ota_memalign(size_t alignment, size_t size)
{
	void *ret;

	rwlock_read_lock(&mm_get_active()->fork_lock);
	DEBUG_INC_COUNT(memAlignCount);
	if (unlikely(!init_thread_done))
		thread_init();
	ret = __ota_memalign(alignment, size);
	rwlock_read_unlock(&mm_get_active()->fork_lock);

	return ret;
}

int ota_posix_memalign(void **memptr, size_t alignment, size_t size)
{
	void *ptr;

	rwlock_read_lock(&mm_get_active()->fork_lock);
	if (unlikely(!init_thread_done))
		thread_init();
	ptr = __ota_memalign(alignment, size);
	rwlock_read_unlock(&mm_get_active()->fork_lock);

	if (ptr != NULL)
		*memptr = ptr;
	else {
		*memptr = NULL;
		return EINVAL;
	}

	DEBUG_INC_COUNT(posixAlignCount);

	return 0;
}

void *ota_aligned_alloc(size_t alignment, size_t size)
{
	DEBUG_INC_COUNT(allocAlignCount);
	debug_printk("ota_aligned_alloc(alignment : 0x%lx, size : 0x%lx)\n", alignment, size);

	return __ota_memalign(alignment, size);
}
