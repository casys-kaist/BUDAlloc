#pragma once

#include "lib/list.h"
#include "lib/types.h"

struct sbpf_map {
	char *name;
	int map_fd;
	struct list_head list;
};

struct sbpf {
	int bpf_fd;
	struct list_head maps;
};

// Forward declaration of the libbpf
struct bpf_object;
struct bpf_program;
struct bpf_map;
struct bpf_link;

// Libbpf APIs
struct bpf_object *bpf_object__open(const char *path);
int bpf_object__load(struct bpf_object *obj);
struct bpf_map *bpf_object__find_map_by_name(const struct bpf_object *obj, const char *name);
int bpf_map__fd(const struct bpf_map *map);
struct bpf_program *bpf_object__find_program_by_name(const struct bpf_object *obj, const char *name);
int bpf_program__set_aux(struct bpf_program *prog, void *aux_ptr, size_t len);
struct bpf_link *bpf_program__attach(const struct bpf_program *prog);
int bpf_program__fd(const struct bpf_program *prog);
int bpf_map_lookup_elem(int fd, const void *key, void *value);
int bpf_map_update_elem(int fd, const void *key, const void *value, __u64 flags);
int bpf_sbpf_call_function(int fd, void *arg_ptr, size_t arg_len);

// sBPF APIs
struct sbpf *sbpf_launch_program(char *path, char *prog_name, void *aux_ptr, size_t aux_len, int map_cnt, ...);
int sbpf_call_function(struct sbpf *bpf, void *arg_ptr, size_t arg_len);
long sbpf_map_lookup_elem(struct sbpf *bpf, char *map_name, long key);
int sbpf_map_update_elem(struct sbpf *bpf, char *map_name, long key, long value);
void debug_bpf(struct sbpf *bpf);
