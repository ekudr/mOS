#ifndef __OBJECT_H__
#define __OBJECT_H__

typedef enum {
    KO_NONE = 0,
    KO_CNODE,
    KO_ENDPOINT,
    KO_FASTCALL,
    KO_REPLAY,
    KO_IRQ,
    KO_FRAME,
    KO_SHMEM,
    KO_TASK,
} ko_type_t;

//#define KO_OWNER(ko)    (((kobject_t *)ko)->owner)
//#define KO_LOCK(ko)     (&((kobject_t *)ko)->lock)
//#define KO_TYPE(ko)    (((kobject_t *)ko)->type)
//#define lock_ko(ko)     acquire(&((kobject_t *)ko)->lock)
//#define unlock_ko(ko)   release(&((kobject_t *)ko)->lock)

struct task;

typedef struct kobject
{
    uint16_t    type;
    uint16_t    refcount;

} kobject_t;

static inline kobject_t *ko_init(kobject_t *ko, struct task *task, uint16_t type)
{
    if (!ko) return NULL;
    __atomic_store_n(&ko->refcount, 1, __ATOMIC_RELEASE);
    ko->type = type;
    return ko;
}

static inline kobject_t *ko_get(kobject_t *ko)
{
    if (ko) __atomic_fetch_add(&ko->refcount, 1, __ATOMIC_ACQ_REL);
    return ko;
}

void mfree(void *ptr);


static inline void ko_put(kobject_t *ko, uint32_t type, void (*destroy)(kobject_t *, uint32_t))
{
    if (ko == NULL) return;
    if (__atomic_sub_fetch(&ko->refcount, 1, __ATOMIC_ACQ_REL) == 1) {
        //destroying if last one
        if (destroy) destroy(ko, type);
        else mfree(ko);
    }
}

#endif /* __OBJECT_H__ */