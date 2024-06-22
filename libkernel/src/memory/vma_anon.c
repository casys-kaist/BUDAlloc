#include "debug.h"
#include "memory.h"
#include "sbpf/bpf.h"
#include "vmem.h"
#include <linux/bpf.h>
#include <sys/mman.h>

struct sbpf *page_ops_bpf;

int vma_op_mmap(struct vm_area_struct *vma)
{
	ASSERT(vma != NULL);

	return 0;
}

int vma_op_unmap(struct vm_area_struct *vma, vaddr_t vaddr, size_t len)
{
	ASSERT(vma != NULL);
	ASSERT(vaddr == vma->vm_start);
	ASSERT(vaddr + len == vma->vm_end);
	ASSERT(PAGE_ALIGNED(len));
#ifdef CONFIG_MEMORY_DEBUG
	void *args[4] = { (void *)PTE_UNMAP, (void *)vma->vm_start, (void *)vma->vm_end, (void *)mm_get_active() };
	return sbpf_call_function(page_ops_bpf, &args[0], sizeof(void *) * 4);
#else
	void *args[3] = { (void *)PTE_UNMAP, (void *)vma->vm_start, (void *)vma->vm_end };
	return sbpf_call_function(page_ops_bpf, &args[0], sizeof(void *) * 3);
#endif
}

int vma_op_set_prot(struct vm_area_struct *vma, unsigned long prot)
{
	void *args[4] = { (void *)PTE_SET_PROT, (void *)vma->vm_start, (void *)vma->vm_end, (void *)prot };

	return sbpf_call_function(page_ops_bpf, &args[0], sizeof(void *) * 4);
}

int vma_op_advise(struct vm_area_struct *vma, vaddr_t vaddr, size_t len, unsigned long advice)
{
	/* Cuation!!! vma is useless in the context of advise!*/
	ASSERT(PAGE_ALIGNED(len));
	ASSERT(PAGE_ALIGNED(vaddr));
	ASSERT(advice == MADV_DONTNEED); // Now we only support dontneed
#ifdef CONFIG_MEMORY_DEBUG
	void *args[4] = { (void *)PTE_UNMAP, (void *)vaddr, (void *)((uint64_t)vaddr + len), (void *)mm_get_active() };
	int ret = sbpf_call_function(page_ops_bpf, &args[0], sizeof(void *) * 4);
#else
	void *args[3] = { (void *)PTE_UNMAP, (void *)vaddr, (void *)((uint64_t)vaddr + len) };
	int ret = sbpf_call_function(page_ops_bpf, &args[0], sizeof(void *) * 3);
#endif

	return ret;
}

const struct vm_operation_struct vma_anon_ops = {
	.open = NULL,
	.close = NULL,
#ifdef CONFIG_USE_PLAT_MEMORY
	.mmap = vma_op_plat_mmap,
#else
	.mmap = vma_op_mmap,
#endif
#ifdef CONFIG_USE_PLAT_MEMORY
	.unmap = vma_op_plat_unmap, /* default */
#else
	.unmap = vma_op_unmap,
#endif
#ifdef CONFIG_USE_PLAT_MEMORY
	.mprotect = vma_op_plat_set_prot, /* default */
#else
	.mprotect = vma_op_set_prot,
#endif
	.madvise = vma_op_advise,
};