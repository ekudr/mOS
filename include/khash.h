#ifndef __KHASH_H__
#define __KHASH_H__

#include <list.h>
#include <spinlock.h>
#include <errno.h>

typedef struct khash_node
{
    uint64_t    key;
    void        *value;
    list_head_t hnode;
} khash_node_t;

typedef struct khash_table
{
    uint64_t    bits;
    spinlock_t  lock;
    list_head_t *buckets;
} khash_table_t;

khash_table_t *khash_create(uint64_t bits);
void khash_destroy(khash_table_t *ht);
kerrno_t khash_insert(khash_table_t *ht, uint64_t key, void *val);
void *khash_lookup(khash_table_t *ht, uint64_t key);
kerrno_t khash_remove(khash_table_t *ht, uint64_t key);


#endif /* __KHASH_H__ */