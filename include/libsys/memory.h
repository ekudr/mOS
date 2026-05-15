#ifndef __LIBSYS_MEMORY_H__
#define __LIBSYS_MEMORY_H__

#include <stdint.h>
#include <list.h>




#define CHUNK_HEAD_SIZE (sizeof(uint64_t) * 2)

/* kernel allocates page rounded memory */
#define PAGE_SIZE               4096 /* 4K pages */

#define PGROUNDUP(sz)           (((sz)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
#define PGROUNDDOWN(a)          (((a)) & ~(PAGE_SIZE-1))

/* RISC-V native round up mem blocks */
#define MEM_ALIGN               16
#define MEMROUNDUP(sz)          (((sz)+MEM_ALIGN-1) & ~(MEM_ALIGN-1))

#define MIN_CHUNK               32
#define HEAP_MIN_SIZE           32768  // 8 pages

#define req2size(req)           (MEMROUNDUP(req) < MIN_CHUNK)  ?     \
                                MIN_CHUNK : MEMROUNDUP(req)

/* Only used to pre-fill the tunables.  */
# define tidx2usize(idx)	(((size_t) idx) * MEM_ALIGN + MIN_CHUNK)

/* When "x" is from chunksize().  */
# define csize2tidx(x) (((x) - MIN_CHUNK + MEM_ALIGN - 1) / MEM_ALIGN)
/* When "x" is a user-provided size.  */
# define usize2tidx(x) csize2tidx (req2size(x))

/* Flags */
#define CHNK_INUSE        0x1
#define CHUNK_MAGIC_ALLOC 0xC0FFEE42u

#define chunk2mem(p)   ((void*)((char*)(p) + CHUNK_HEAD_SIZE))
#define mem2chunk(mem) ((mchunkptr_t)((char*)(mem) - CHUNK_HEAD_SIZE))

/* offset 2 to use otherwise unindexable first 2 bins */
#define fastbin_index(sz)       ((((unsigned int) (sz)) >> 4) - 2)

/* The maximum fastbin request size we support */
#define MAX_FAST_SIZE     160

#define NFASTBINS  (fastbin_index (req2size (MAX_FAST_SIZE)) + 1)

static inline int safe_fastbin_index(uint64_t sz) {
    if (sz < MIN_CHUNK || sz > MAX_FAST_SIZE) return -1;
    return (int)fastbin_index(sz);
}


struct mem_chunk
{
    uint32_t magic;    /* CHUNK_MAGIC_ALLOC when allocated, 0 when free */
    uint32_t flags;    /* CHNK_INUSE etc. */
    uint64_t size;
    list_head_t freelist;
};
typedef struct mem_chunk mem_chunk_t;
typedef struct mem_chunk* mchunkptr_t;

struct mem_heap
{
    list_head_t heaplist;
    uint64_t size;
    uint64_t dummy;   // to align    
};
typedef struct mem_heap mem_heap_t;

struct mem_mgr
{
    //mutex ????
    list_head_t qheap;
    list_head_t qfast[NFASTBINS];
    list_head_t qfree;

    // ----- Statistic -----
    // System memory allocated
    uint64_t sysmem;

};
typedef struct mem_mgr mem_mgr_t;


/*
 * Memory map structures
 */

enum{
    MAP_SHARED  = 0x0001,
    MAP_PRIVATE = 0x0002,
    MAP_MEMIO   = 0x0004,
    MAP_READ    = 0x0010,
    MAP_WRITE   = 0x0020,
    MAP_EXEC    = 0x0040,
};

extern char _end[];

void *malloc(size_t size);
void free(void *ptr);
void *mmap(void *addr, uint64_t len, uint64_t flags, void *paddr);

#endif /* __LIBSYS_MEMORY_H__ */