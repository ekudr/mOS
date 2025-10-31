#ifndef __OBJECT_H__
#define __OBJECT_H__

typedef enum {
    KO_NONE = 0,
    KO_ENDPOINT,
    KO_FASTCALL,
    KO_REPLAY,
    KO_IRQ,
    KO_SHMEM,
} ko_type_t;

struct task;

typedef struct kobject
{
    uint32_t    type;
    uint64_t    refcount;
    struct task *owner;
} kobject_t;

static inline kobject_t *ko_init(kobject_t *ko, struct task *task, uint32_t type)
{
    if (ko == NULL) return NULL;
    __atomic_store_n(&ko->refcount, 1, __ATOMIC_RELEASE);
    ko->type = type;
    ko->owner = task;
    return ko;
}

static inline kobject_t *ko_get(kobject_t *ko)
{
    if (ko) __atomic_fetch_add(&ko->refcount, 1, __ATOMIC_ACQ_REL);
    return ko;
}

void mfree(void *ptr);


static inline void ko_put(kobject_t *ko, void (*destroy)(kobject_t *))
{
    if (ko == NULL) return;
    if (__atomic_sub_fetch(&ko->refcount, 1, __ATOMIC_ACQ_REL) == 1) {
        //destroying if last one
        if (destroy) destroy(ko);
        else mfree(ko);
    }
}

#endif /* __OBJECT_H__ */