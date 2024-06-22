#pragma once

#include "lib/stddef.h"

// Refered https://rigtorp.se/spinlock/

typedef struct spinlock {
	// Is locked?
	int locked;
} spinlock_t __attribute__((aligned(64)));

#if defined(__i386__) || defined(__x86_64__)
static inline void __attribute__((always_inline)) spin_lock(spinlock_t *spl)
{
	for (;;) {
		// Optimistically assume the lock is free on the first try
		if (__atomic_exchange_n(&spl->locked, 1, __ATOMIC_ACQ_REL) == 0) {
			return;
		}
		// Wait for lock to be released without generating cache misses
		while (__atomic_load_n(&spl->locked, __ATOMIC_RELAXED) != 0) {
			// Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
			// hyper-threads
			__builtin_ia32_pause();
		}
	}
}

// Unlock spinlock
static inline void __attribute__((always_inline)) spin_unlock(spinlock_t *spl)
{
	__atomic_store_n(&spl->locked, 0, __ATOMIC_RELEASE);
}

// Lock spinlock without acquiring spin lock
static inline void __attribute__((always_inline)) spin_lock_init(spinlock_t *spl)
{
	spl->locked = 0;
}

// try lock spin lock else return false immediately
static inline int __attribute__((always_inline)) spin_trylock(spinlock_t *spl)
{
	return __atomic_load_n(&spl->locked, __ATOMIC_RELAXED) == 0 &&
	       __atomic_exchange_n(&spl->locked, 1, __ATOMIC_ACQ_REL) == 0;
}
#endif

static inline int mbpf_spin_trylock(spinlock_t *spl)
{
	// return __atomic_load_n(&spl->locked, __ATOMIC_RELAXED) == 0 &&
	//        __atomic_exchange_n(&spl->locked, 1, __ATOMIC_ACQUIRE) == 0;
	return spl->locked == 0 && __atomic_exchange_n(&spl->locked, 1, __ATOMIC_ACQ_REL) == 0;
}

static inline void mbpf_spin_unlock(spinlock_t *spl)
{
	// __atomic_store_n(&spl->locked, 0, __ATOMIC_RELEASE);
	spl->locked = 0; // TODO : validate correctneess of ordering
}
