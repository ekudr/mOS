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

typedef struct kobject
{
    uint32_t    type;
    uint64_t    refcount;
//    uint32_t    rights;
} kobject_t;

inline kobject_t *ko_init(kobject_t *ko)
{
    if (ko == NULL)
        return NULL;
    ko->refcount = 1;
//    ko->rights   = 0;
    ko->type     = 0;
    return ko;
}

inline kobject_t *ko_get(kobject_t *ko)
{
    if (ko) __atomic_fetch_add(&ko->refcount, 1, __ATOMIC_ACQ_REL);
    return ko;
}

void mfree(void *ptr);

inline void ko_put(kobject_t *ko, void (*destroy)(kobject_t *))
{
    if (ko == NULL) return;
    if (__atomic_fetch_sub(&ko->refcount, 1, __ATOMIC_ACQ_REL) == 1) {
        //destroying if last one
        if (destroy) destroy(ko);
        else mfree(ko);
    }
}

#endif /* __OBJECT_H__ */