#include "kconfig.h"
#include "lib/list.h"
#include "lib/stddef.h"
#include "lib/trie.h"
#include "lib/types.h"
#include "memory.h"
#include "ota.h"
#include "rwlock.h"
#include "spin_lock.h"
#include "vmem.h"
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <stdatomic.h>
#include <stdint.h>

// #define DEBUG_PAGE_TABLE
#define ALIAS_PREFETCH_LENGTH 256

#define NULL_CHECK(PTR)                                                                                                \
	do {                                                                                                           \
		if (PTR == NULL)                                                                                       \
			return NULL;                                                                                   \
	} while (0)

#define NULL_CHECK_INVALIDATE(PTR, INV_STATEMENT)                                                                      \
	do {                                                                                                           \
		if (PTR == NULL) {                                                                                     \
			{                                                                                              \
				INV_STATEMENT                                                                          \
			}                                                                                              \
			return NULL;                                                                                   \
		}                                                                                                      \
	} while (0)

struct trie_iter {
	struct trie_node *k_nodes[5]; // for L1, L2, L3, L4(root kaddr)
#ifdef CONFIG_TRIE_CACHE
	struct trie_node *(*p_u_nodes)[5]; // with index (different from entry meta in real entry)
// addr level : 2,3,4 (0, 1 is always need to be validated with bpf_u2k since l1 lock)
// index : 0,1,2,3
#endif
	struct trie_node *u_nodes[5]; // local unodes (L1, L2, L3, L4)
	uint64_t indices[4]; // index 0, 1, 2, 3
};

#define __x86_64__ 1
#if defined(__x86_64__)
#define AT_LOAD(X) (*(X)) // In x86-64, It is already in release-aquire semantics.
#else
#error "Unsupported architecture"
#define AT_LOAD(X) __atomic_load_n((X), __ATOMIC_SEQ_CST)
#endif
// For upper level tries, bpf only loads
// For leaf, load or cmpxchg (for cmpxcchg, we force seq_cst)

#if defined(DEBUG_PAGE_TABLE) || defined(CONFIG_DEBUG_ALLOC)
#define bpf_debug(...) bpf_printk(__VA_ARGS__)
#else
#define bpf_debug(...)
#endif

#define KNODE(i) iter->k_nodes[i]
#define UNODE(i) (iter->u_nodes[i])
#define UIDX(i) (iter->indices[i])
#define UIDX_SAFE(i) (iter->indices[i] & 0x1ff)

#define CACHE(i) (*iter->p_u_nodes)[i]
#define CACHE_ADDR(i) (TRIE_GET_ADDR(CACHE(i)))
#define CACHE_IDX(i) ((uint64_t)CACHE(i) & 0x3ff) // 1ff | 200 (inv_index)
#define CACHE_PARENT_IDX(i) ((uint64_t)CACHE(i) >> TRIE_META_SHIFT)

#define ADDR_OF(V) (TRIE_GET_ADDR(V))
#define INDEX_OF(V) ((uint64_t)V & 0x3ff)
#define PARENT_INDEX_OF(V) ((uint64_t)V >> TRIE_META_SHIFT)

// #define CACHE(i) iter->u_nodes[i]
// #define T_IDX(i) (*iter->p_indices)[i]
// #define T_IDX(i) iter->u_nodes[i]

// Need to think that if fail to initialize then invalidate all index ? or just tail indcies
// Now just tail indices but may be changed

// trylock_failed becomes true only if it is already lock (not destroyed)
static inline struct trie_node *__attribute__((always_inline))
try_lock_trie_node(struct trie_node **p_node, bool *trylock_failed)
{
	struct trie_node *node_entry;

	uint16_t lock;
	node_entry = *p_node;
	if (node_entry == NULL)
		return NULL; // not initilaized entry
	lock = TRIE_GET_LOCK(node_entry);
	if (lock != TRIE_UNLOCKED_BIT) {
		if (lock == TRIE_LOCKED_BIT)
			*trylock_failed = true;
		return NULL;
	}
	if (!ATOMIC_CAS_REL_ACQ(p_node, &node_entry, (struct trie_node *)((uint64_t)node_entry | TRIE_LOCKED_BIT))) {
		*trylock_failed = true;
		return NULL;
	}
	return TRIE_GET_ADDR(node_entry);
}

// In bpf, do not need to reclaim since free always occurs in user level
static inline void __attribute__((always_inline)) unlock_trie_node(struct trie_node **p_node)
{
	struct trie_node *node_entry;
	node_entry = *p_node;
	if (TRIE_GET_LOCK(node_entry) != TRIE_LOCKED_BIT) {
		bpf_debug("error in unlock");
		// maybe possible in bpf?
		return;
	}
	*p_node = (struct trie_node *)((uint64_t)node_entry & TRIE_LOCK_MASK); // release
}

static inline bool __attribute__((always_inline))
init_iter(struct trie_iter *iter, struct trie_node *root, uint64_t vaddr)
{
	UNODE(4) = root;
	for (int i = 0; i < 4; i++) {
		UIDX(i) = (vaddr >> (12 + i * 9)) & 0x1ff;
	}
	return true;
}

// root should be initialized
static inline bool __attribute__((always_inline)) init_l4(struct trie_iter *iter)
{
	KNODE(4) = bpf_uaddr_to_kaddr(UNODE(4), PAGE_SIZE);
	NULL_CHECK_INVALIDATE(KNODE(4), bpf_debug("kernel root is not valid"););
	return true;
}

// index & l4 should be initialized
static inline bool __attribute__((always_inline)) init_l3(struct trie_iter *iter)
{
	UNODE(3) = TRIE_GET_ADDR(KNODE(4)->trie_node[UIDX_SAFE(3)]);
	NULL_CHECK_INVALIDATE(UNODE(3), bpf_debug("UNODE no entry");); // May be skipeed in optimistic situation
	KNODE(3) = bpf_uaddr_to_kaddr(UNODE(3), PAGE_SIZE);
	NULL_CHECK_INVALIDATE(KNODE(3), bpf_debug("KNODE bpf_uaddr_to_kaddr failed"););
/* Write to user-bpf shared cache : accessed in user spacce or bpf context in other threads*/
/* Do not change the cache assigning operation into the below TOCTOU check!!!!*/
#ifdef CONFIG_TRIE_CACHE
	CACHE(3) = (struct trie_node *)((uint64_t)UNODE(3) | UIDX(3));
#endif
	/* TOCTOU prevention */
	NULL_CHECK_INVALIDATE(TRIE_GET_ADDR(KNODE(4)->trie_node[UIDX_SAFE(3)]), {
#ifdef CONFIG_TRIE_CACHE
		CACHE(3) = CACHE_INVAL;
#endif
		bpf_debug("UNODE invalidted");
	});
	// Now it is safe to access KNODE(3) with guarantee that UNODE(3) is not reused by other mappings
	return true;
}

// index & l3 should be initialized
static inline bool __attribute__((always_inline)) init_l2(struct trie_iter *iter)
{
	UNODE(2) = TRIE_GET_ADDR(KNODE(3)->trie_node[UIDX_SAFE(2)]);
	NULL_CHECK_INVALIDATE(UNODE(2), bpf_debug("UNODE no entry"););
	KNODE(2) = bpf_uaddr_to_kaddr(UNODE(2), PAGE_SIZE);
	NULL_CHECK_INVALIDATE(KNODE(2), bpf_debug("KNODE bpf_uaddr_to_kaddr failed"););
	// CACHE(2) only can be used with valid L3
#ifdef CONFIG_TRIE_CACHE
	CACHE(2) = (struct trie_node *)((UIDX(3) << TRIE_META_SHIFT) | (uint64_t)UNODE(2) | UIDX(2));
#endif
	NULL_CHECK_INVALIDATE(TRIE_GET_ADDR(KNODE(3)->trie_node[UIDX_SAFE(2)]), {
#ifdef CONFIG_TRIE_CACHE
		CACHE(2) = CACHE_INVAL;
#endif
		bpf_debug("UNODE invalidted");
	});
	return true;
}

// index & l2 should be initialized
static inline bool init_l1(struct trie_iter *iter, bool *trylock_failed)
{
	UNODE(1) = try_lock_trie_node(&KNODE(2)->trie_node[UIDX_SAFE(1)], trylock_failed);
	NULL_CHECK_INVALIDATE(UNODE(1), bpf_debug("UNODE lock failed"););
#ifdef CONFIG_DEBUG_LOCK
	bpf_debug("locked 0x%lx\n", &UNODE(2)->trie_node[UIDX_SAFE(1)]);
#endif
	KNODE(1) = bpf_uaddr_to_kaddr(UNODE(1), PAGE_SIZE);
	NULL_CHECK_INVALIDATE(KNODE(1), {
		unlock_trie_node(&KNODE(2)->trie_node[UIDX_SAFE(1)]);
		bpf_debug("KNODE bpf_uaddr_to_kaddr failed");
#ifdef CONFIG_DEBUG_LOCK
		bpf_debug("unlocked 0x%lx\n", &UNODE(2)->trie_node[UIDX_SAFE(1)]);
#endif
	});
	// For L1, do not need to recheck since lock prevents unlocking from other threads
	return true;
}

// returns pointer to the entry if suceeds
static inline void **initialize_iter(struct trie_iter *iter, bool *trylock_failed)
{
	if (!init_l4(iter))
		return NULL;
	if (!init_l3(iter))
		return NULL;
	if (!init_l2(iter))
		return NULL;
	if (!init_l1(iter, trylock_failed))
		return NULL;

	return &KNODE(1)->data[UIDX_SAFE(0)];
}

static inline void finish_iter(struct trie_iter *iter)
{
	unlock_trie_node(&KNODE(2)->trie_node[UIDX_SAFE(1)]);
#ifdef CONFIG_DEBUG_LOCK
	bpf_debug("unlocked 0x%lx\n", &UNODE(2)->trie_node[UIDX_SAFE(1)]);
#endif
}

// Depends on the user (munmapped node index should be invalidated)
// returns pointer to the entry if suceeds
#ifdef CONFIG_TRIE_CACHE
static inline void **adjust_iter(struct trie_iter *iter, bool *trylock_failed)
{
	struct trie_node *cache2, *cache3;
	uint16_t indices[4];
	void *uaddr;

	cache3 = CACHE(3);
	// TODO : decouple cache2/cache3
	if (INDEX_OF(cache3) == UIDX(3)) { // If invalidated, INDEX_OF returns 0x200 that is not matched to any UIDX
		cache2 = CACHE(2);
		if (INDEX_OF(cache2) == UIDX(2) && PARENT_INDEX_OF(cache2) == UIDX(3)) {
			UNODE(2) = ADDR_OF(cache2);
			KNODE(2) = bpf_uaddr_to_kaddr(UNODE(2), PAGE_SIZE);
			NULL_CHECK_INVALIDATE(KNODE(2), bpf_debug("KNODE bpf_uaddr_to_kaddr failed"););
			// TOCTOU prevention with cache2 check
			if (cache2 == CACHE(2)) { // Safe to access TNODE(2)
				if (!init_l1(iter, trylock_failed))
					return NULL;

				return &KNODE(1)->data[UIDX_SAFE(0)];
			}
			// CACHE2 changed : rely on upper level cache
			// Even if it is invalidated, invalidated unode may not be the local cache2 but another thread's cache2
		}
		UNODE(3) = ADDR_OF(cache3);
		KNODE(3) = bpf_uaddr_to_kaddr(UNODE(3), PAGE_SIZE);
		NULL_CHECK_INVALIDATE(KNODE(3), bpf_debug("KNODE bpf_uaddr_to_kaddr failed"););
		if (cache3 == CACHE(3)) {
			if (!init_l2(iter))
				return NULL;
			if (!init_l1(iter, trylock_failed))
				return NULL;

			return &KNODE(1)->data[UIDX_SAFE(0)];
		}
	}
	return initialize_iter(iter, trylock_failed);
}
#endif

// returns pointer to the entry if suceeds
// increment iter and returns the pointer
#ifdef CONFIG_BATCHED_FREE
static inline void **next(struct trie_iter *iter, bool *l1_changed)
#else
static inline void **next(struct trie_iter *iter)
#endif
{
	// at the entry of next function, KNODE(2) ~ KNODE(1) is always valid (in multi thread)
	void *uaddr;
	bool _try_lock_failed;
	struct trie_node *cache2, *cache3;

	UIDX(0) = (UIDX(0) + 1) & 0x1ff;
	if (UIDX(0) != 0) {
		return &KNODE(1)->data[UIDX(0)];
	}
#ifdef CONFIG_BATCHED_FREE
	*l1_changed = true;
#endif
	unlock_trie_node(&KNODE(2)->trie_node[UIDX_SAFE(1)]);
#ifdef CONFIG_DEBUG_LOCK
	bpf_debug("unlocked 0x%lx\n", &UNODE(2)->trie_node[UIDX_SAFE(1)]);
#endif
	UIDX(1) = (UIDX(1) + 1) & 0x1ff;
	if (UIDX(1) != 0) {
		if (!init_l1(iter, &_try_lock_failed))
			return NULL;
		return &KNODE(1)->data[UIDX_SAFE(0)];
	}
	UIDX(2) = (UIDX(2) + 1) & 0x1ff;
	// TODO: use CACHE(2) for new UIDX(2)
	if (UIDX(2) != 0) {
#ifdef CONFIG_TRIE_CACHE
		if (!KNODE(3)) {
			cache3 = CACHE(3);
			if (INDEX_OF(cache3) == UIDX(3)) {
				UNODE(3) = ADDR_OF(cache3);
				KNODE(3) = bpf_uaddr_to_kaddr(UNODE(3), PAGE_SIZE);
				NULL_CHECK_INVALIDATE(KNODE(3), bpf_debug("KNODE bpf_uaddr_to_kaddr failed"););
				/* Prevent TOCTOU */
				if (cache3 != CACHE(3)) {
					// TODO: compare with just return NULL since prefectch can fail
					if (!init_l4(iter))
						return NULL;
					if (!init_l3(iter))
						return NULL;
				}
			} else {
				// TODO: compare with just return NULL since prefectch can fail
				if (!init_l4(iter))
					return NULL;
				if (!init_l3(iter))
					return NULL;
			}
		}
#endif
		if (!init_l2(iter))
			return NULL;
		if (!init_l1(iter, &_try_lock_failed))
			return NULL;
		return &KNODE(1)->data[UIDX_SAFE(0)];
	}
	UIDX(3) = (UIDX(3) + 1) & 0x1ff;
	if (UIDX(3) != 0) {
#ifdef CONFIG_TRIE_CACHE
		if (!KNODE(4)) {
			if (!init_l4(iter))
				return NULL;
		}
#endif
		if (!init_l3(iter))
			return NULL;
		if (!init_l2(iter))
			return NULL;
		if (!init_l1(iter, &_try_lock_failed))
			return NULL;

		return &KNODE(1)->data[UIDX_SAFE(0)];
	}
	return NULL; // overflow alias
}
struct prefetch_ctx {
	struct mm_struct *mm;
	struct trie_iter *iter;
	unsigned int flags;
	void *alias_page;
	void *canon_page;
	int64_t alias_len;
	bool need_unlock;
	size_t window_size;
#ifdef CONFIG_BATCHED_FREE
	size_t reuse_thread_idx;
	bool need_flush;
#endif
};

static long prefetch_fn(u32 _index, void *ctx);

static void *window_prev_vaddr = NULL;
static size_t window_prev_size = 1;

#ifdef CONFIG_BATCHED_FREE
static inline int flush_alias_free_buf(struct mm_struct *mm, unsigned int thread_index, unsigned int buf_version)
{
	thread_index = thread_index % MAX_PER_THREAD_NUM;
	struct mm_per_thread *mpt = (struct mm_per_thread *)(&mm->perthread[thread_index]);

	// if buf version diff, then prev mapping already unmapped!
	// if buf empty, all unmapped
	if (mpt->buf_version != buf_version || mpt->buf_empty) // do not need to flush
		return 0; // concurrent free whold be processed in next
	if (mbpf_spin_trylock((spinlock_t *)&mpt->buf_lock)) {
		if (mpt->buf_empty) { // possible by race condition. Wihout this, further branch need to be added in the below for loop
			mbpf_spin_unlock((spinlock_t *)&mpt->buf_lock);
			return 0;
		}
		typedef struct contiguous_alias(*ptr_arr)[FREE_ALIAS_BUF_SIZE];
		ptr_arr buf = (ptr_arr)bpf_uaddr_to_kaddr(mpt->free_alias_buf, PAGE_SIZE);
		unsigned int buf_index = mpt->buf_index;
		uint64_t prev_alias_start;
		uint64_t prev_len;

		if (buf == NULL) {
			bpf_debug("No buf!");
			return 1;
		}
#if defined CONFIG_DEBUG_ALLOC || defined DEBUG_PAGE_TABLE
		if (buf_index < 0 || buf_index > FREE_ALIAS_BUF_SIZE) {
			bpf_debug("invalid buf index : %d", buf_index);
			return 1;
		}
#endif
		for (unsigned int i = 0; i < FREE_ALIAS_BUF_SIZE; i++) {
			prev_alias_start = (*buf)[i].start;
			prev_len = (*buf)[i].len;
			(*buf)[i].start = (*buf)[i].len = 0;

#if defined CONFIG_DEBUG_ALLOC || defined DEBUG_PAGE_TABLE
			if (prev_alias_start == 0) {
				bpf_debug("free 0 alias");
				return 1;
			}
#endif

			bpf_debug("lazy free with prev alias 0x%lx, len : 0x%lx", prev_alias_start, prev_len);
			if (bpf_unset_page_table((void *)prev_alias_start, prev_len) != 0) {
				bpf_debug("bpf_unset_page_table(0x%lx, 0x%lx) failed with in PF component",
					   prev_alias_start, prev_len);
				mbpf_spin_unlock((spinlock_t *)&mpt->buf_lock);
				return 1;
			}
			/* Temporary, should be fixed */
			DEBUG_DEC_MM_VAL(&mm->stats.current_alloc, 1);
			if (i == buf_index)
				break;
		}
		mpt->buf_index = 0;
		mpt->buf_cnt = 0;
		mpt->buf_empty = true;
		mpt->buf_version = (mpt->buf_version + 1) & (N_VERSIONS - 1);
		mbpf_spin_unlock((spinlock_t *)&mpt->buf_lock);
	} else {
		return 2; // try_lock_failed
	}

	return 0;
}
#endif

SEC("sbpf/page_fault")
int page_fault(struct vm_fault *fault)
{
	struct mm_struct *mm;
	void *vaddr_page;
	void *canon_page;
	struct trie_iter iter;
	void **p_entry;
	bool trylock_failed;
	void *trie_entry;
	struct prefetch_ctx ctx;
	struct page_fault_stats *stats;
	int ret = 0;

	bpf_debug("#PF 0x%lx", fault->vaddr);

#ifdef CONFIG_MEMORY_DEBUG
	mm = fault->mm;
	mm = (struct mm_struct *)bpf_uaddr_to_kaddr(mm, sizeof(struct mm_struct));
	if (mm == NULL)
		return 1;

	stats = &mm->stats;
	if (stats == NULL)
		return 1;
#endif

	/* fault vaddr is canonical address. */
	if ((void *)fault->vaddr < ALIAS_BASE) {
		bpf_debug("canonical fault 0x%lx", fault->vaddr);

		bpf_set_page_table((void *)PAGE_ALIGN_DOWN((uint64_t)fault->vaddr), PAGE_SIZE,
				   (void *)PAGE_ALIGN_DOWN((uint64_t)fault->vaddr), fault->flags, PAGE_SHARED_EXEC);
		DEBUG_INC_MM_VAL(&stats->total_alloc, PAGE_SIZE);
		DEBUG_INC_MM_VAL(&stats->current_alloc, PAGE_SIZE);
		DEBUG_CMP_INC_MM_VAL(&stats->max_alloc, &stats->current_alloc);

		return 0;
	}

	/* fault vaddr is alias address. */
#ifndef CONFIG_MEMORY_DEBUG
	mm = fault->mm;
	mm = (struct mm_struct *)bpf_uaddr_to_kaddr(mm, sizeof(struct mm_struct));
	if (mm == NULL) {
		bpf_debug("mm is null");
		return 1;
	}
#endif
	if (mbpf_rwlock_read_trylock(&mm->fork_lock) != 1) {
		return 0; // fork is ongoing
	}

	/* Page mapping logic. */
	vaddr_page = (void *)((uint64_t)fault->vaddr & PAGE_MASK);
	init_iter(&iter, mm->vma_trie, (uint64_t)vaddr_page);
#ifdef CONFIG_TRIE_CACHE
	iter.p_u_nodes = &mm->trie_cached_nodes;
	for (int i = 1; i <= 4; i++) {
		iter.k_nodes[i] = NULL;
	}
	p_entry = adjust_iter(&iter, &trylock_failed);
#else
	p_entry = initialize_iter(&iter, &trylock_failed);
#endif
	if (p_entry == NULL) {
		if (trylock_failed) { // retry in next pf
			goto RET;
		} else {
			bpf_debug("Invalid memory access or UAF detected at 0x%lx", fault->vaddr);
			ret = 1;
			goto RET;
		}
	}

	trie_entry = *p_entry;
	if (trie_entry == NULL) {
		bpf_debug("Invalid memory access or UAF detected at 0x%lx with null entry", fault->vaddr);
		finish_iter(&iter);
		ret = 1;
		goto RET;
	}

	if (((uint64_t)trie_entry & PAGE_META_MAPPED) != 0) {
		bpf_debug("Already mapped 0x%lx! It is possible in multithreaded env", fault->vaddr);
		finish_iter(&iter);
		goto RET;
	}

	/* Main page fault logic. */
	// canon_page = TRIE_GET_ADDR(trie_entry);
	canon_page = (void *)((uint64_t)trie_entry & (PAGE_MASK & TRIE_META_MASK));

	size_t alloc_idx = TRIE_GET_META(trie_entry) & 0x3f;
	// The number 28 is the end of reservation index (0x1000).
	if (alloc_idx < 28) {
		ctx.window_size = ALIAS_PREFETCH_LENGTH;
	} else {
		// Update window size
		if (vaddr_page == window_prev_vaddr) {
			if (window_prev_size < CONFIG_ALIAS_WINDOW_SIZE_MAX)
				window_prev_size *= 2;
		} else {
			window_prev_size = CONFIG_ALIAS_WINDOW_SIZE_MIN;
		}
		ctx.window_size = window_prev_size;
	}

#ifdef CONFIG_BATCHED_FREE
	if (GET_FLUSH(trie_entry) == 1) {
		size_t reuse_thread_idx = GET_TID(trie_entry);
		unsigned int buf_version = GET_BUF_VER(trie_entry);
		ctx.reuse_thread_idx = reuse_thread_idx;
		int flush_ret = flush_alias_free_buf(mm, reuse_thread_idx, buf_version);
		if (flush_ret == 2) { //lock failed
			finish_iter(&iter);
			goto RET;
		}
#if defined CONFIG_DEBUG_ALLOC || defined DEBUG_PAGE_TABLE
		else if (flush_ret == 1) {
			finish_iter(&iter);
			ret = 1;
			goto RET;
		}
#endif
	} else {
		ctx.reuse_thread_idx = -1;
		int flush_cur_index = 0;
		if (mm->max_perthread_num != 1) {
			flush_cur_index = __atomic_fetch_add(&mm->cur_flush_idx, 1, __ATOMIC_RELEASE);
			if (flush_cur_index >= mm->max_perthread_num) {
				flush_cur_index = flush_cur_index - mm->max_perthread_num;
				// mm->cur_flush_idx = 1;
				mm->cur_flush_idx = (flush_cur_index + 1) & (N_THREADS - 1);
			}
		}
#if defined CONFIG_DEBUG_ALLOC || defined DEBUG_PAGE_TABLE
		if (flush_alias_free_buf(mm, flush_cur_index, N_VERSIONS) == 1) {
			finish_iter(&iter);
			ret = 1;
			goto RET;
		}
#else
		flush_alias_free_buf(mm, flush_cur_index, N_VERSIONS);
#endif
	}
#endif

	ctx.mm = mm;
	ctx.iter = &iter;
	ctx.flags = fault->flags;
	ctx.alias_page = vaddr_page;
	ctx.alias_len = PAGE_SIZE;
	ctx.canon_page = canon_page;
	ctx.need_unlock = true;
#ifdef CONFIG_BATCHED_FREE
	ctx.need_flush = false;
#endif

	*p_entry = (void *)((uint64_t)trie_entry | PAGE_META_MAPPED);
	bpf_loop(ALIAS_PREFETCH_LENGTH, prefetch_fn, &ctx, 0);
	if (ctx.need_unlock) {
		finish_iter(&iter);
	}
	if (alloc_idx > 28)
		window_prev_vaddr = ctx.alias_page + ctx.alias_len;
RET:
	mbpf_rwlock_read_unlock(&mm->fork_lock);
	return ret;
}

static long prefetch_fn(u32 _index, void *context)
{
	struct prefetch_ctx *ctx = context;
	void *cur_canon_page, *trie_entry;
	void **p_entry;
#ifdef CONFIG_BATCHED_FREE
	p_entry = next(ctx->iter, &ctx->need_flush);
#else
	p_entry = next(ctx->iter);
#endif
	if (!p_entry) {
		ctx->need_unlock = false;
		ctx->window_size = 0;
		goto map;
	}

	trie_entry = *p_entry;
	if (trie_entry == NULL || ((uint64_t)trie_entry & PAGE_META_MAPPED) != 0) {
		// Access to not exist or freed entry.
		ctx->window_size = 0;
		goto map;
	}

#ifdef CONFIG_BATCHED_FREE
	if (ctx->reuse_thread_idx != -1) {
		if (GET_FLUSH(trie_entry) == 1) {
			size_t reuse_thread_idx = GET_TID(trie_entry);
			unsigned int buf_version = GET_BUF_VER(trie_entry);

			if (reuse_thread_idx != ctx->reuse_thread_idx || ctx->need_flush) {
				if (flush_alias_free_buf(ctx->mm, reuse_thread_idx, buf_version) != 0) {
					ctx->window_size = 0;
					goto map;
				}
				ctx->reuse_thread_idx = reuse_thread_idx;
				ctx->need_flush = false;
			}
		}
	}
#endif

	cur_canon_page = (void *)((uint64_t)trie_entry & (PAGE_MASK & TRIE_META_MASK));
	if (cur_canon_page != ctx->canon_page) {
		goto map;
	} else {
		*p_entry = (void *)((uint64_t)trie_entry | PAGE_META_MAPPED);
		ctx->alias_len += PAGE_SIZE;
		if (_index == ALIAS_PREFETCH_LENGTH - 1) {
			ctx->window_size = 0;
			goto map;
		}
		return 0;
	}

map:
	bpf_debug("map page 0x%lx to 0x%lx len %d", (uint64_t)ctx->alias_page, (uint64_t)ctx->canon_page,
		  ctx->alias_len);
	bpf_set_page_table(ctx->alias_page, ctx->alias_len, ctx->canon_page, ctx->flags, PAGE_SHARED_EXEC);
	if (ctx->window_size > 1) {
		ctx->alias_page = (void *)((uint64_t)ctx->alias_page + ctx->alias_len);
		ctx->canon_page = cur_canon_page;
		ctx->alias_len = PAGE_SIZE;
		ctx->window_size--;
		return 0;
	}
	return 1;
}

char _license[] SEC("license") = "GPL";
