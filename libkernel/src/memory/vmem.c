#include "vmem.h"
#include "debug.h"
#include "errno.h"
#include "kmalloc.h"
#include "kconfig.h"
#include "lib/compiler.h"
#include "lib/list.h"
#include "lib/minmax.h"
#include "lib/string.h"
#include "spin_lock.h"
#include "rwlock.h"
#include "lib/trie.h"
#include "memory.h"
#include "plat.h"
#include <pthread.h>
/*
 * Pointer to currently active virtual address space.
 * TODO: This should move to a CPU-local variable
 */
static struct mm_struct *vmem_active_mm = NULL;

/* Forward declarations */
static void vmem_vma_unmap(struct vm_area_struct *vma, vaddr_t vaddr, size_t len);
static void vmem_vma_unlink_and_free(struct vm_area_struct *vma);

void debug_mm_struct(struct mm_struct *mm)
{
	struct vm_area_struct *cur;
	char prefix[5];
	prefix[4] = '\0';

	printk("debug maps:\n");
	list_for_each_entry (cur, &mm->vma_list, list) {
		prefix[0] = cur->vm_page_prot & PROT_READ ? 'r' : '-';
		prefix[1] = cur->vm_page_prot & PROT_WRITE ? 'w' : '-';
		prefix[2] = cur->vm_page_prot & PROT_EXEC ? 'x' : '-';
		prefix[3] = cur->vm_flags & MAP_SHARED ? 's' : 'p';
		printk("%012lx-%012lx %s %08lx %12s\n", cur->vm_start, cur->vm_end, prefix, cur->vm_flags,
		       cur->name == NULL ? "ANON" : cur->name);
	}
}

inline struct mm_struct *mm_get_active(void)
{
	return vmem_active_mm;
}

int vas_set_active(struct mm_struct *mm)
{
	vmem_active_mm = mm;

	return 0;
}

void prepare(void)
{
	rwlock_write_lock(&mm_get_active()->fork_lock);
}

void parent(void)
{
	rwlock_write_unlock(&mm_get_active()->fork_lock); // unlock write lock and proceed with other threads
}

void child(void)
{
	rwlock_init(&mm_get_active()->fork_lock); // reset rwlock since other thread may lock rwl->r during fork
}

int mm_init(struct mm_struct *mm)
{
	ASSERT(mm != NULL);

	mm->vma_base = PAGE_ALIGN(VADDR_BASE);
	mm->flags = 0;
	INIT_LIST_HEAD(&mm->vma_list);
	// trie_init(&mm->vma_trie); vma_trie do not include ref_cnt maybe it is TODO?
	mm->vma_trie = plat_mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
#ifdef CONFIG_TRIE_CACHE
	for (int i = 0; i < 4; i++)
		mm->trie_cached_nodes[i] = CACHE_INVAL;
#endif
#ifdef CONFIG_SUPPORT_FORK_SAFETY
	rwlock_init(&mm->fork_lock);
	pthread_atfork(prepare, parent, child);
#endif
#ifdef CONFIG_BATCHED_FREE
	spin_lock_init(&mm->mm_lock);
	mm->cur_flush_idx = 0;
#endif
	vas_set_active(mm);

	return 0;
}

void mm_destory(struct mm_struct *mm)
{
	struct vm_area_struct *vma, *next;

	list_for_each_entry_safe (vma, next, &mm->vma_list, list) {
		vmem_vma_unmap(vma, vma->vm_start, vmem_vma_len(vma));
		vmem_vma_unlink_and_free(vma);
	}

	ASSERT(list_empty(&mm->vma_list));

	if (vmem_active_mm == mm) {
		vmem_active_mm = NULL;
	}
}

static void vmem_vma_destroy(struct vm_area_struct *vma)
{
	ASSERT(vma);
	kfree(vma);
}

static void vmem_vma_unlink_and_free(struct vm_area_struct *vma)
{
	ASSERT(vma);
	ASSERT(!list_empty(&vma->list));

	list_del(&vma->list);
	vmem_vma_destroy(vma);
}

static struct vm_area_struct *vmem_find_vma(struct mm_struct *vas, vaddr_t vaddr, size_t len)
{
	struct vm_area_struct *vma;
	vaddr_t vstart = vaddr;
	vaddr_t vend = vaddr + max(len, (size_t)1);

	ASSERT(vas);

	list_for_each_entry (vma, &vas->vma_list, list) {
		if ((vend > vma->vm_start) && (vstart < vma->vm_end))
			return vma;
	}

	return NULL;
}

const struct vm_area_struct *vma_find(struct mm_struct *vas, vaddr_t vaddr)
{
	return vmem_find_vma(vas, vaddr, 0);
}

static int vmem_vma_find_range(struct mm_struct *vas, vaddr_t *vaddr, size_t *len, struct vm_area_struct **start,
			       struct vm_area_struct **end)
{
	struct vm_area_struct *vma_start, *vma_end, *next;
	vaddr_t vstart, vend;
	size_t vl;
	int algn_lvl;

	ASSERT(vas);
	ASSERT(start);
	ASSERT(end);

	ASSERT(vaddr);
	vstart = *vaddr;

	ASSERT(len);
	vl = *len;

	ASSERT(vstart <= VADDR_MAX - vl);
	vend = vstart + vl;

	if (!*start) {
		vma_start = vmem_find_vma(vas, vstart, vl);
		if (unlikely(!vma_start))
			return -ENOENT;
	} else {
		vma_start = *start;
	}

	/* Adjust vstart to make sure it is within the VMA. We expect
	 * vstart to be properly aligned by the caller.
	 */
	ASSERT(vstart < vma_start->vm_end);
	if (vstart < vma_start->vm_start) {
		vstart = vma_start->vm_start;
	} else if (vstart > vma_start->vm_start) {
		if (!PAGE_ALIGNED(vstart))
			return -EINVAL;
	}

	/* Find the end VMA */
	vma_end = vma_start;
	if (likely(vl > 0)) {
		while (vma_end->list.next != &vas->vma_list) {
			if (vend > vma_end->vm_start && vend <= vma_end->vm_end)
				break;

			next = list_next_entry(vma_end, list);
			if (vend <= next->vm_start)
				break;
			/* Above condition is for the case where
			** req :     [.***....]
			** vma map : [AAA.BB.C]
			*/
			vma_end = next;
		}

		/* Adjust vend to make sure it is within the VMA. We expect
		 * vend to be properly aligned by the caller (via len).
		 */
		if (vend > vma_end->vm_end) {
			vend = vma_end->vm_end;
		} else if (vend < vma_end->vm_end) {
			if (!PAGE_ALIGNED(vend))
				return -EINVAL;
		}
	}

	*vaddr = vstart;
	*len = vend - vstart;
	*start = vma_start;
	*end = vma_end;

	return 0;
}

static void vmem_insert_vma(struct mm_struct *vas, struct vm_area_struct *vma)
{
	struct list_head *prev;
	struct vm_area_struct *cur;

	ASSERT(vas);
	ASSERT(list_empty(&vma->list));
	ASSERT(!vmem_find_vma(vas, vma->vm_start, vma->vm_end - vma->vm_start));

	prev = &vas->vma_list;
	list_for_each_entry (cur, &vas->vma_list, list) {
		if (vma->vm_start < cur->vm_end) {
			ASSERT(vma->vm_end <= cur->vm_start);

			list_add(&vma->list, prev);
			return;
		}

		prev = &cur->list;
	}

	list_add_tail(&vma->list, &vas->vma_list);
}

static inline int vmem_vma_can_merge(struct vm_area_struct *vma, struct vm_area_struct *next)
{
	ASSERT(vma);
	ASSERT(next);
	ASSERT(vma->vm_mm == next->vm_mm);

	return ((vma->vm_end == next->vm_start) && (vma->vm_flags == next->vm_flags) &&
		(vma->vm_page_prot == next->vm_page_prot) &&
		((vma->name == next->name) || (vma->name && next->name && !strcmp(vma->name, next->name))));
}

static int vmem_vma_do_try_merge_with_next(struct vm_area_struct *vma)
{
	struct vm_area_struct *next;

	ASSERT(vma);

	if (vma->list.next == &vma->vm_mm->vma_list)
		return -ENOENT;

	next = list_next_entry(vma, list);

	if (!vmem_vma_can_merge(vma, next))
		return -EPERM;

	vma->vm_end = next->vm_end;

	vmem_vma_unlink_and_free(next);
	return 0;
}

static struct vm_area_struct *vmem_vma_try_merge_with_next(struct vm_area_struct *vma)
{
	vmem_vma_do_try_merge_with_next(vma);
	return vma;
}

static struct vm_area_struct *vmem_vma_try_merge_with_prev(struct vm_area_struct *vma)
{
	struct vm_area_struct *prev;

	if (vma->list.prev == &vma->vm_mm->vma_list)
		return vma;

	prev = list_prev_entry(vma, list);
	return (vmem_vma_do_try_merge_with_next(prev) == 0) ? prev : vma;
}

static struct vm_area_struct *vmem_vma_try_merge(struct vm_area_struct *vma)
{
	vmem_vma_try_merge_with_next(vma);
	return vmem_vma_try_merge_with_prev(vma);
}

static int vmem_vma_split(struct vm_area_struct *vma, vaddr_t vaddr, struct vm_area_struct **new_vma)
{
	struct vm_area_struct *v = NULL;

	ASSERT(vma);
	ASSERT(new_vma);

	ASSERT(vaddr >= vma->vm_start && vaddr < vma->vm_end);

	if (!v) {
		v = kmalloc(sizeof(struct vm_area_struct));
		if (unlikely(!v))
			return -ENOMEM;
	}

	ASSERT(v);
	INIT_LIST_HEAD(&v->list);
	v->vm_start = vaddr;
	v->vm_end = vma->vm_end;
	v->vm_mm = vma->vm_mm;
	v->vm_ops = vma->vm_ops;
	v->vm_page_prot = vma->vm_page_prot;
	v->vm_flags = vma->vm_flags;
	v->name = vma->name;

	vma->vm_end = vaddr;

	list_add(&v->list, &vma->list);

	*new_vma = v;
	return 0;
}

static inline int vmem_vma_need_split(struct vm_area_struct *vma, size_t len, const unsigned long *prot)
{
	ASSERT(vma);

	return (len > 0) && (!prot || (vma->vm_page_prot != *prot));
}

static int vmem_split_vma(struct mm_struct *mm, vaddr_t vaddr, size_t len, unsigned long *prot,
			  struct vm_area_struct **start, struct vm_area_struct **end)
{
	struct vm_area_struct *vma_start = NULL, *vma_end, *vma;
	vaddr_t vend;
	size_t left, right;
	int ret;

	ASSERT(mm);
	ASSERT(start);
	ASSERT(end);

	/*
	 * We can have the following cases:
	 *
	 * AAAAAAAAA        AAAAAAAAA         AAAAAAAAA         AAAAAAAAA
	 * ^-------^        ^---^                 ^---^           ^---^
	 *
	 * AAA C BBB        AAA C BBB         AAA C BBB         AAA C BBB
	 * ^-------^         ^------^         ^------^           ^-----^
	 */
	ret = vmem_vma_find_range(mm, &vaddr, &len, &vma_start, &vma_end);
	if (unlikely(ret))
		return ret;

	vend = vaddr + len;
	left = vaddr - vma_start->vm_start;
	right = vma_end->vm_end - vend;

	if (vmem_vma_need_split(vma_start, left, prot)) {
		ret = vmem_vma_split(vma_start, vaddr, &vma);
		if (unlikely(ret))
			return ret;

		ASSERT(vma);

		if (vma_start == vma_end)
			vma_end = vma;

		vma_start = vma;
	}

	if (likely(len > 0)) {
		if (vmem_vma_need_split(vma_end, right, prot)) {
			// vend --- vm-a_end
			ret = vmem_vma_split(vma_end, vend, &vma);
			if (unlikely(ret)) {
				vmem_vma_try_merge(vma_start);
				return ret;
			}

			ASSERT(vma);
		}
	} else {
		ASSERT(vma_start == vma_end);
	}

	*start = vma_start;
	*end = vma_end;

	return 0;
}

int vma_op_deny()
{
	return -EPERM;
}

int vma_op_plat_unmap(struct vm_area_struct *vma, vaddr_t vaddr, size_t len)
{
	ASSERT(vaddr >= vma->vm_start);
	ASSERT(vaddr + len <= vma->vm_end);
	ASSERT(PAGE_ALIGNED(len));

	return plat_munmap((void *)vaddr, len); // later it needs to be munmapped internally
}

int vma_op_plat_mmap(struct vm_area_struct *vma)
{
	ASSERT(vma != NULL);

	plat_mmap((void *)vma->vm_start, vma->vm_end - vma->vm_start, vma->vm_page_prot, vma->vm_flags, -1, 0);

	return 0;
}

static void vmem_vma_unmap(struct vm_area_struct *vma, vaddr_t vaddr, size_t len)
{
	int ret;

	ASSERT(vma->vm_ops->unmap != NULL);
	ret = vma->vm_ops->unmap(vma, vaddr, len);

	if (unlikely(ret)) {
		PANIC("Failed to unmap address range");
	}
}

static void vmem_vma_unmap_and_free(struct vm_area_struct *vma)
{
	vmem_vma_unmap(vma, vma->vm_start, vmem_vma_len(vma));
	vmem_vma_destroy(vma);
}

static void vmem_unmap_and_free_vma(struct vm_area_struct *start, struct vm_area_struct *end)
{
	struct vm_area_struct *vma = start, *next;

	ASSERT(start);
	ASSERT(end);

	/* It is safe now to unmap and free the VMAs */
	while (vma != end) {
		next = list_next_entry(vma, list);
		vmem_vma_unmap_and_free(vma);

		vma = next;
	}

	vmem_vma_unmap_and_free(end);
}

int vma_unmap(struct mm_struct *mm, vaddr_t vaddr, size_t len)
{
	struct vm_area_struct *vma_start = NULL, *vma_end;
	int ret;

	if (unlikely(len == 0))
		return 0;

	ret = vmem_split_vma(mm, vaddr, len, NULL, &vma_start, &vma_end);
	if (unlikely(ret)) {
		if (ret == -ENOENT)
			return 0;

		return ret;
	}

	/* Unlink all VMAs starting from vma_start to vma_end */
	vma_start->list.prev->next = vma_end->list.next;
	vma_end->list.next->prev = vma_start->list.prev;

	vmem_unmap_and_free_vma(vma_start, vma_end);

	return 0;
}

static vaddr_t vmem_first_fit(struct mm_struct *mm, vaddr_t base, size_t align, size_t len)
{
	vaddr_t vaddr = base;
	struct vm_area_struct *cur;

	ASSERT(mm);

	/* Since we are scanning the VAS for an empty address range, we need
	 * to be careful not to overflow. Checks are thus always active and not
	 * just asserts.
	 */
	list_for_each_entry (cur, &mm->vma_list, list) {
		if (unlikely(vaddr > SIZE_MAX - align))
			return VADDR_INV;

		vaddr = ALIGN(vaddr, align);

		if (unlikely(vaddr > SIZE_MAX - len))
			return VADDR_INV;

		if (vaddr + len <= cur->vm_start)
			return vaddr;

		vaddr = max(cur->vm_end, base);
	}

	if (unlikely(vaddr > SIZE_MAX - align))
		return VADDR_INV;

	return ALIGN(vaddr, align);
}

int vma_map(struct mm_struct *mm, vaddr_t *vaddr, size_t len, unsigned long prot, unsigned long flags, const char *name,
	    void *aux)
{
	struct vm_area_struct *vma_start = NULL;
	struct vm_area_struct *vma_end = NULL;
	struct vm_area_struct *vma = NULL;
	vaddr_t va;
	int ret;

	ASSERT(mm);
	ASSERT(vaddr);
	ASSERT(len > 0);

	if (unlikely(!PAGE_ALIGNED(len)))
		return -EINVAL;

	va = *vaddr;
	if (va == 0) {
		va = vmem_first_fit(mm, VADDR_BASE, PAGE_SIZE, len);
		if (unlikely(va == VADDR_INV))
			return -ENOMEM;
	} else {
		if (unlikely(!PAGE_ALIGN(va)))
			return -EINVAL;

		if ((void *)*vaddr < (void *)VADDR_BASE) {
			// Check *vaddr (as a alignment hint) is power of 2.
			ASSERT((*vaddr & (*vaddr - 1)) == 0);
			va = (*vaddr + VADDR_BASE - 1) & ~(*vaddr - 1);
		}

		/* The caller specified an exact address to map to. We only
		 * allow this if the mapping does not collide with other
		 * mappings. However, we enforce the new mapping if the replace
		 * flag is provided by unmapping everything in the range first.
		 */
	retry:
		if ((vma_start = vmem_find_vma(mm, va, len))) {
			//			if (unlikely(!(flags & UK_VMA_MAP_REPLACE)))
			//			Now, just ret -EEXIST
			if (*vaddr) {
				va = (*vaddr + vma_start->vm_end - 1) & ~(*vaddr - 1);
				goto retry;
			} else {
				return -EEXIST;
			}

			ret = vmem_split_vma(mm, va, len, NULL, &vma_start, &vma_end);
			if (unlikely(ret))
				return ret;

			/* It could be that during the split operation we
			 * recognized that len must be aligned to a larger
			 * page size to allow the split.
			 */
			ASSERT(vma_end->vm_end > vma_start->vm_start);
			len = vma_end->vm_end - vma_start->vm_start;
		}
	}

	ASSERT(PAGE_ALIGNED(va));
	ASSERT(va <= VADDR_MAX - len);

	/* Create a new VMA for the requested range. */
	vma = kmalloc(sizeof(struct vm_area_struct));
	if (unlikely(!vma))
		return -ENOMEM;

	vma->name = NULL;
	if (flags & MAP_ANONYMOUS) {
		vma->vm_ops = &vma_anon_ops;
	}

	ASSERT(vma);

	/* If this mapping replaces existing VMAs we now have to unmap them
	 * before we attempt to map the new VMA because this entail changes to
	 * the page table.
	 */
	if (vma_start) {
		ASSERT(vma_end);

		/* Unlink all VMAs starting from vma_start to vma_end */
		vma_start->list.prev->next = vma_end->list.next;
		vma_end->list.next->prev = vma_start->list.prev;

		vmem_unmap_and_free_vma(vma_start, vma_end);
	}

	ASSERT(vmem_find_vma(mm, va, len) == NULL);

	INIT_LIST_HEAD(&vma->list);
	vma->vm_start = va;
	vma->vm_end = va + len;
	vma->vm_mm = mm;
	vma->vm_ops = vma->vm_ops;
	vma->vm_page_prot = prot;
	vma->vm_flags = flags | MAP_FIXED;
	vma->aux = aux;
	if (name)
		vma->name = name;

	if (flags & MAP_POPULATE) {
		TODO();
	}

	vmem_insert_vma(mm, vma);
	vma->vm_ops->mmap(vma);
	vmem_vma_try_merge(vma);

	*vaddr = va;
	// printk("va = %lx, len = %lx, prot = %lx, flags = %lx, name = %s\n", va, len, prot, flags, name);

	return 0;
}

int vma_op_plat_set_prot(struct vm_area_struct *vma, unsigned long prot)
{
	return plat_mprotect((void *)vma->vm_start, vmem_vma_len(vma), prot);
}

static void vmem_vma_set_prot(struct vm_area_struct *vma, unsigned long prot)
{
	int rc;

	ASSERT(vma);

	if (vma->vm_page_prot == prot)
		return;

	rc = vma->vm_ops->mprotect(vma, prot);
	//	rc = vma_op_set_prot(vma, prot);

	// for file, dma, etc,....
	//	rc = VMA_SETATTR(vma, attr);
	if (unlikely(rc)) {
		/* We consider a failure to set attributes in an address range
 		 * as fatal to simplify error handling. In contrast to unmap(),
 		 * this can be changed if necessary.
 		 */
		PANIC("Failed to set attributes ");
	}

	vma->vm_page_prot = prot;
}

static void vmem_set_prot_vma(struct vm_area_struct *start, struct vm_area_struct *end, unsigned long prot)
{
	struct vm_area_struct *vma = start;

	ASSERT(start);
	ASSERT(end);

	while (vma != end) {
		vmem_vma_set_prot(vma, prot);
		vma = list_next_entry(vma, list);
	}

	vmem_vma_set_prot(end, prot);

	/* Do a second pass and try to merge VMAs */
	vma = vmem_vma_try_merge_with_next(end);
	ASSERT(vma == end);

	vma = start;
	while (vma != end) {
		vma = vmem_vma_try_merge_with_prev(vma);
		vma = list_next_entry(vma, list);
	}

	vmem_vma_try_merge_with_prev(end);
}

int vma_set_prot(struct mm_struct *mm, vaddr_t vaddr, size_t len, unsigned long prot)
{
	struct vm_area_struct *vma_start = NULL, *vma_end;
	int rc;

	if (unlikely(len == 0))
		return 0;

	rc = vmem_split_vma(mm, vaddr, len, &prot, &vma_start, &vma_end);
	if (unlikely(rc)) {
		if (rc == -ENOENT)
			return 0;

		return rc;
	}

	vmem_set_prot_vma(vma_start, vma_end, prot);

	return 0;
}

static int vmem_vma_advise(struct vm_area_struct *vma, vaddr_t vaddr, size_t len, unsigned long advice)
{
	int ret;
	ASSERT(vma->vm_ops->madvise != NULL);
	ret = vma->vm_ops->madvise(vma, vaddr, len, advice);
	if (unlikely(ret)) {
		PANIC("Failed to madvise address range");
	}
	return ret;
}

int vma_advise(struct mm_struct *mm, vaddr_t vaddr, size_t len, unsigned long advice)
{
	struct vm_area_struct *vma_start = NULL, *vma_end, *vma;
	vaddr_t vend;
	int ret;
	if (unlikely(len == 0))
		return 0;

	ret = vmem_vma_find_range(mm, &vaddr, &len, &vma_start, &vma_end);
	if (unlikely(ret)) {
		if (ret == -ENOENT)
			return 0;

		return ret;
	}

	vend = vaddr + len;
	vma = vma_start;
	while (vma != vma_end) {
		ret = vmem_vma_advise(vma, vaddr, vma->vm_end - vaddr, advice);
		if (unlikely(ret))
			return ret;

		vma = list_next_entry(vma, list);
		vaddr = vma->vm_start;
	}

	return vmem_vma_advise(vma, vaddr, vend - vaddr, advice);
}
