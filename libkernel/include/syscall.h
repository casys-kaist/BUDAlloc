#pragma once

#include <asm/unistd_64.h>
#include "lib/errno.h"
#include "lib/stddef.h"
#include "lib/types.h"

#define MAX_SYSCALL_NR 512

#define __MAP0(m, ...)
#define __MAP1(m, t, a, ...) m(t, a)
#define __MAP2(m, t, a, ...) m(t, a), __MAP1(m, __VA_ARGS__)
#define __MAP3(m, t, a, ...) m(t, a), __MAP2(m, __VA_ARGS__)
#define __MAP4(m, t, a, ...) m(t, a), __MAP3(m, __VA_ARGS__)
#define __MAP5(m, t, a, ...) m(t, a), __MAP4(m, __VA_ARGS__)
#define __MAP6(m, t, a, ...) m(t, a), __MAP5(m, __VA_ARGS__)
#define __MAP(n, ...) __MAP##n(__VA_ARGS__)

#define __SC_DECL(t, a) t a

typedef union syscall_ptr {
	long (*syscall_arg0)(void);
	long (*syscall_arg1)(unsigned long);
	long (*syscall_arg2)(unsigned long, unsigned long);
	long (*syscall_arg3)(unsigned long, unsigned long, unsigned long);
	long (*syscall_arg4)(unsigned long, unsigned long, unsigned long, unsigned long);
	long (*syscall_arg5)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
	long (*syscall_arg6)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
} syscall_ptr_t;

typedef struct syscall_info {
	const char *syscall_name;
	uint32_t syscall_nr;
	uint32_t arg_nr;
	syscall_ptr_t syscall_p;
} syscall_info_t;

#define __SYSCALL_DEFINE_METADATA(nr, x, name)                                                                         \
	struct syscall_info __syscall_info_sys##name = {                                                               \
		.syscall_name = "sys" #name,                                                                           \
		.syscall_nr = nr,                                                                                      \
		.arg_nr = x,                                                                                           \
		.syscall_p = NULL,                                                                                     \
	};                                                                                                             \
	struct syscall_info __attribute__((section("__syscalls_info"))) *__p_syscall_meta_##name =                     \
		&__syscall_info_sys##name;

#define __SYSCALL_DEFINEx(nr, x, name, ...)                                                                            \
	__SYSCALL_DEFINE_METADATA(nr, x, name)                                                                         \
	long sys##name(__MAP(x, __SC_DECL, __VA_ARGS__))

#define SYSCALL_DEFINE0(nr, name)                                                                                      \
	__SYSCALL_DEFINE_METADATA(nr, 0, name)                                                                         \
	long sys_##name()
#define SYSCALL_DEFINE1(nr, name, ...) __SYSCALL_DEFINEx(nr, 1, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE2(nr, name, ...) __SYSCALL_DEFINEx(nr, 2, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE3(nr, name, ...) __SYSCALL_DEFINEx(nr, 3, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE4(nr, name, ...) __SYSCALL_DEFINEx(nr, 4, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE5(nr, name, ...) __SYSCALL_DEFINEx(nr, 5, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE6(nr, name, ...) __SYSCALL_DEFINEx(nr, 6, _##name, __VA_ARGS__)

#define SYSCALL_DEFINE_MAXARGS 6

#define VALIDATE_PTR(PTR)                                                                                              \
	do {                                                                                                           \
		if (syscall_validate_ptr((void *)(PTR)))                                                               \
			return -EINVAL;                                                                                \
	} while (0)

struct syscall_register {
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rdx;
	uint64_t r10;
	uint64_t r8;
	uint64_t r9;
} __attribute__((packed));

/// system call handler
long handle_syscall(int n, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);

// From this, definition of system calls goes by
long sys_ni(); // index -1
long sys_exit(int); //

long sys_mmap(void *, size_t, int, int, int, off_t);
long sys_munmap(void *, size_t);
long sys_mprotect(void *, size_t, int);
long sys_madvise(void *, size_t, int);
long sys_exit_group(int);