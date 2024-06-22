#pragma once

#include "lib/stddef.h"
#include "lib/types.h"
#include <stdint.h>

#define TRI_SIZE 512
#define TRIE_INV_MARK ((uint16_t)-1)
// #define DEBUG_TRIE

struct trie_node {
	union {
		struct trie_node *trie_node[TRI_SIZE];
		uint64_t trie_entry[TRI_SIZE];
		struct l2_entry {
			uint8_t lock;
			uint16_t alloc_count;
		} __attribute((aligned(64))) l2_entry[TRI_SIZE];
		void *data[TRI_SIZE];
	};
};

struct user_trie_iter {
	struct trie_node *nodes[5]; // 1~4 are valid
	uint16_t indices[4]; // 0~3 are valid
};

#define ADDR_END(addr, end, shift)                                                                                     \
	({                                                                                                             \
		unsigned long __boundary = ((addr) + (1UL << shift)) & ~((1UL << shift) - 1);                          \
		(__boundary - 1 < (end)-1) ? __boundary : (end);                                                       \
	})

typedef int (*trie_func)(void *node, uint64_t addr, void *aux);

#define TRIE_META_SHIFT 48
#define TRIE_META_MASK ((0x1ULL << TRIE_META_SHIFT) - 1)
#define TRIE_LOWER_MASK (~0xfffULL)
#define TRIE_GET_META(data) ((uint64_t)data >> TRIE_META_SHIFT)
#define TRIE_SET_META(data, meta)                                                                                      \
	(data = (void *)((uint64_t)TRIE_GET_ADDR(data) | ((uint64_t)(meta) << TRIE_META_SHIFT)))
#define TRIE_META_INV ((void *)(0xffffULL << TRIE_META_SHIFT))
#define TRIE_META_ONE (1ULL << TRIE_META_SHIFT)

// Valid for L2
#define TRIE_UNLOCKED_BIT (0)
#define TRIE_LOCKED_BIT (1)
#define TRIE_DESTORYED_BIT (2)
#define TRIE_LOCKED(X) (((uint64_t)(X)&TRIE_LOCKED_BIT) != 0)
#define TRIE_DESTROYED(X) (((uint64_t)(X)&TRIE_DESTORYED_BIT) != 0)
#define TRIE_GET_LOCK(X) ((uint64_t)(X)&3)
#define TRIE_LOCK_MASK (~(3ULL))

// #define TRIE_GET_ADDR(data) (struct trie_node *)((uint64_t)(data) & (TRIE_META_MASK & TRIE_LOCK_MASK))
#define TRIE_GET_ADDR(data) (struct trie_node *)((uint64_t)(data) & (TRIE_META_MASK & TRIE_LOWER_MASK))
#define TRIE_GET_CANON(data) ((uint64_t)(data) & (PAGE_META_MASK & TRIE_META_MASK))
// For interiror nodes
#define TRIE_INC_REF_CNT(data) TRIE_SET_META(data, TRIE_GET_META(data) + 1);
#define TRIE_DEC_REF_CNT(data) TRIE_SET_META(data, TRIE_GET_META(data) - 1);
#define TRIE_INITIAL_CNT (0x200ULL << TRIE_META_SHIFT)

#define TRIE_ENTRY_LOAD(M) __atomic_load_n((M), __ATOMIC_SEQ_CST)
#define ATOMIC_LOAD_RELAX(M) __atomic_load_n((M), __ATOMIC_RELAXED)
#define ATOMIC_STORE_RELEASE(M, V) __atomic_store_n((M), V, __ATOMIC_RELEASE)
#define ATOMIC_LOAD_ACQUIRE(M) __atomic_load_n((M), __ATOMIC_ACQUIRE)
#define ATOMIC_CAS_REL_ACQ(M, P_EXPECTED, DESIRED)                                                                     \
	__atomic_compare_exchange_n(M, P_EXPECTED, DESIRED, false, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)
#define ATOMIC_SUB_FETCH_RELEASE(M, V) __atomic_sub_fetch((M), V, __ATOMIC_RELEASE)
#define ATOMIC_FETCH_SUB_RELEASE(M, V) __atomic_fetch_sub((M), V, __ATOMIC_RELEASE)

#if defined(__i386__) || defined(__x86_64__)
#define PAUSE() __builtin_ia32_pause()
#else
#if !defined(__clang__)
#error "Not x86"
#endif
#endif

#define TRIE_ENTRY_STORE(M, V) __atomic_store_n((M), V, __ATOMIC_SEQ_CST)
#define TRIE_ENTRY_FETCH_SUB(M, V) __atomic_fetch_sub((M), V, __ATOMIC_SEQ_CST)
#define TRIE_ENTRY_SUB_FETCH(M, V) __atomic_sub_fetch((M), V, __ATOMIC_SEQ_CST)
#define TRIE_ENTRY_FETCH_OR(M, V) __atomic_fetch_or((M), V, __ATOMIC_SEQ_CST)

#define CAS(M, P_EXPECTED, DESIRED)                                                                                    \
	__atomic_compare_exchange_n(M, P_EXPECTED, DESIRED, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

/*
// For pointers to L3, L2 tables
** [63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 ..... 0]
** [            <alloc + access cnt>               |   <addr> ]

// For pointers to L1 tables
** [63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 ..... 0]
** [..........|<lock>|       <alloc cnt>           |   <addr> ]

// For leaf
** [63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 ..... 0]
** [.............................|    <index>      |   <addr> ]
*/
#define TRIE_INDEX_LARGE ((uint64_t)63)

#define TRIE_TIDX_SHIFT 54


struct trie_node;
void *trie_insert(struct trie_node *root, const void *alias_page, void *entry);
void *trie_remove(struct trie_node *root, const void *alias_page);
void *trie_search(struct trie_node *root, const void *alias_page);
void **trie_search_node(struct trie_node *root, const void *alias_page);
int trie_insert_range(struct trie_node *root, uint64_t start, uint64_t end, bool touch, trie_func func, void *aux);
int trie_free_range(struct trie_node *root, uint64_t start, uint64_t end, trie_func func, void *aux);
void trie_debug(struct trie_node *root);

void **trie_iter_init(struct user_trie_iter *iter, struct trie_node *root, uint64_t key);
void trie_iter_finish(struct user_trie_iter *iter);
void **trie_iter_next(struct user_trie_iter *iter);

void **trie_alloc_iter_init(struct user_trie_iter *iter, struct trie_node *root, uint64_t key);
void trie_alloc_iter_finish(struct user_trie_iter *iter);
void **trie_alloc_iter_next(struct user_trie_iter *iter);

void trie_iter_dec_cnt(struct user_trie_iter *iter);

int trie_remove_range(struct trie_node *root, uint64_t start, uint64_t end);
