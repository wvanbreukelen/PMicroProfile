#include "ledger.h"

struct lfq_ctx staging_mmap_queue_ctx;
struct lfq_ctx staging_over_mmap_queue_ctx;


volatile int async_close_enable;

pthread_spinlock_t staging_mmap_lock;
pthread_spinlock_t staging_over_mmap_lock;


int MMAP_PAGE_SIZE;
int MMAP_HUGEPAGE_SIZE;
void* _nvp_zbuf; // holds all zeroes.  used for aligned file extending. TODO: does sharing this hurt performance?
pthread_spinlock_t	node_lookup_lock[NUM_NODE_LISTS];
struct NVFile* _nvp_fd_lookup;

#if WORKLOAD_ROCKSDB
int execve_fd_passing[32768];
int _nvp_ino_lookup[32768];
#elif WORKLOAD_FILEBENCH
int execve_fd_passing[16384];
int _nvp_ino_lookup[16384];
#else
int execve_fd_passing[1024];
int _nvp_ino_lookup[1024];
#endif

int _nvp_free_list_head;
struct full_dr* _nvp_full_drs;
int full_dr_idx;
struct NVTable_maps *_nvp_tbl_mmaps;
struct NVTable_maps *_nvp_over_tbl_mmaps;
struct NVLarge_maps *_nvp_tbl_regions;

struct InodeToMapping* _nvp_ino_mapping;
int OPEN_MAX; // maximum number of simultaneous open files
