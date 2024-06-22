/* Debug Settings */
#define CONFIG_NDEBUG 1 /* disable all debug options including mimalloc and ota */
// #define CONFIG_USE_PLAT_MEMORY 1 /* use platform (linux) mmap and unmap */
// #define CONFIG_DEBUG_ALLOC 1 /* print each alloc/free operations */
// #define CONFIG_DEBUG_LOCK 1 /* print each lock/unlock operations */
// #define CONFIG_DEBUG_MIMALLOC 1 /* debug mimalloc */
// #define CONFIG_TRACK_PREFETCH_HIT 1 /* track number of prefetch hits */
// #define CONFIG_MEMORY_DEBUG 1 /* print memory usage statistics */

/* One Time Allocator - Alias Settings */
#define CONFIG_ALIAS_WINDOW_SIZE_MAX 256 /* alias window max length for large page (size > 0x1000) */
#define CONFIG_ALIAS_WINDOW_SIZE_MIN 1 /* alias window min length for large page (size > 0x1000)*/

#define CONFIG_BATCHED_FREE 1 /* enable deferred free (disable immediate detection of UAF bugs) */
#define CONFIG_ADOPTIVE_BATCHED_FREE 1 /* adoptive batched free (should enable batched free, default 1) */
#define CONFIG_BATCHED_FREE_MAX_MATCH 3 /* freearray traversing maximum */
#define CONFIG_BATCHED_FREE_MAX_PAGES 1024 /* number of maximum batched pages */

/* One Time Allocator - Canonical Settings */
#define CONFIG_CANONICAL_HASH_SORT 1 /* enable hash sort for the canonial address to reduce the fragementations */ 
#define CONFIG_CANONICAL_HASH_SORT_BUCKET_SIZE 6 /* bucket size for the hashtable */
#define CONFIG_CANONICAL_HASH_SORT_DELAYED_SIZE 512 /* number of deferred canonical address */

#define CONFIG_TRIE_CACHE 1 /* use trie cache optimization (default 1) */
#define CONFIG_ALIAS_CACHE 2 /* use alias_cache optimization to prevent lock contention (default 2)*/
#define CONFIG_MIN_REMAIN_ALIASES 4 /* consumes last aliases to reduce memory overhead */

/* One Time Allocator - Multithreading */
// #define CONFIG_SUPPORT_FORK_SAFETY 1 /* mitgation of deadlock during fork (default 1) */
