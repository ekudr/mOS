#ifndef __LIBC_IPC_H__
#define __LIBC_IPC_H__

int ipc_endpoint_create(uint32_t rights);
int ipc_fastcall_create(uint32_t rights);
long cap_grant(int from_id, uint64_t to_pid, int to_slot, uint32_t req_rights);
kerrno_t ipc_send(uint32_t id, void *msg, uint64_t size);
kerrno_t ipc_receive(uint32_t id, void *msg, uint64_t size);
kerrno_t ipc_replay(uint32_t id, void *msg, uint64_t size);
kerrno_t ipc_call(uint32_t id, void *msg, void *rep, uint64_t size);

inline uint64_t __fast_call_7(uint64_t n, uint64_t a, uint64_t b, uint64_t c, 
                            uint64_t d, uint64_t e, uint64_t f, uint64_t h,
                            uint64_t *_a, uint64_t *_b, uint64_t *_c, 
                            uint64_t *_d, uint64_t *_e, uint64_t *_f);

#endif /* __LIBC_IPC_H__ */