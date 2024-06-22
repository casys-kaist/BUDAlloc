#include "memory.h"
#include "debug.h"
#include "kmalloc.h"
#include "kthread.h"
#include "lib/err.h"
#include "ota.h"
#include "sbpf/bpf.h"
#include "vmem.h"
#include "plat.h"

int memory_init()
{
	struct mm_struct *mm =  plat_mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	struct sbpf *bpf;

	mm_init(mm);
	ota_init();
#ifndef CONFIG_USE_PLAT_MEMORY
	bpf = sbpf_launch_program("/lib/sbpf/page_fault.bpf.o", "page_fault", mm, sizeof(struct mm_struct), 0);
	page_ops_bpf = sbpf_launch_program("/lib/sbpf/page_ops.bpf.o", "page_ops", mm, sizeof(struct mm_struct), 0);
#endif

	return 0;
}
