#ifndef __CAP_H__
#define __CAP_H__

struct kobject;

struct task;

#define MAX_CAPS 64

#include <cap_types.h>

typedef struct cap_entry
{
    bool       valid;   // ??? Ican as not NULL object
    cap_type_t type;    // use it for non-kernel object like REPLAY
                        // Maybbe I need to create kobbject
    struct kobject  *obj;
    uint32_t   rights;
} cap_entry_t;

int cap_install(struct task *t, void *obj, cap_type_t type, uint32_t rights);
cap_entry_t *cap_lookup(struct task *t, uint32_t id);
void cap_free(struct task *t, uint32_t id);
int sys_capability_create(struct task *t, cap_type_t type, uint32_t rights);
int sys_endpoint_create(struct task *t, uint32_t right);
int fastcall_create(struct task *t, uint32_t rights);
long sys_cap_grant(int from_id, uint64_t to_pid, int to_slot, uint32_t req_rights);
int cap_grant_into(struct task *from, int from_cap_id, \
                    struct task *to, int to_slot, uint32_t req_rights);

#endif /* __CAP_H__ */