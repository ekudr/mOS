#ifndef __DMA_POOL_H__
#define __DMA_POOL_H__

#include <stdint.h>
#include <stddef.h>
#include <list.h>

typedef uintptr_t   paddr_t;

typedef struct dma_block {
    size_t      size;                    // Size of this block (including header)
    paddr_t     paddr;                // Physical address of this block
    int         free;                       // 1 if free, 0 if allocated
    list_head_t block_list;    // List of blocks in the pool
} dma_block_t;

typedef struct dma_pool
{
    int      cap_id;            // Capability id
    size_t   size;
    paddr_t  pa_base;
    void *   va_base;
    size_t   offset;
    list_head_t block_list;
} dma_pool_t;


static inline paddr_t dma_get_phys(dma_pool_t *p, void *va)
{
    uintptr_t off = (uintptr_t)va - (uintptr_t)p->va_base;
    return (paddr_t)p->pa_base + off;
}

static inline size_t align_up(size_t val, size_t align)
{
    return (val + align - 1) & ~(align - 1);
}

static inline size_t align_down(size_t val, size_t align)
{
    return val & ~(align - 1);
}

void dma_pool_init(dma_pool_t **p, size_t size);
void *dma_alloc(dma_pool_t *p, size_t size, size_t align);
void dma_free(dma_pool_t *p, void *ptr);

static inline void *dma_alloc_page(dma_pool_t *p)
{
    return dma_alloc(p, 4096, 4096);
}

void dma_cache_invalidate(dma_pool_t *p, void *va, size_t size);
void dma_cache_flush(dma_pool_t *p, void *va, size_t size);
void dma_pool_dump(dma_pool_t *p);

#endif /* __DMA_POOL_H__ */