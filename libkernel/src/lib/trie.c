#include "lib/trie.h"
#include "debug.h"
#include "errno.h"
#include "kmalloc.h"
#include "lib/stddef.h"
#include "lib/string.h"
#include "lib/types.h"
#include "memory.h"
#include "ota.h"
#include "plat.h"
#include "vmem.h"
#include "kthread.h"
#include <stdatomic.h>
#include <stdint.h>
#include <pthread.h>

#define STORE(X, V) ATOMIC_STORE_RELEASE(X, V)
#define LOAD(X) ATOMIC_LOAD_ACQUIRE(X)

#define TRIE_BLOCK_SIZE_INIT (1 * PAGE_SIZE)
#define TRIE_BLOCK_N_THRESHOLD 128
#define TRIE_BLOCK_SIZE_THRESHOLD (128 * PAGE_SIZE)
__TLS uint64_t trie_block_start;
__TLS uint64_t trie_block_end;
__TLS uint64_t trie_block_size = TRIE_BLOCK_SIZE_INIT;

#define RECLAIM_LIST_LEN 8
struct reclaim_range {
	uint64_t start; // 0 for not initialized, 1 for can be reused
	uint64_t end;
};
;
static __TLS struct reclaim_range reclaim_list[RECLAIM_LIST_LEN];
static __TLS uint64_t n_reclaims = 0;
// uint64_t trie_usage = 0;

#ifdef CONFIG_TRIE_CACHE
static inline void __attribute__((always_inline)) invalidate_cached_node(uint64_t level, struct trie_node *inv_addr)
{
	struct trie_node **p_cache = &mm_get_active()->trie_cached_nodes[level];
	struct trie_node *cache = LOAD(p_cache);

	ASSERT(level == 2 || level == 3);

	if (TRIE_GET_ADDR(cache) == inv_addr) {
		ATOMIC_CAS_REL_ACQ(p_cache, &cache, CACHE_INVAL);
		// cas success : invalidated
		// cas fail : cache does not point to inv_addr ok!
	}
}
#endif

static void reclaim(void *ptr)
{
	struct reclaim_range *local_list = reclaim_list;
	uint64_t *p_n_saved = &n_reclaims;
	uint64_t n_saved = *p_n_saved;
	uint64_t u_ptr = (uint64_t)ptr;

	ASSERT(((uint64_t)ptr & (PAGE_SIZE - 1)) == 0);
	ASSERT(n_saved < TRIE_BLOCK_N_THRESHOLD);

	n_saved += 1;

	// Traverse reclaim list
	for (size_t i = 0; i < RECLAIM_LIST_LEN; i++) {
		if (local_list[i].end == u_ptr) {
			local_list[i].end += PAGE_SIZE;
			goto RET;
		}
		if (local_list[i].start == u_ptr + PAGE_SIZE) {
			local_list[i].start = u_ptr;
			goto RET;
		}
	}

	// Fill an empty space if possible
	for (size_t i = 0; i < RECLAIM_LIST_LEN; i++) {
		if (local_list[i].end == 0) {
			ASSERT(local_list[i].start == 0);
			local_list[i].start = u_ptr;
			local_list[i].end = u_ptr + PAGE_SIZE;
			goto RET;
		}
	}

	// Evict index 0 range
	plat_munmap((void *)local_list[0].start, (local_list[0].end - local_list[0].start));
	n_saved -= (local_list[0].end - local_list[0].start) >> PAGE_SHIFT;
	local_list[0].start = u_ptr;
	local_list[0].end = u_ptr + PAGE_SIZE;

RET:
	if (n_saved == TRIE_BLOCK_N_THRESHOLD) {
		for (size_t i = 0; i < RECLAIM_LIST_LEN; i++) {
			if (local_list[i].end != 0) {
				ASSERT(local_list[i].start != 0);
				plat_munmap((void *)local_list[i].start, (local_list[i].end - local_list[i].start));
				local_list[i].start = 0;
				local_list[i].end = 0;
			}
		}
		n_saved = 0;
	}
	*p_n_saved = n_saved;
}

static inline void *__attribute__((always_inline)) get_new_page()
{
	uint64_t *p_start = &trie_block_start, *p_end = &trie_block_end, *p_size = &trie_block_size;
	uint64_t start = *p_start, end = *p_end, size = *p_size;

	if (start == end) {
		start = (uint64_t)plat_mmap(NULL, trie_block_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE,
					    -1, 0);
		// uint64_t a = __atomic_fetch_add(&trie_usage, size, __ATOMIC_RELAXED);
		// if ((a % 1024000) == 0) {
		// 	printk("trie_usage : %lldKB\n", a / 1024);
		// }
		if (start == 0)
			PANIC("failed to allocate memory for trie");
		end = start + size;
		*p_end = end;
		if (size < TRIE_BLOCK_SIZE_THRESHOLD)
			*p_size = size * 2;
	}
	*p_start = start + PAGE_SIZE;

	ASSERT(end > start && (end - start) % PAGE_SIZE == 0);

	return (void *)start;
}

static inline void __attribute__((always_inline)) rollback_page()
{
	trie_block_start -= PAGE_SIZE;
	ASSERT(trie_block_end - trie_block_size <= trie_block_start);
}

static inline struct trie_node *__attribute__((always_inline))
lock_trie_node_with_entry(struct trie_node **p_node, struct trie_node *node_entry)
{
	struct trie_node *node_ptr;
	uint64_t lock;

	// TODO : fast spinlock maybe possible?
	ASSERT(node_entry != NULL);

	while (1) { // spins
		lock = TRIE_GET_LOCK(node_entry);
		if (lock == TRIE_UNLOCKED_BIT) {
			if (ATOMIC_CAS_REL_ACQ(p_node, &node_entry,
					       (struct trie_node *)((uint64_t)node_entry | TRIE_LOCKED_BIT))) {
				break;
			} else {
				continue; // CAS sets node_entry
			}
		}
		// PANIC("not in this state in single thread");
		if (lock == TRIE_DESTORYED_BIT) { // node destroyed
			return NULL;
		}
		// lock == 1 -> retry
		PAUSE(); // backoff
		node_entry = ATOMIC_LOAD_RELAX(p_node);
	}

#ifdef CONFIG_DEBUG_LOCK
	printk("[%lx] locked 0x%lx : 0x%lx\n", pthread_self(), p_node, node_entry);
#endif
	node_ptr = TRIE_GET_ADDR(node_entry);

	return node_ptr;
}

static inline struct trie_node *__attribute__((always_inline)) lock_trie_node(struct trie_node **p_node)
{
	struct trie_node *node_entry, *node_ptr;
	uint64_t lock;

	// TODO : fast spinlock maybe possible?
	node_entry = ATOMIC_LOAD_RELAX(p_node);

	if (node_entry == NULL)
		return NULL; // not initilaized entry
	while (1) { // spins
		lock = TRIE_GET_LOCK(node_entry);
		if (lock == TRIE_UNLOCKED_BIT) {
			if (ATOMIC_CAS_REL_ACQ(p_node, &node_entry,
					       (struct trie_node *)((uint64_t)node_entry | TRIE_LOCKED_BIT))) {
				break;
			} else {
				continue; // CAS sets node_entry
			}
		}
		// PANIC("not in this state in single thread");
		if (lock == TRIE_DESTORYED_BIT) { // node destroyed
			return NULL;
		}
		// lock == 1 -> retry
		PAUSE(); // backoff
		node_entry = ATOMIC_LOAD_RELAX(p_node);
	}

#ifdef CONFIG_DEBUG_LOCK
	printk("[%lx] locked 0x%lx : 0x%lx\n", pthread_self(), p_node, node_entry);
#endif
	node_ptr = TRIE_GET_ADDR(node_entry);

	return node_ptr;
}

// ret : is node destroyed with unlock
static inline bool __attribute__((always_inline)) unlock_trie_node(struct trie_node **p_node, int index)
{
	// level : 1
	struct trie_node *node_entry, *node_ptr;
	uint16_t count;

	node_entry = ATOMIC_LOAD_RELAX(p_node); // Relax Since the thread holds the lock
#ifdef CONFIG_DEBUG_LOCK
	printk("[%lx] unlock 0x%lx : 0x%lx\n", pthread_self(), p_node, node_entry);
	if (TRIE_GET_LOCK(node_entry) != 1) {
		printk("[%lx] not locked 0x%lx : 0x%lx\n", pthread_self(), p_node, node_entry);
	}
#endif
	count = TRIE_GET_META(node_entry) & 0x3ff;
	node_ptr = TRIE_GET_ADDR(node_entry);

	ASSERT(TRIE_LOCKED(node_entry));
	ASSERT(node_ptr != NULL);

	if (count == 0) {
		// For lock, order of operations for invalidation does not matter.
		ATOMIC_STORE_RELEASE(p_node, (struct trie_node *)TRIE_DESTORYED_BIT); // Unlink
		// Order of inv cached index and munmap does not matter since in bpf context, seperate ref cnt mechanism operate.
		reclaim(node_ptr);
		// __atomic_fetch_sub(&trie_usage, PAGE_SIZE, __ATOMIC_RELAXED);
		return true;
	} else {
		ATOMIC_STORE_RELEASE(p_node,
				     (struct trie_node *)((uint64_t)node_entry & TRIE_LOCK_MASK)); // make it unlocked
		return false;
	}
}

// Initialize trie node
static inline struct trie_node *__attribute__((always_inline)) trie_init_with_lock(struct trie_node **p_node_entry)
{
	struct trie_node *prev_entry = NULL;
	struct trie_node *new_page = get_new_page();
	uint64_t initial_meta = TRIE_INITIAL_CNT | TRIE_LOCKED_BIT; // Initial couint: 512

	if (!ATOMIC_CAS_REL_ACQ(p_node_entry, &prev_entry, (struct trie_node *)((uint64_t)new_page | initial_meta))) {
		// already initialized
		rollback_page();
		// need to lock
		return lock_trie_node_with_entry(p_node_entry, prev_entry);
	} else {
		return new_page;
	}
}

// Initialize trie node
static inline struct trie_node *__attribute__((always_inline)) trie_init(struct trie_node **p_node_entry)
{
	struct trie_node *prev_entry = NULL;
	struct trie_node *new_page = get_new_page();
	uint64_t initial_meta = TRIE_INITIAL_CNT; // Initial count: 512

	if (!ATOMIC_CAS_REL_ACQ(p_node_entry, &prev_entry, (struct trie_node *)((uint64_t)new_page | initial_meta))) {
		// already initialized
		rollback_page();
		return TRIE_GET_ADDR(prev_entry);
	} else {
		return new_page;
	}
}

static inline struct trie_node *__attribute__((always_inline)) touch_lock_trie_node(struct trie_node **p_node)
{
	struct trie_node *node_entry, *node_ptr;
	uint16_t count;

	node_entry = ATOMIC_LOAD_RELAX(p_node);
	ASSERT(!TRIE_DESTROYED(node_entry)); // not to be destroyed
	node_ptr = TRIE_GET_ADDR(node_entry);
	if (!node_ptr) {
		node_ptr = trie_init_with_lock(p_node);
	} else {
		node_ptr = lock_trie_node_with_entry(p_node, node_entry); // just lock wihout additional read
	}
	ASSERT(node_ptr != NULL);

	return node_ptr;
}

static inline struct trie_node *__attribute__((always_inline)) touch_trie_node(struct trie_node **p_node)
{
	struct trie_node *node_entry, *node_ptr;
	uint16_t count;

	node_ptr = TRIE_GET_ADDR(ATOMIC_LOAD_RELAX(p_node));
	if (!node_ptr)
		node_ptr = trie_init(p_node);
	ASSERT(node_ptr != NULL);

	return node_ptr;
}

static inline struct trie_node *__attribute__((always_inline)) acquire_trie_node(struct trie_node **p_node)
{
	struct trie_node *node_entry, *node_ptr;
	uint64_t count;

	node_entry = ATOMIC_LOAD_RELAX(p_node);
	while (1) {
		count = TRIE_GET_META(node_entry);
		if (count == 0) {
			return NULL;
		}
		if (ATOMIC_CAS_REL_ACQ(p_node, &node_entry,
				       (struct trie_node *)((uint64_t)node_entry + TRIE_META_ONE))) {
			break;
		}
	}
	node_ptr = TRIE_GET_ADDR(node_entry);

	return node_ptr;
}

// Returns bool whether it needs to decrease count of parent node
static inline bool __attribute__((always_inline))
release_trie_node(struct trie_node **p_node, bool two_sub, int level, int index)
{
	uint16_t count;
	uint64_t subtracted;
	struct trie_node *node_entry, *node_ptr;

	subtracted = two_sub ? (TRIE_META_ONE * 2) : TRIE_META_ONE;
	node_entry = ATOMIC_SUB_FETCH_RELEASE(p_node, subtracted);
	count = TRIE_GET_META(node_entry);

	if (count == 0) {
		node_ptr = TRIE_GET_ADDR(node_entry);
		// !!! Order is important : unlink -> inv cache -> reclaim
		// other thread should check count
		ATOMIC_STORE_RELEASE(p_node, NULL); // Unlink
#ifdef CONFIG_TRIE_CACHE
		invalidate_cached_node(level, node_ptr);
#endif
		reclaim(node_ptr);

		// __atomic_fetch_sub(&trie_usage, PAGE_SIZE, __ATOMIC_RELAXED);
		return true;
	}

	return false;
}

// iter : uninitiliazed iterator pointer
// root : valid root pointer
// key : valid or invalid key
// ret : NULL if failed, otherwise L1 entry that points to trie_entry
// side effects :
// L3, L2 access counts are incremented
// L1 table is locked
// Safety : it should be released with finish_trie_iter or next returns NULL
void **trie_iter_init(struct user_trie_iter *iter, struct trie_node *root, uint64_t key)
{
	struct trie_node *temp;
	bool is_last;

	for (int i = 0; i < 4; i++) {
		iter->indices[i] = (key >> (12 + i * 9)) & 0x1ff;
	}

	// L4 is always vaild
	iter->nodes[4] = root;

	// Check L3 info & access if vaild
	iter->nodes[3] = acquire_trie_node(&iter->nodes[4]->trie_node[iter->indices[3]]);
	if (iter->nodes[3] == NULL) {
		return NULL;
	}

	// Now iter->nodes[3] is valid until release
	// Check L2 info & access if vaild
	iter->nodes[2] = acquire_trie_node(&iter->nodes[3]->trie_node[iter->indices[2]]);
	if (iter->nodes[2] == NULL) {
		goto L3_RELEASE;
	}

	// Now iter->nodes[2] is valid until release
	// Check L1 info & lock if valid
	iter->nodes[1] = lock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]]);
	if (iter->nodes[1] == NULL) {
		goto L2_RELEASE;
	}

	// Now iter->nodes[1] is valid until unlock
	return &iter->nodes[1]->data[iter->indices[0]];

	// L1
L2_RELEASE:
	is_last = release_trie_node(&iter->nodes[3]->trie_node[iter->indices[2]], false, 2, iter->indices[2]);
L3_RELEASE:
	release_trie_node(&iter->nodes[4]->trie_node[iter->indices[3]], is_last, 3, iter->indices[3]);

	return NULL;
}

// Releases L2, L3 table
void trie_iter_finish(struct user_trie_iter *iter)
{
	bool is_last;

	is_last = unlock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]], iter->indices[1]);
	is_last = release_trie_node(&iter->nodes[3]->trie_node[iter->indices[2]], is_last, 2, iter->indices[2]);
	release_trie_node(&iter->nodes[4]->trie_node[iter->indices[3]], is_last, 3, iter->indices[3]);
}

void **trie_iter_next(struct user_trie_iter *iter)
{
	bool is_last;

	iter->indices[0] = (iter->indices[0] + 1) & 0x1ff;
	if (iter->indices[0] != 0) {
		return &iter->nodes[1]->data[iter->indices[0]];
	}

	// idx0 == 0
	is_last = unlock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]], iter->indices[1]);
	iter->indices[1] = (iter->indices[1] + 1) & 0x1ff;
	if (iter->indices[1] != 0) {
		iter->nodes[1] = lock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]]);
		if (iter->nodes[1] == NULL) {
			goto L2_RELEASE;
		}
		if (is_last) {
			is_last = release_trie_node(&iter->nodes[3]->trie_node[iter->indices[2]], false, 2,
						    iter->indices[2]);
			ASSERT(is_last == 0); // Since lock suceeded
		}
		return &iter->nodes[1]->data[0];
	}

	// idx1 == 0
	is_last = release_trie_node(&iter->nodes[3]->trie_node[iter->indices[2]], is_last, 2, iter->indices[2]);
	iter->indices[2] = (iter->indices[2] + 1) & 0x1ff;
	if (iter->indices[2] != 0) {
		iter->nodes[2] = acquire_trie_node(&iter->nodes[3]->trie_node[iter->indices[2]]);
		if (iter->nodes[2] == NULL) {
			goto L3_RELEASE;
		}
		if (is_last) {
			is_last = release_trie_node(&iter->nodes[4]->trie_node[iter->indices[3]], false, 3,
						    iter->indices[3]);
			ASSERT(is_last == 0); // Since acquire success
		}
		iter->nodes[1] = lock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]]);
		if (iter->nodes[1] == NULL) {
			goto L2_RELEASE;
		}
		return &iter->nodes[1]->data[0];
	}

	// idx2 == 0
	release_trie_node(&iter->nodes[4]->trie_node[iter->indices[3]], is_last, 3, iter->indices[3]);
	iter->indices[3] = (iter->indices[3] + 1) & 0x1ff;
	if (iter->indices[3] != 0) {
		iter->nodes[3] = acquire_trie_node(&iter->nodes[4]->trie_node[iter->indices[3]]);
		if (iter->nodes[3] == NULL) {
			return NULL;
		}
		iter->nodes[2] = acquire_trie_node(&iter->nodes[3]->trie_node[iter->indices[2]]);
		if (iter->nodes[2] == NULL) {
			is_last = false;
			goto L3_RELEASE;
		}
		iter->nodes[1] = lock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]]);
		if (iter->nodes[1] == NULL) {
			goto L2_RELEASE;
		}
		return &iter->nodes[1]->data[0];
	}

	PANIC("overflow");

L2_RELEASE:
	is_last = release_trie_node(&iter->nodes[3]->trie_node[iter->indices[2]], is_last, 2, iter->indices[2]);
L3_RELEASE:
	release_trie_node(&iter->nodes[4]->trie_node[iter->indices[3]], is_last, 3, iter->indices[3]);

	return NULL; // releases, unlocks. now iter is not valid
}

// Should be succed always
// do not need to increase access count since ref_cnt > 0 before deletion
void **trie_alloc_iter_init(struct user_trie_iter *iter, struct trie_node *root, uint64_t key)
{
	struct trie_node *temp;

	for (int i = 0; i < 4; i++) {
		iter->indices[i] = (key >> (12 + i * 9)) & 0x1ff;
	}

	// L4 is always vaild
	iter->nodes[4] = root;
	iter->nodes[3] = touch_trie_node(&iter->nodes[4]->trie_node[iter->indices[3]]);
	iter->nodes[2] = touch_trie_node(&iter->nodes[3]->trie_node[iter->indices[2]]);
	iter->nodes[1] = touch_lock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]]);

	// Now iter->nodes[1] is valid until unlock
	return &iter->nodes[1]->data[iter->indices[0]];
}

void trie_alloc_iter_finish(struct user_trie_iter *iter)
{
	bool is_last = unlock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]], iter->indices[1]);
	ASSERT(is_last == false); // not yet deleted
}

void **trie_alloc_iter_next(struct user_trie_iter *iter)
{
	bool is_last;

	iter->indices[0] = (iter->indices[0] + 1) & 0x1ff;
	if (iter->indices[0] != 0) {
		return &iter->nodes[1]->data[iter->indices[0]];
	}

	// idx0 == 0
	is_last = unlock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]], iter->indices[1]);
	ASSERT(is_last == false);
	iter->indices[1] = (iter->indices[1] + 1) & 0x1ff;
	if (iter->indices[1] != 0) {
		iter->nodes[1] = touch_lock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]]);
		return &iter->nodes[1]->data[0];
	}

	// idx1 == 0
	iter->indices[2] = (iter->indices[2] + 1) & 0x1ff;
	if (iter->indices[2] != 0) {
		iter->nodes[2] = touch_trie_node(&iter->nodes[3]->trie_node[iter->indices[2]]);
		iter->nodes[1] = touch_lock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]]);
		return &iter->nodes[1]->data[0];
	}

	// idx2 == 0
	iter->indices[3] = (iter->indices[3] + 1) & 0x1ff;
	if (iter->indices[3] != 0) {
		iter->nodes[3] = touch_trie_node(&iter->nodes[4]->trie_node[iter->indices[3]]);
		iter->nodes[2] = touch_trie_node(&iter->nodes[3]->trie_node[iter->indices[2]]);
		iter->nodes[1] = touch_lock_trie_node(&iter->nodes[2]->trie_node[iter->indices[1]]);
		return &iter->nodes[1]->data[0];
	}

	PANIC("overflow");
}

// TODO : batching
// ONly valid for L1 table that does not have acces count
void trie_iter_dec_cnt(struct user_trie_iter *iter)
{
	int count;

#ifdef CONFIG_DEBUG_LOCK
	if (TRIE_GET_LOCK(ATOMIC_LOAD_RELAX(&iter->nodes[2]->trie_node[iter->indices[1]])) != 1) {
		printk("[%lx] not locked 0x%lx\n", pthread_self(), &iter->nodes[2]->trie_node[iter->indices[1]]);
		for (;;)
			;
	}
#endif

	ATOMIC_STORE_RELEASE(&iter->nodes[2]->trie_entry[iter->indices[1]],
			     iter->nodes[2]->trie_entry[iter->indices[1]] - TRIE_META_ONE);
}

int trie_remove_range(struct trie_node *root, uint64_t start, uint64_t end)
{
	struct trie_node *index_1_node;
	struct trie_node *index_2_node;
	struct trie_node *index_3_node;
	struct trie_node *index_4_node;
	int index_1;
	int index_2;
	int index_3;
	int index_4;
	uint64_t next_index_1;
	uint64_t next_index_2;
	uint64_t next_index_3;
	uint64_t next_index_4;
	uint64_t addr;
	int ret;
	bool is_last;
	size_t diff_cnt;

	// End 0 means the entire address space
	if (end == 0)
		end = (uint64_t)-PAGE_SIZE;

	ASSERT(root != NULL);
	ASSERT(end > start);
	ASSERT((start & (PAGE_SIZE - 1)) == 0);
	ASSERT((end & (PAGE_SIZE - 1)) == 0);

	// printk("%d %d %d\n", index_1, index_2, index_3);
	addr = start;
	index_4_node = root;
	index_4 = (addr >> 39) & 0x1ff;

	do {
		next_index_4 = ADDR_END(addr, end, 39);
		index_3_node = acquire_trie_node(&index_4_node->trie_node[index_4]);
		ASSERT(index_3_node);
		index_3 = (addr >> 30) & 0x1ff;
		do {
			next_index_3 = ADDR_END(addr, end, 30);
			index_2_node = acquire_trie_node(&index_3_node->trie_node[index_3]);
			ASSERT(index_2_node);
			index_2 = (addr >> 21) & 0x1ff;
			do {
				next_index_2 = ADDR_END(addr, end, 21);
				index_1_node = lock_trie_node(&index_2_node->trie_node[index_2]);
				ASSERT(index_1_node);
				diff_cnt = (next_index_2 - addr) >> PAGE_SHIFT;
				ATOMIC_STORE_RELEASE(&index_2_node->trie_node[index_2],
						     index_2_node->trie_node[index_2] - TRIE_META_ONE * diff_cnt);
				is_last = unlock_trie_node(&index_2_node->trie_node[index_2], index_2);
				ASSERT((next_index_2 == next_index_3) || !is_last);
			} while (index_2++, addr = next_index_2, addr != next_index_3);
			is_last = release_trie_node(&index_3_node->trie_node[index_3], is_last, 2, index_3);
			ASSERT((next_index_3 == next_index_4) || !is_last);
		} while (index_3++, addr = next_index_3, addr != next_index_4);
		release_trie_node(&index_4_node->trie_node[index_4], is_last, 3, index_4);
	} while (index_4++, addr = next_index_4, addr != end);

	return 0;
}