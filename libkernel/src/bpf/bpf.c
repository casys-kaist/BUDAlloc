#include "sbpf/bpf.h"
#include "debug.h"
#include "kmalloc.h"
#include "lib/errno.h"
#include "lib/list.h"
#include "lib/stddef.h"
#include "lib/string.h"
#include "lib/types.h"
#include "stdarg.h"

void debug_bpf(struct sbpf *bpf)
{
	struct sbpf_map *cur;
	printk("debug bpf, fd: %d\n", bpf->bpf_fd);
	list_for_each_entry (cur, &bpf->maps, list) {
		printk("map fd: %d, name: %s\n", cur->map_fd, cur->name);
	}
}

struct sbpf *sbpf_launch_program(char *prog_path, char *prog_name, void *aux_ptr, size_t aux_len, int map_cnt, ...)
{
	struct bpf_object *bpf_obj = NULL;
	struct bpf_program *bpf_prog;
	struct bpf_map *bpf_map;
	va_list maps;
	char *cursor_map_name;
	struct sbpf *bpf = kmalloc(sizeof(struct sbpf));
	INIT_LIST_HEAD(&bpf->maps);

	// BPF object file is parsed.
	bpf_obj = bpf_object__open(prog_path);
	if (bpf_obj == NULL) {
		PANIC("Opening eBPF program failed\n");
	}

	// BPF maps are created, various relocations are resolved and BPF programs are
	// loaded into the kernel and verfied. But BPF program is yet executed.
	if (bpf_object__load(bpf_obj)) {
		PANIC("Loading eBPF program failed\n");
	}

	va_start(maps, map_cnt);
	for (int i = 0; i < map_cnt; i++) {
		struct sbpf_map *map = kmalloc(sizeof(struct sbpf_map));
		cursor_map_name = va_arg(maps, char *);

		bpf_map = bpf_object__find_map_by_name(bpf_obj, cursor_map_name);
		if (bpf_map == NULL) {
			PANIC("Loading eBPF map failed\n");
		}

		map->name = cursor_map_name;
		map->map_fd = bpf_map__fd(bpf_map);
		list_add_tail(&map->list, &bpf->maps);
	}
	va_end(maps);

	// Attachment phase, This is the phase for BPF program attached to various BPF
	// hook such as tracepoints, kprobes, cgroup hooks and network packet pipeline
	// etc.

	bpf_prog = bpf_object__find_program_by_name(bpf_obj, prog_name);
	if (aux_ptr != NULL) {
		if (bpf_program__set_aux(bpf_prog, aux_ptr, aux_len))
			PANIC("Loading aux ptr failed\n");
	}

	if (bpf_program__attach(bpf_prog) == NULL) {
		PANIC("Attaching eBPF link failed\n");
	}
	bpf->bpf_fd = bpf_program__fd(bpf_prog);

	return bpf;
}

int sbpf_call_function(struct sbpf *bpf, void *arg_ptr, size_t arg_len)
{
	ASSERT(bpf);
	
	return bpf_sbpf_call_function(bpf->bpf_fd, arg_ptr, arg_len);
}

static int get_map_fd(struct sbpf *bpf, char *map_name)
{
	struct sbpf_map *cur;
	list_for_each_entry (cur, &bpf->maps, list) {
		if (!strcmp(cur->name, map_name)) {
			return cur->map_fd;
		}
	}

	return -EINVAL;
}

long sbpf_map_lookup_elem(struct sbpf *bpf, char *map_name, long key)
{
	int map_fd;
	long value;

	ASSERT(bpf);
	ASSERT(map_name);

	map_fd = get_map_fd(bpf, map_name);
	if (map_fd < 0) {
		return -EINVAL;
	}

	if (bpf_map_lookup_elem(map_fd, &key, &value)) {
		return -EINVAL;
	}

	return value;
}

int sbpf_map_update_elem(struct sbpf *bpf, char *map_name, long key, long value)
{
	int map_fd;

	ASSERT(bpf);
	ASSERT(map_name);

	map_fd = get_map_fd(bpf, map_name);
	if (map_fd < 0) {
		return -EINVAL;
	}

	bpf_map_update_elem(map_fd, &key, &value, 0);

	return 0;
}
