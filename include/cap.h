#ifndef __CAP_H__
#define __CAP_H__

struct kobject;

struct task;



#include <cap_types.h>

typedef struct cap_entry
{
//    bool       valid;   // ??? Ican as not NULL object
    cap_type_t type;    // use it for non-kernel object like REPLAY
                        // Maybbe I need to create kobbject
    struct kobject  *obj;
    uint32_t   rights;
} cap_entry_t;

#define MAX_CAPS 128

void *create_cnode(void);

int cap_install(struct task *t, void *obj, cap_type_t type, uint32_t rights);
int cap_replay_install(struct task *task, struct task *server);
cap_entry_t *cap_lookup(struct task *t, int id);
void cap_free(struct task *t, int id);
int sys_capability_create(struct task *t);
int sys_endpoint_create(struct task *t);

long sys_cap_grant(int from_id, uint64_t to_pid, int to_slot, uint32_t req_rights);
int cap_grant_into(struct task *from, int from_cap_id, \
                    struct task *to, int to_slot, uint32_t req_rights);
int sys_cap_transfer(int src_cap, int dest_cap, uint32_t req_rights);
int sys_cap_shmem_create(struct task *t);

#endif /* __CAP_H__ */