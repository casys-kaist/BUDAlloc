#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>

// Function pointer typedefs for original malloc and free functions
typedef void* (*malloc_t)(size_t);
typedef void (*free_t)(void*);

// Pointers to the original malloc and free functions
malloc_t original_malloc = NULL;
free_t original_free = NULL;

// Counter variables for malloc and free calls
static unsigned long malloc_count = 0;
static unsigned long free_count = 0;

static void init_mm();
static int (*main_orig)(int, char **, char **);

static int dup_stderr;

void cleanup(void);

int __attribute__((visibility("default")))
__libc_start_main(int (*main)(int, char **, char **), int argc, char **argv, int (*init)(int, char **, char **),
          void (*fini)(void), void (*rtld_fini)(void), void *stack_end)
{
    /* Save the real main function address */
    main_orig = main;

    /* Find the real __libc_start_main()... */
    typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");
	atexit(cleanup);

	return orig(main_orig, argc, argv, init, fini, rtld_fini, stack_end);
}

// Interposed malloc function
void* malloc(size_t size) {
    // Load the original malloc function if not already loaded
    if (!original_malloc) {
        original_malloc = dlsym(RTLD_NEXT, "malloc");
    }
    
    // Increment malloc counter
    malloc_count++;
    
    // Call the original malloc function
    return original_malloc(size);
}

// Interposed free function
void free(void* ptr) {
    // Load the original free function if not already loaded
    if (!original_free) {
        original_free = dlsym(RTLD_NEXT, "free");
    }
    
    // Increment free counter
    free_count++;
    
    // Call the original free function
    original_free(ptr);
}

// Cleanup function
void cleanup() {
    // Output malloc and free counts
	printf("statistics:\n");
    printf("malloc calls: %lu\n", malloc_count);
    printf("free calls: %lu\n", free_count);
}


