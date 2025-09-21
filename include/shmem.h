#ifndef __SHMEM_H__
#define __SHMEM_H__

struct task;
struct shmem_block;

typedef struct shmqueue
{
    spinlock_t      lock;
    uint64_t        id;
    uint64_t        key;
    uint64_t        flags;
    list_head_t     shqlist;
    struct task     *task;  
    int             refcount;
    struct shmem_block   *memblock;
} shmqueue_t;

typedef struct shmem_page
{
    struct shmem_page   *next;
    struct shmem_block  *head;
    uint64_t            ppn;
} shmem_page_t;


typedef struct shmem_block
{
    size_t      size;
    size_t      npages;
    struct shmem_page *head;
} shmem_block_t;

shmqueue_t *shmem_create(uint64_t key, size_t size, uint64_t flags);
shmqueue_t *shmem_lookup(uint64_t shmid);
kerrno_t shmem_init(void);
#endif /* __SHMEM_H__ */