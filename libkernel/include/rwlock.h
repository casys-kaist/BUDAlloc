#pragma once

// refer https://doc.dpdk.org/api/rte__rwlock_8h_source.html

#define RWLOCK_WAIT 0x1 /* Writer is waiting */
#define RWLOCK_WRITE 0x2 /* Writer has the lock */
#define RWLOCK_MASK (RWLOCK_WAIT | RWLOCK_WRITE)
/* Writer is waiting or has lock */
#define RWLOCK_READ 0x4 /* Reader increment */

typedef struct rwlock {
	unsigned long long int cnt;
} rwlock_t __attribute__((aligned(64)));

#ifdef CONFIG_SUPPORT_FORK_SAFETY
#if defined(__i386__) || defined(__x86_64__)
#include "lib/compiler.h"
static inline void __attribute__((always_inline)) rwlock_init(rwlock_t *rwl)
{
	rwl->cnt = 0;
}

static inline void __attribute__((always_inline)) rwlock_read_lock(rwlock_t *rwl)
{
	unsigned long long int x;
	while (1) {
		while (__atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED) & RWLOCK_MASK) {
			__builtin_ia32_pause();
		}
		x = __atomic_add_fetch(&rwl->cnt, RWLOCK_READ, __ATOMIC_ACQUIRE);
		if (likely(!(x & RWLOCK_MASK)))
			return;
		__atomic_fetch_sub(&rwl->cnt, RWLOCK_READ, __ATOMIC_RELAXED);
	}
}

static inline void __attribute__((always_inline)) rwlock_read_unlock(rwlock_t *rwl)
{
	__atomic_fetch_sub(&rwl->cnt, RWLOCK_READ, __ATOMIC_RELEASE);
}

static inline void __attribute__((always_inline)) rwlock_write_lock(rwlock_t *rwl)
{
	unsigned long long int x;

	while (1) {
		x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);

		/* No readers or writers? */
		if (likely(x < RWLOCK_WRITE)) {
			/* Turn off RTE_RWLOCK_WAIT, turn on RTE_RWLOCK_WRITE */
			if (__atomic_compare_exchange_n(&rwl->cnt, &x, RWLOCK_WRITE, 0, __ATOMIC_ACQUIRE,
							__ATOMIC_RELAXED))
				return;
		}

		/* Turn on writer wait bit */
		if (!(x & RWLOCK_WAIT))
			__atomic_fetch_or(&rwl->cnt, RWLOCK_WAIT, __ATOMIC_RELAXED);

		/* Wait until no readers before trying again */
		while (__atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED) > RWLOCK_WAIT)
			__builtin_ia32_pause();
	}
}

static inline void __attribute__((always_inline)) rwlock_write_unlock(rwlock_t *rwl)
{
	__atomic_fetch_sub(&rwl->cnt, RWLOCK_WRITE, __ATOMIC_RELEASE);
}
#else
static inline int mbpf_rwlock_read_trylock(rwlock_t *rwl)
{
	unsigned long long int x;
	// x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);
	x = rwl->cnt;
	if (x & RWLOCK_MASK) {
		return 0; // failed
	}
	x = __atomic_add_fetch(&rwl->cnt, RWLOCK_READ, __ATOMIC_ACQUIRE);
	if (x & RWLOCK_MASK) {
		__atomic_fetch_sub(&rwl->cnt, RWLOCK_READ, __ATOMIC_RELEASE);
		return 0;
	}
	return 1;
}

#define AADD(DST, V) asm volatile("lock *(u64 *)(%0+0) += %1" : "=r"(DST) : "r"(V), "0"(DST))

static inline void mbpf_rwlock_read_unlock(rwlock_t *rwl)
{
	__atomic_fetch_sub(&rwl->cnt, RWLOCK_READ, __ATOMIC_RELEASE);
}
#endif
#else
#define rwlock_init(rwlock_t) ((void)0);
#define rwlock_read_lock(rwlock_t) ((void)0);
#define rwlock_read_unlock(rwlock_t) ((void)0);
#define rwlock_write_lock(rwlock_t) ((void)0);
#define rwlock_write_unlock(rwlock_t) ((void)0);
#define mbpf_rwlock_read_trylock(rwlock_t) (1)
#define mbpf_rwlock_read_unlock(rwlock_t) ((void)0);
#endif
