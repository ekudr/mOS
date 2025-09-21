#include <common.h>
#include <khash.h>





/*
 *  hash function
 */ 
static inline uint64_t khash_fn(uint64_t key, uint64_t bits)
{
    return (key * 11400714819323198485llu) >> (64 - bits);
} 

/*
 *  Create hash table
 */ 
khash_table_t *khash_create(uint64_t bits)
{
    khash_table_t   *ht;
    uint64_t        size = 1 << bits;

    ht = malloc(sizeof(khash_table_t));
    if (ht == NULL)
        return NULL;

    ht->buckets = malloc(size*sizeof(list_head_t));
    if (ht->buckets == NULL){
        mfree(ht);
        return NULL;
    }

    for (int i=0; i<size; i++)
        list_init(&ht->buckets[i]);

    ht->bits = bits;
    initlock(&ht->lock, "hash table");

    return ht;
}
 
/*
 * Destroy hash table
 */
void khash_destroy(khash_table_t *ht)
{
    khash_node_t *n;
    uint64_t      size = 1 << ht->bits;

    for (int i = 0; i < size; i++){
        list_for_each_entry(n, &ht->buckets[i], hnode){
            list_del(&n->hnode);
            mfree(n);
        }
    }
    mfree(ht->buckets);
    mfree(ht);
}

/*
 * Insert into hash table
 */
kerrno_t khash_insert(khash_table_t *ht, uint64_t key, void *val)
{
    khash_node_t *n;
    uint64_t h = khash_fn(key, ht->bits);

    n = malloc(sizeof(khash_node_t));
    if (n == NULL)
        return -ENOMEM;
    
    n->key = key;
    n->value = val;

    acquire(&ht->lock);
    list_add(&ht->buckets[h], &n->hnode);
    release(&ht->lock);
    return 0;
}

/* 
 * Lookup in hash table
 */
void *khash_lookup(khash_table_t *ht, uint64_t key)
{
    khash_node_t *n;
    uint64_t h = khash_fn(key, ht->bits);

    acquire(&ht->lock);    
    list_for_each_entry(n, &ht->buckets[h], hnode){
        if (n->key == key){
            release(&ht->lock);
            return n->value; 
        }
    }
    release(&ht->lock);
    return NULL;    
}

/*
 * Remove from hash table
 */
kerrno_t khash_remove(khash_table_t *ht, uint64_t key)
{
    khash_node_t *n;
    uint64_t h = khash_fn(key, ht->bits);

    acquire(&ht->lock);    
    list_for_each_entry(n, &ht->buckets[h], hnode){
        if (n->key == key){
            list_del(&n->hnode);
            mfree(n);
            release(&ht->lock);
            return SUCCESS; 
        }
    }
    release(&ht->lock);
    return -ENOENT;
}