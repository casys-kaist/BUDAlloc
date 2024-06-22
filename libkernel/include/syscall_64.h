#pragma once

#include <asm/unistd_64.h>
#include "syscall.h"

/* This file is included at the end of the syscall initialization
	const sys_call_ptr_t sys_call_table[__NR_syscall_max+1] = {
    [0 ... __NR_syscall_max] = &sys_ni_syscall,
    [0] = sys_read,
    [1] = sys_write,
    [2] = sys_open,
    ...
    ...
    ...
}; */

#define __SYSCALL_COMMON(nr, sym) __SYSCALL_64(nr, sym)
#define __SYSCALL_64(nr, sym) [nr] = (long int (*)(void))sym,

__SYSCALL_COMMON(__NR_mmap, sys_mmap)
__SYSCALL_COMMON(__NR_munmap, sys_munmap)
__SYSCALL_COMMON(__NR_mprotect, sys_mprotect)
__SYSCALL_COMMON(__NR_madvise, sys_madvise)
__SYSCALL_COMMON(__NR_exit_group, sys_exit_group)