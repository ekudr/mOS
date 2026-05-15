/*
 *  Memory allocation for user space
 */

#include <libsys/common.h>
#include <libsys/memory.h>


int request_heap(size_t size);

mem_mgr_t memory_manager;

mem_mgr_t *gp_mmgr = NULL;

void *heap_top;

int mem_init(size_t size)
{
    gp_mmgr = &memory_manager;
    list_init(&gp_mmgr->qheap);
    for(int i = 0; i < NFASTBINS; i++){
        list_init(&gp_mmgr->qfast[i]);
    }
    list_init(&gp_mmgr->qfree);

    gp_mmgr->sysmem = 0;

    heap_top = (void *)PGROUNDUP((uint64_t)_end);

    if(request_heap(size) < 0)
        return -1;

    return SUCCESS;
}

/*
 * Insert chunk into qfree in address-sorted order, then merge with any
 * immediately adjacent free neighbors (forward coalescing and backward
 * coalescing). Heap-block boundaries are handled implicitly: each mmap'd
 * block starts with a mem_heap_t header, so address arithmetic across block
 * boundaries never satisfies the adjacency check.
 */
static void __sorted_insert_and_coalesce(mchunkptr_t chunk)
{
    list_head_t *pos;
    mchunkptr_t next_free = NULL;

    list_for_each(pos, &gp_mmgr->qfree) {
        mchunkptr_t cur = container_of(pos, mem_chunk_t, freelist);
        if ((uintptr_t)cur > (uintptr_t)chunk) {
            next_free = cur;
            break;
        }
    }

    if (next_free)
        __list_add(&chunk->freelist, next_free->freelist.prev, &next_free->freelist);
    else
        list_add_tail(&gp_mmgr->qfree, &chunk->freelist);

    /* Merge with prev neighbor if adjacent */
    if (!list_is_first(&chunk->freelist, &gp_mmgr->qfree)) {
        mchunkptr_t prev = list_prev_entry(chunk, freelist);
        if ((char*)prev + CHUNK_HEAD_SIZE + prev->size == (char*)chunk) {
            prev->size += CHUNK_HEAD_SIZE + chunk->size;
            list_del(&chunk->freelist);
            chunk = prev;
        }
    }

    /* Merge with next neighbor if adjacent */
    if (!list_is_last(&chunk->freelist, &gp_mmgr->qfree)) {
        mchunkptr_t nxt = list_next_entry(chunk, freelist);
        if ((char*)chunk + CHUNK_HEAD_SIZE + chunk->size == (char*)nxt) {
            chunk->size += CHUNK_HEAD_SIZE + nxt->size;
            list_del(&nxt->freelist);
        }
    }
}

/* Drain all fastbins into qfree with coalescing. Called before growing the heap. */
static void __consolidate_fastbins(void)
{
    for (int i = 0; i < NFASTBINS; i++) {
        while (!list_is_empty(&gp_mmgr->qfast[i])) {
            list_head_t *p = gp_mmgr->qfast[i].next;
            mchunkptr_t c = container_of(p, mem_chunk_t, freelist);
            list_del(p);
            __sorted_insert_and_coalesce(c);
        }
    }
}

static inline void __add_to_fast(mchunkptr_t chunk)
{
    int idx = safe_fastbin_index(chunk->size);
    if (idx < 0) { __sorted_insert_and_coalesce(chunk); return; }
    list_add(&gp_mmgr->qfast[idx], &chunk->freelist);
}

static mchunkptr_t __split_chunk(mchunkptr_t chunk, uint64_t size)
{
    uint64_t sz = chunk->size - size;

    chunk->size = size;

    mchunkptr_t nchunk = (mchunkptr_t)((uint64_t)chunk + CHUNK_HEAD_SIZE + size);
    nchunk->magic = 0;
    nchunk->flags = 0;
    nchunk->size = sz - CHUNK_HEAD_SIZE;

    if (nchunk->size <= MAX_FAST_SIZE)
        __add_to_fast(nchunk);
    else
        __sorted_insert_and_coalesce(nchunk);

    return chunk;
}

static void *__find_free(size_t size)
{
    list_head_t *chpos;
    mchunkptr_t chunk;

    /* Fast first */
    if (size <= MAX_FAST_SIZE) {
        int idx = safe_fastbin_index(size);
        if (idx >= 0 && !list_is_empty(&gp_mmgr->qfast[idx])) {
            chpos = gp_mmgr->qfast[idx].next;
            chunk = container_of(chpos, mem_chunk_t, freelist);
            list_del(&chunk->freelist);
            chunk->flags |= CHNK_INUSE;
            chunk->magic = CHUNK_MAGIC_ALLOC;
            return chunk2mem(chunk);
        }
    }

    /*
     * Scan qfree. On miss, consolidate fastbins (which may produce larger
     * coalesced chunks) and retry once before giving up and returning NULL.
     * If NULL is returned, malloc() will call request_heap() and retry.
     */
    for (int pass = 0; pass <= 1; pass++) {
        list_for_each_entry(chunk, &gp_mmgr->qfree, freelist) {
            if (chunk->size >= size) {
                list_del(&chunk->freelist);
                chunk->flags |= CHNK_INUSE;
                chunk->magic = CHUNK_MAGIC_ALLOC;
                if ((chunk->size - size) >= MIN_CHUNK + CHUNK_HEAD_SIZE)
                    __split_chunk(chunk, size);
                return chunk2mem(chunk);
            }
        }
        if (!pass) __consolidate_fastbins();
    }

    return NULL;
}

int request_heap(size_t size)
{
    mem_heap_t *heap;
    mchunkptr_t chunk;

    size += CHUNK_HEAD_SIZE + sizeof(mem_heap_t);
    if (size < CHUNK_HEAD_SIZE + sizeof(mem_heap_t)) return -EINVAL; // overflow guard
    size = PGROUNDUP(size);

    size_t bsize = (size <= HEAP_MIN_SIZE) ? HEAP_MIN_SIZE : size;

    heap = (mem_heap_t *)mmap(heap_top, bsize, MAP_PRIVATE | MAP_READ | MAP_WRITE, 0);

    /* mOS sys_mmap returns NULL on failure, unlike POSIX MAP_FAILED */
    if (heap == NULL)
        return -ENOMEM;
    heap_top += bsize;

    list_init(&heap->heaplist);
    list_add(&gp_mmgr->qheap, &heap->heaplist);

    chunk = (mem_chunk_t *)((uint64_t)heap + sizeof(mem_heap_t));
    chunk->magic = 0;
    chunk->flags = 0;
    chunk->size = (bsize - sizeof(mem_heap_t) - CHUNK_HEAD_SIZE);

    /* New heap is always at the highest address; append to tail to preserve sorted order */
    list_add_tail(&gp_mmgr->qfree, &chunk->freelist);

    gp_mmgr->sysmem += bsize;
    return 0;
}

/*
 * Allocate memory.
 */
void *malloc(size_t size)
{
    void *ret;
    size = req2size(size);

    /* Not thread-safe: protect with a once-flag if threading is added */
    if (gp_mmgr == NULL)
        if (mem_init(size) < 0)
            return NULL;

    while ((ret = __find_free(size)) == NULL) {
        if (request_heap(size) < 0)
            return NULL;
    }
    return ret;
}


void free(void *ptr)
{
    if (!ptr) return;

    /* Alignment check */
    if ((uintptr_t)ptr % MEM_ALIGN) {
        debug("free: misaligned pointer %p\n", ptr);
        return;
    }

    mchunkptr_t chunk = mem2chunk(ptr);

    /* Magic check — detects double-free, foreign pointer, header corruption */
    if (chunk->magic != CHUNK_MAGIC_ALLOC) {
        debug("free: bad magic at %p (got 0x%X)\n", ptr, chunk->magic);
        return;
    }

    if (!(chunk->flags & CHNK_INUSE)) {
        debug("free: chunk %p not marked in-use\n", ptr);
        return;
    }

    /* Size sanity */
    if (chunk->size == 0
        || chunk->size > gp_mmgr->sysmem
        || (chunk->size % MEM_ALIGN)) {
        debug("free: bad size %lu at %p\n", chunk->size, ptr);
        return;
    }

    chunk->flags &= ~CHNK_INUSE;
    chunk->magic = 0;
    list_init(&chunk->freelist);

    if (chunk->size <= MAX_FAST_SIZE) {
        __add_to_fast(chunk);
        return;
    }

    __sorted_insert_and_coalesce(chunk);
}
