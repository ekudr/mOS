#ifndef __LIBC_IPC_H__
#define __LIBC_IPC_H__

#define IPC_NOWAIT  0x01     // do non-blocking ipc request

int ipc_endpoint_create(uint32_t rights);
int ipc_fastcall_create(uint32_t rights);

kerrno_t ipc_send(int cap_id, void *msg, uint64_t size);
int ipc_receive(int id, void *msg, uint64_t size, int flags);
kerrno_t ipc_reply(int cap_id, void *msg, uint64_t size);
kerrno_t ipc_call(int cap_id, void *msg, void *rep, uint64_t size);

void *ipc_shm_attach(int cap_id, const void *addr, int flags);

uint64_t __fast_call_7(uint64_t n, uint64_t a, uint64_t b, uint64_t c, 
                            uint64_t d, uint64_t e, uint64_t f, uint64_t h,
                            uint64_t *_a, uint64_t *_b, uint64_t *_c, 
                            uint64_t *_d, uint64_t *_e, uint64_t *_f);

#endif /* __LIBC_IPC_H__ */