#include "kthread.h"
#include "kmalloc.h"
#include "lib/err.h"
#include "lib/list.h"
#include "memory.h"
#include "plat.h"
#include "debug.h"

static LIST_HEAD(kthread_list);

struct kthread *kthread_create(int (*threadfn)(void *data), void *data, char *fnname)
{
	void *stack = kmalloc(KTHREAD_STACK_SIZE);
	int flags = CLONE_FILES | CLONE_FS | CLONE_IO | CLONE_VM;
	struct kthread *kt;
	int pid;

	stack += KTHREAD_STACK_SIZE;
	stack = (void *)ALIGN_DOWN((unsigned long)stack, KTHREAD_STACK_ALIGN);

	kt = kmalloc(sizeof(struct kthread));
	kt->flags = flags;
	kt->full_name = fnname;
	kt->threadfn = threadfn;

	pid = plat_clone(threadfn, stack, flags, data);
	kt->pid = pid;

	if (pid < 0) {
		kfree(kt);
		kfree(stack);
		return ERR_PTR(pid);
	}

	list_add_tail(&kt->list, &kthread_list);

	return kt;
}

void exit_kthread(void)
{
	struct kthread *cur;

	list_for_each_entry(cur, &kthread_list, list) {
		plat_kill(cur->pid, 9);
	}
}
