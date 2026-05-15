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
/*
    for(int i=160; i<=512; i+=16){
        printf("size %d tindx %d\n", i, usize2tidx(i));
    }
*/    
    return SUCCESS;
}

static inline void __add_to_free(mchunkptr_t chunk)
{
    list_add(&gp_mmgr->qfree, &chunk->freelist);
}

static inline void __add_to_fast(mchunkptr_t chunk)
{
    int idx = safe_fastbin_index(chunk->size);
    if (idx < 0) { __add_to_free(chunk); return; }
    list_add(&gp_mmgr->qfast[idx], &chunk->freelist);
}

static mchunkptr_t __split_chunk(mchunkptr_t chunk, uint64_t size)
{
    mchunkptr_t nchunk;
//    debug("Srink the chunk 0x%lX sz 0x%lX -> 0x%lX\n", chunk, chunk->size, size);
    uint64_t sz = chunk->size - size;

    chunk->size = size;

    nchunk = (mchunkptr_t)((uint64_t)chunk + CHUNK_HEAD_SIZE + size);
    nchunk->flags = 0;
    nchunk->size = sz - CHUNK_HEAD_SIZE;

    if(nchunk->size <= MAX_FAST_SIZE) {
        __add_to_fast(nchunk);
    } else {
        __add_to_free(nchunk);
    }    
 //   debug("The new chunk 0x%lX sz 0x%lX\n", nchunk, nchunk->size);
//    debug("The requested chunk 0x%lX sz 0x%lX\n", chunk, chunk->size);
    return chunk;
}

static void *__find_free(size_t size)
{
    list_head_t *chpos;
//    mem_heap_t *heap;
    mchunkptr_t chunk;
//    debug("Looking memsize 0x%lX\n", size);

    // Fast first
    if(size <= MAX_FAST_SIZE){
        int idx = safe_fastbin_index(size);
        if(idx >= 0 && !list_is_empty(&gp_mmgr->qfast[idx])){
            chpos = gp_mmgr->qfast[idx].next;
            chunk = container_of(chpos, mem_chunk_t, freelist);
            list_del(&chunk->freelist);
            chunk->flags |= CHNK_INUSE;
            return chunk2mem(chunk);
        }
    }
    

    if (list_is_empty(&gp_mmgr->qfree))
        if (request_heap(size) < 0) return NULL;

    list_for_each_entry(chunk, &gp_mmgr->qfree, freelist) {
        if(chunk->size >= size) {
//            debug("Checking chunk 0x%lX sz 0x%lX\n", chunk, chunk->size);
            list_del(&chunk->freelist);
            chunk->flags |= CHNK_INUSE;
            if((chunk->size - size) >= MIN_CHUNK + CHUNK_HEAD_SIZE) {                
                __split_chunk(chunk, size);
            }
//            debug("Return the chunk 0x%lX sz 0x%lX\n", chunk, chunk->size);
            return chunk2mem(chunk);
        }
    }

    return NULL;
}

int request_heap(size_t size)
{
    mem_heap_t *heap;
    mchunkptr_t chunk;
//    debug("Requested memsize 0x%lX\n", size);
    size += CHUNK_HEAD_SIZE + sizeof(mem_heap_t);
    if (size < CHUNK_HEAD_SIZE + sizeof(mem_heap_t)) return -EINVAL; // overflow guard
//    debug("memsize + structures 0x%lX\n", size);
    size = PGROUNDUP(size);
//    debug("Page round up memsize 0x%lX\n", size);

    size_t bsize = (size <= HEAP_MIN_SIZE) ? HEAP_MIN_SIZE : size;
    
//    debug("Request memory for heap 0x%lX bytes\n", bsize);
//    heap = (mem_heap_t *)sbrk(bsize);

    heap = (mem_heap_t *)mmap(heap_top, bsize, MAP_PRIVATE | MAP_READ | MAP_WRITE, 0);

    /* mOS sys_mmap returns NULL on failure, unlike POSIX MAP_FAILED */
    if(heap == NULL)
        return -ENOMEM;
    heap_top += bsize;

//    debug("Heap address returned 0x%lX\n", heap);
    list_init(&heap->heaplist);

    list_add(&gp_mmgr->qheap,&heap->heaplist);
    chunk = (mem_chunk_t *)((uint64_t)heap + sizeof(mem_heap_t));
    chunk->flags = 0;
    chunk->size = (bsize - sizeof(mem_heap_t) - CHUNK_HEAD_SIZE);

    __add_to_free(chunk);

    gp_mmgr->sysmem += bsize;
//    debug("Chunk address 0x%lX size 0x%lX\n", chunk, chunk->size);
    return 0; 
}

/*
 * Allocate memory. 
 */
void *malloc(size_t size)
{
    void *ret;
    size = req2size(size);
//    debug("Alocating memsize 0x%lX\n", size);

    /* Not thread-safe: protect with a once-flag if threading is added */
    if(gp_mmgr == NULL)
        if(mem_init(size) < 0 )
            return NULL;

    while ((ret = __find_free(size)) == NULL) {
        if(request_heap(size) < 0)
            return NULL;        
    }
//    debug("mem address 0x%lX\n", ret);
    return ret;
}     


void free(void *ptr)
{
    mchunkptr_t chunk;
    if (!ptr)
        return;

    chunk = mem2chunk(ptr);
    if (!(chunk->flags & CHNK_INUSE)) return; // double-free guard
    chunk->flags &= ~(CHNK_INUSE);
    list_init(&chunk->freelist);

    if(chunk->size <= MAX_FAST_SIZE) {
        __add_to_fast(chunk);
        return;
    }

    __add_to_free(chunk);
    
}