#include "lib/compiler.h"
#include "lib/errno.h"
#include "memory.h"
#include "plat.h"
#include "syscall.h"
#include "vmem.h"


#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#ifndef MAP_UNINITIALIZED
#define MAP_UNINITIALIZED 0x4000000
#endif /* !MAP_UNINITIALIZED */

#define VALID_PROT_MASK (PROT_READ | PROT_WRITE | PROT_EXEC)

#define USE_BPF

int do_mmap(void **addr, size_t len, int prot, int flags, int fd, off_t offset, void *aux)
{
	struct mm_struct *mm = mm_get_active();
	vaddr_t vaddr;
	const struct uk_vma_ops *vops;
	int rc;

	if (unlikely(len == 0))
		return -EINVAL;

	/* len will overflow when aligning it to page size */
	if (unlikely(len > SIZE_MAX - PAGE_SIZE))
		return -ENOMEM;

	if (unlikely(offset < 0 || !PAGE_ALIGNED(offset)))
		return -EINVAL;

	if (unlikely((size_t)offset > SIZE_MAX - len))
		return -EOVERFLOW;

	if (*addr) {
		if (flags & MAP_FIXED)
			TODO();

		vaddr = (vaddr_t)*addr;
	} else {
		/* Linux returns -EPERM if addr is NULL for a fixed mapping */
		if (unlikely(flags & MAP_FIXED))
			return -EPERM;

		if (unlikely(flags & MAP_FIXED_NOREPLACE))
			return -EPERM;

		vaddr = 0;
	}

	if (flags & MAP_ANONYMOUS) {
		if ((flags & MAP_SHARED) || (flags & MAP_SHARED_VALIDATE) == MAP_SHARED_VALIDATE) {
			/* MAP_SHARED(_VALIDATE): Note, we ignore it for
			 * anonymous memory since we only have a single
			 * process. There is no one to share the mapping with.
			 * It is thus fine to create a private mapping.
			 */
		} else if (unlikely(!(flags & MAP_PRIVATE)))
			return -EINVAL;

		/* We do not support unrestricted VMAs */
		if (unlikely(flags & MAP_GROWSDOWN))
			return -ENOTSUP;

		if (flags & MAP_UNINITIALIZED)
			TODO();

		if (flags & MAP_HUGETLB) {
			return -EINVAL;
		}
	} else {
		return -ENOTSUP;
	}

	/* Linux will always align len to the selected page size */
	len = PAGE_ALIGN(len);

	if (flags & MAP_POPULATE)
		TODO();

	/* MAP_LOCKED: Ignored for now */
	/* MAP_NONBLOCKED: Ignored for now */
	/* MAP_NORESERVE : Ignored for now */

	// need lock
	pthread_mutex_lock(&mutex);
	rc = vma_map(mm, &vaddr, len, prot, flags, NULL, aux);
	pthread_mutex_unlock(&mutex);
	// need unlock

	*addr = (void *)vaddr;

	return rc;
}

SYSCALL_DEFINE6(9, mmap, void *, addr, size_t, len, int, prot, int, flags, int, fd, off_t, offset)
{
	int ret;

	ret = do_mmap(&addr, len, prot, flags, fd, offset, NULL);

	if (unlikely(ret)) {
		errno = -ret;
		return (long)(-1);
	}

	return (long)addr;
}

SYSCALL_DEFINE3(10, mprotect, void *, addr, size_t, len, int, prot)
{
	struct mm_struct *mm = mm_get_active();
	vaddr_t vaddr = (vaddr_t)addr;
	int rc;

	pthread_mutex_lock(&mutex);
	rc = vma_set_prot(mm, vaddr, PAGE_ALIGN(len), prot);
	pthread_mutex_unlock(&mutex);
	if (unlikely(rc)) {
		if (rc == -ENOENT)
			return -ENOMEM;

		return rc;
	}

	return 0;
}

SYSCALL_DEFINE2(11, munmap, void *, addr, size_t, len)
{
	struct mm_struct *mm = mm_get_active();
	int rc;

	if (unlikely(len == 0))
		return -EINVAL;

	pthread_mutex_lock(&mutex);
	rc = vma_unmap(mm, (vaddr_t)addr, PAGE_ALIGN(len));
	pthread_mutex_unlock(&mutex);
	if (unlikely(rc)) {
		if (rc == -ENOENT)
			return 0;

		return rc;
	}
	return 0;
}

SYSCALL_DEFINE3(28, madvise, void *, addr, size_t, len, int, advice)
{
	struct mm_struct *mm = mm_get_active();
	vaddr_t vaddr = (vaddr_t)addr;
	int rc;

	pthread_mutex_lock(&mutex);
	rc = vma_advise(mm, vaddr, len, advice);
	pthread_mutex_unlock(&mutex);
	if (unlikely(rc)) {
		if (rc == -ENOENT)
			return -ENOMEM;

		return rc;
	}

	return 0;
}
