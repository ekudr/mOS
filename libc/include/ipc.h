#ifndef __LIBC_IPC_H__
#define __LIBC_IPC_H__

int ipc_endpoint_create(uint32_t rights);
int ipc_fastcall_create(uint32_t rights);

void *ipc_shm_attach(int cap_id, const void *addr, int flags);

#endif /* __LIBC_IPC_H__ */