#include "errno.h"
#include "lib/types.h"
#include "plat.h"
#include <asm/unistd.h>
#include <asm/unistd_64.h>
#include <stdlib.h>

#ifndef asm_impl
#define asm_impl

void ____asm_impl(void)
{
	/*
	 * enter_syscall triggers a kernel-space system call
	 */
	asm volatile(".globl enter_syscall \n\t"
		     "enter_syscall: \n\t"
		     "movq %rdi, %rax \n\t"
		     "movq %rsi, %rdi \n\t"
		     "movq %rdx, %rsi \n\t"
		     "movq %rcx, %rdx \n\t"
		     "movq %r8, %r10 \n\t"
		     "movq %r9, %r8 \n\t"
		     "movq 8(%rsp),%r9 \n\t"
		     "syscall \n\t"
		     "ret \n\t");
}
#endif

void *plat_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	return (void *)enter_syscall(__NR_mmap, addr, length, prot, flags, fd, offset);
}

int plat_munmap(void *addr, size_t length)
{
	return (int)enter_syscall(__NR_munmap, addr, length);
}

int plat_mprotect(void *addr, size_t length, int prot)
{
	return (int)enter_syscall(__NR_mprotect, addr, length, prot);
}

int plat_madvise(void *addr, size_t length, int advice)
{
	return (int)enter_syscall(__NR_madvise, addr, length, advice);
}

ssize_t plat_write(int fd, const void *buf, size_t count)
{
	return (ssize_t)enter_syscall(__NR_write, fd, buf, count);
}

ssize_t plat_read(int fd, void *buf, size_t count)
{
	return (ssize_t)enter_syscall(__NR_read, fd, buf, count);
}

void plat_exit(int status)
{
	enter_syscall(__NR_exit, status);
}

void plat_exit_group(int status)
{
	enter_syscall(__NR_exit_group, status);
}

int plat_clone(int (*__fn)(void *__arg), void *__child_stack, int __flags, void *__arg, ...)
{
	int pid;

	if (!__fn || !__child_stack)
		return -EINVAL;

	__child_stack -= 2;
	*(void **)(__child_stack + 1) = __arg;
	*(void **)(__child_stack) = __fn;

	asm volatile("syscall\n\t" : "=a"(pid) : "0"(__NR_clone), "D"(__flags), "S"(__child_stack));

	if (!pid) {
		asm volatile("xorl %ebp, %ebp\n\t"
			     "popq %rax\n\t"
			     "popq %rcx\n\t"
			     "call *%rax\n\t"
			     "movq %rax, %rdi\n\t"
			     "movl $60, %eax\n\t"
			     "syscall\n\t");
	}

	return pid;
}

int plat_kill(int pid, int sig)
{
	return enter_syscall(__NR_kill, pid, sig);
}

int plat_getpid(void)
{
	return enter_syscall(__NR_getpid);
}

int plat_open(const char *pathname, int flags, mode_t mode)
{
	return enter_syscall(__NR_open, pathname, flags, mode);
}

int plat_close(int fd)
{
	return enter_syscall(__NR_close, fd);
}

int plat_access(const char *pathname, int mode)
{
	return enter_syscall(__NR_access, pathname, mode);
}

int plat_dup(int oldfd)
{
	return enter_syscall(__NR_dup, oldfd);
}

int plat_dup2(int oldfd, int fd)
{
	return enter_syscall(__NR_dup2, oldfd, fd);
}