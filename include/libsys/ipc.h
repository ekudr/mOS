#ifndef __LIBSYS_IPC_H__
#define __LIBSYS_IPC_H__

#include <stdint.h>

#ifndef IPC_NOWAIT
#define IPC_NOWAIT  0x100000000     // do not sleep task
#endif /* IPC_NOWAIT */
#ifndef IPC_EXIST
#define IPC_EXIST   0x200000000     // do not create new
#endif /* IPC_EXIST */

int snd_msg(uint64_t qid, uint64_t type, uintptr_t buf, uint64_t size, uint64_t flags);
int rcv_msg(uint64_t qid, uint64_t type, uintptr_t buf, uint64_t size, uint64_t flags);
uint64_t get_msg(uint64_t qkey, uint64_t flags);

uint64_t shmget(uint64_t key, size_t size, uint64_t shmflg);
void *shmat(uint64_t shmid, const void *shmaddr, uint64_t shmflg);
int shmdt(const void *shmaddr);



#endif /* __LIBSYS_IPC_H__ */