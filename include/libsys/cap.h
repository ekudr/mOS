#ifndef __CAP_H__
#define __CAP_H__

#include <cap_types.h>
#include <libsys/ipc.h>

typedef int cap_id_t;
typedef uint32_t cap_rights_t;

int create_capability(cap_type_t type, cap_rights_t rights);
// int cap_transfer(int src_cap, int dest_cap, uint32_t req_rights);
int cap_grant(int from_id, uint64_t to_pid, int to_slot, uint32_t req_rights);

int cap_shmem_create(uint64_t size, cap_rights_t rights);
int cap_dmamem_create(uint64_t size, cap_rights_t rights, uint64_t *paddr);
int cap_frame_create(void *addr, uint64_t size, cap_rights_t rights);
int cap_create(uint64_t dest, msg_info_t info);
int cap_free(int cap_id);

int cap_task_mem_move(int cap_id, void *vaddr, int mem_cap, uint64_t flags);
int cap_task_run(int cap_id, uint64_t entry);

#endif /* __CAP_H__ */