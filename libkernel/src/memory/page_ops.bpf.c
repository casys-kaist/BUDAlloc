#include "errno.h"
#include "kconfig.h"
#include "lib/list.h"
#include "spin_lock.h"
#include "memory.h"
#include "vmem.h"
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

static long map_page(void *alias, void *canonical);
static int set_protection(void *args[]);
static int clear(void *args[]);
static int flush(void *args[]);

#if defined(DEBUG_PAGE_TABLE) || defined(CONFIG_DEBUG_ALLOC)
#define bpf_debug(...) bpf_printk(__VA_ARGS__)
#else
#define bpf_debug(...)
#endif

#define ASSERT_BPF(x)                                                                                                  \
	if (!(x)) {                                                                                                    \
		bpf_debug("assertion failed");                                                                        \
		return 1;                                                                                              \
	}

SEC("sbpf/function")
int page_ops(void *args[])
{
	void *op = args[0];
	switch ((unsigned long)op) {
	case PTE_MAP:
		return map_page(args[1], args[2]);
	case PTE_UNMAP:
		return clear(&args[1]);
	case PTE_SET_PROT:
		return set_protection(&args[1]);
#ifdef CONFIG_BATCHED_FREE
	case 3:
		return flush(&args[1]);
#endif
	default:
		return 1;
	}
}

static long map_page(void *alias, void *canonical)
{
	if (bpf_set_page_table((void *)PAGE_ALIGN_DOWN((uint64_t)alias), PAGE_SIZE,
			       (void *)PAGE_ALIGN_DOWN((uint64_t)canonical), 0, PAGE_SHARED_EXEC) != 0) {
		bpf_debug("bpf_set_page_table(0x%lx, 0x%lx, 0x%lx) failed with in map_page",
			   (void *)PAGE_ALIGN_DOWN((uint64_t)alias), PAGE_SIZE,
			   (void *)PAGE_ALIGN_DOWN((uint64_t)canonical));
		return 1;
	}
	// bpf_debug("map page 0x%lx to 0x%lx", (uint64_t)alias, (uint64_t)canonical);
	return 1;
}

static long set_page_prot(u32 index, unsigned long ctx[])
{
	unsigned long cur = ctx[0] + (index << PAGE_SHIFT);
	unsigned long prot = ctx[1];
	return 1;
	// bpf_set_page_table((void *)cur, 0, 0, prot, PTE_SET_PROT);
	// return 0;
}

static int set_protection(void *args[])
{
	unsigned long start_u = (unsigned long)args[0];
	unsigned long end_u = (unsigned long)args[1];
	unsigned long prot = (unsigned long)args[2];

	ASSERT_BPF(PAGE_ALIGNED(start_u));
	ASSERT_BPF(PAGE_ALIGNED(end_u));
	ASSERT_BPF(start_u < end_u);

	u32 n_pages = (end_u - start_u) >> PAGE_SHIFT;
	unsigned long ctx[] = { start_u, prot };

	long ret = bpf_loop(n_pages, set_page_prot, ctx, 0);
	ASSERT_BPF(ret != -EINVAL);
	ASSERT_BPF(ret != -E2BIG);
	ASSERT_BPF(ret == n_pages);

	return 0;
}

static int clear(void *args[])
{
	unsigned long start_u = (unsigned long)args[0];
	unsigned long end_u = (unsigned long)args[1];

	ASSERT_BPF(PAGE_ALIGNED(start_u));
	ASSERT_BPF(PAGE_ALIGNED(end_u));
	ASSERT_BPF(start_u < end_u);

#if defined CONFIG_DEBUG_ALLOC || defined DEBUG_PAGE_TABLE
	bpf_debug("demand free with prev alias 0x%lx with len : 0x%lx", start_u, end_u - start_u);
#endif
	if (bpf_unset_page_table((void *)start_u, end_u - start_u) != 0) {
		bpf_debug("bpf_unset_page_table(0x%lx, 0x%lx) failed in clear", (void *)start_u, end_u - start_u);
		return 1; // madvise
	}
#ifdef CONFIG_MEMORY_DEBUG
	struct mm_struct *mm = (struct mm_struct *)bpf_uaddr_to_kaddr(args[2], sizeof(struct mm_struct));
	if (mm == NULL) {
		return 1;
	}

	struct page_fault_stats *stats = &mm->stats;
	if (stats == NULL) {
		return 1;
	}
#endif
	/* Temporary, should be fixed */
	DEBUG_DEC_MM_VAL(&stats->current_alloc, 1);
	return 0;
}

#ifdef CONFIG_BATCHED_FREE
static int flush(void *args[])
{
	// lazy free
	struct mm_struct *mm = (struct mm_struct *)bpf_uaddr_to_kaddr((void *)args[0], sizeof(struct mm_struct));
	if (mm == NULL) {
		bpf_debug("page_ops: mm is null in flush");
		return 1;
	}
	int per_thread_index = (unsigned long)args[1];
	if (per_thread_index >= MAX_PER_THREAD_NUM || per_thread_index < 0) {
		bpf_debug("page_ops: per_thread_index is out of range");
		return 1;
	}
	struct mm_per_thread *mpt = (struct mm_per_thread *)(&mm->perthread[per_thread_index]);
	typedef struct contiguous_alias(*ptr_arr)[FREE_ALIAS_BUF_SIZE];

	ptr_arr buf = (ptr_arr)bpf_uaddr_to_kaddr(mpt->free_alias_buf, PAGE_SIZE);
	if (buf == NULL) {
		bpf_debug("page_ops: No buf!");
		return 1;
	}
	unsigned int buf_index = mpt->buf_index;
	uint64_t prev_alias_start;
	uint64_t prev_len;
#ifdef CONFIG_MEMORY_DEBUG
	struct page_fault_stats *stats = &mm->stats;
	if (stats == NULL) {
		return 1;
	}
#endif
	for (unsigned int i = 0; i < FREE_ALIAS_BUF_SIZE; i++) {
		prev_alias_start = (*buf)[i].start;
		prev_len = (*buf)[i].len;
		(*buf)[i].start = (*buf)[i].len = 0;

#if defined CONFIG_DEBUG_ALLOC || defined DEBUG_PAGE_TABLE
		if (prev_alias_start == 0) {
			bpf_debug("page_ops: free 0 alias");
			return 1;
		}
#endif
#if defined CONFIG_DEBUG_ALLOC || defined DEBUG_PAGE_TABLE
		/** bpf_debug("lazy free with prev alias 0x%lx with len : 0x%lx", prev_alias_start, prev_len); */
#endif
		if (bpf_unset_page_table((void *)prev_alias_start, prev_len) != 0) {
			bpf_debug("bpf_unset_page_table(0x%lx, 0x%lx) failed in flush", prev_alias_start, prev_len);
			return 1;
		}
		/* Temporary, should be fixed */
		DEBUG_DEC_MM_VAL(&stats->current_alloc, 1);
		if (i == buf_index)
			break;
	}
	mpt->buf_index = 0;
	mpt->buf_cnt = 0;
	mpt->buf_empty = true;
	mpt->buf_version = (mpt->buf_version + 1) & (N_VERSIONS - 1); // 0 ~ 7
	return 0;
}
#endif
char _license[] SEC("license") = "GPL";
