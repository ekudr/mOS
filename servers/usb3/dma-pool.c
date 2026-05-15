#include <libsys/common.h>
#include <string.h>
#include <libsys/memory.h>
#include <cap.h>

#include "dma-pool.h"

void dma_pool_init(dma_pool_t **out, size_t size)
{
    if (unlikely(*out)) {
        debug("[DMA_POOL] DMA pool already exist\n");
        return;
    }

    dma_pool_t *p = (dma_pool_t *)malloc(sizeof(dma_pool_t));

    p->size = 0;
    p->offset = 0;

    p->cap_id = cap_dmamem_create(size, CRIGHT_MAP, (uint64_t *)&p->pa_base);
    if (p->cap_id < 0) {
        debug("[DMA_POOL] Cannot allocate DMA mem block\n");
        free(p);
        return;
    }
    p->size = size;

    p->va_base = mmap(NULL, size, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)p->pa_base);
    if (!p->va_base) {
        debug("[DMA_POOL] Cannot map DMA mem block\n");
        free(p);
        return;
    }

    list_init(&p->block_list);

    dma_block_t *block = (dma_block_t *)malloc(sizeof(dma_block_t));
    block->size = size;
    block->paddr = p->pa_base;
    block->free = 1;

    list_add(&p->block_list, &block->block_list);

    *out = p;
    debug("[DMA_POOL] Allocated %u bytes at phys 0x%lX virt 0x%lX\n", p->size, p->pa_base, p->va_base);
}

void *dma_alloc(dma_pool_t *p, size_t size, size_t align)
{
    if (!p || !size) return NULL;

    size = align_up(size, align);
    
    dma_block_t *block;
    list_for_each_entry(block, &p->block_list, block_list) {
        if (block->free) {
            size_t aligned_addr = align_up((size_t)block->paddr, align);
            size_t padding = aligned_addr - (size_t)block->paddr;
            if (block->size >= (size + padding)) {
                // Found a suitable block
                if (padding > 0) {
                    // Create a new block for the padding
                    dma_block_t *pad_block = (dma_block_t *)malloc(sizeof(dma_block_t));
                    pad_block->size = padding;
                    pad_block->paddr = block->paddr;
                    pad_block->free = 1;
                    list_add(&block->block_list, &pad_block->block_list);

                    // Adjust the original block
                    block->size -= padding;
                    block->paddr += padding;
                }

                if ((block->size > size) && (block->size > align)) {
                    // Split the block
                    dma_block_t *new_block = (dma_block_t *)malloc(sizeof(dma_block_t));
                    new_block->size = block->size - size;
                    new_block->paddr = block->paddr + size;
                    new_block->free = 1;
                    list_add(&block->block_list, &new_block->block_list);

                    block->size = size;
                }

                block->free = 0;
                void *va = (uint8_t *)p->va_base + ((size_t)block->paddr - (size_t)p->pa_base);
                memset(va, 0, size);
                return va;
            }
        }
    }

    return NULL;
}

void dma_free(dma_pool_t *p, void *ptr)
{
    if (!p || !ptr) return;

    paddr_t paddr = dma_get_phys(p, ptr);

    dma_block_t *block;
    list_for_each_entry(block, &p->block_list, block_list) {
        if (block->paddr == paddr) {
            block->free = 1;
            // Coalescing adjacent free blocks can be implemented here
            return;
        }
    }
}

// Invalidate cache for DMA memory region
void dma_cache_invalidate(dma_pool_t *p, void *va, size_t size)
{
    if (!p || !va || !size) return; 

    size = align_up(size, 64);
    void *paddr = (void *)align_down(dma_get_phys(p, va), 64);
    cache_invalidate(paddr, size);
}

// Flash cache for DMA memory region
void dma_cache_flush(dma_pool_t *p, void *va, size_t size)
{
    if (!p || !va || !size) return; 
    size = align_up(size, 64);
    void *paddr = (void *)align_down(dma_get_phys(p, va), 64);
    cache_flush(paddr, size);
}

void dma_pool_dump(dma_pool_t *p)
{
    if (!p) return;

    debug("[DMA_POOL] Dumping DMA pool blocks:\n");
    dma_block_t *block;
    list_for_each_entry(block, &p->block_list, block_list) {
        debug("  Block at phys 0x%lX size %u bytes %s\n",
              block->paddr, block->size, block->free ? "FREE" : "ALLOCATED");
    }
}