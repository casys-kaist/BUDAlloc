#include "boot.h"
#include "debug.h"
#include "kmalloc.h"
#include "kthread.h"
#include "memory.h"
#include "vmem.h"

int kernel_entry(void)
{
	memory_init();
	atexit(kernel_exit);
	return 0;
}

void kernel_exit(void)
{
#ifdef CONFIG_MEMORY_DEBUG
	ota_malloc_stats();
#endif
	exit_kthread();
}