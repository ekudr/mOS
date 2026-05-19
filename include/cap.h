#ifndef __CAP_H__
#define __CAP_H__

struct kobject;

struct task;

#define MAX_ROOT_CAPS 64
#define MAX_CAPS 170

#define MAX_MEM_SLOTS 170

#include <cap_types.h>


typedef struct cap_entry
{
    uint32_t        type;
    uint32_t        rights;
    struct kobject *obj;
    uint64_t        badge;  // per-cap badge for CAP_NOTIFICATION (ORed into notif->word on signal)
} cap_entry_t;

typedef struct cap_node
{
    struct kobject  hdr;
    cap_entry_t     caps[MAX_CAPS]; // 170 caps can fit 
} cap_node_t;

_Static_assert((sizeof(cap_node_t) < 0x1000), "cap_node_t structure size");

enum cap_root{
    CAP_ROOT,
    MEM_ROOT,

    ROOT_NUMS,
};

_Static_assert((ROOT_NUMS < MAX_ROOT_CAPS), "task caps number");

// cap id format 
//    +--------+--------+--------+--------+
// 31 |  8bits | 8 bits | 8 bits | 8 bits | 0
//    +--------+--------+--------+--------+ 
//    |   00   |root idx|node idx|node idx|
//    +--------+--------+--------+--------+
#define get_cap_root(id) (((int)id >> 16) & 0xFF)
#define set_cap_root(id) (((int)id & 0xFF) << 16)


static inline void cap_insert(cap_entry_t *ce, void *obj, cap_type_t type, uint32_t rights)
{
    ce->type   = type;
    ce->rights = rights;    
    ce->obj    = ko_get((kobject_t *)obj);
//    ce->badge  = 0;
}

void *create_cnode(void);
int cap_init_cnode(struct task *t);
int cap_install(struct task *t, void *obj, cap_type_t type, uint32_t rights);
int cap_replay_install(struct task *task, struct task *server);
cap_entry_t *cap_lookup(struct task *t, int id);
void cap_free(struct task *t, int id);
int sys_capability_create(struct task *t);
int sys_endpoint_create(struct task *t);

long sys_cap_grant(int from_id, uint64_t to_pid, int to_slot, uint32_t req_rights, uint64_t badge);
int cap_grant_into(struct task *from, int from_cap_id, \
                    struct task *to, int to_slot, uint32_t req_rights, uint64_t badge);
int sys_cap_transfer(int src_cap, int dest_cap, uint32_t req_rights);
int sys_cap_shmem_create(struct task *t);
cap_node_t *cap_create_node(struct task *t);

#endif /* __CAP_H__ */