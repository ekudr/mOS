#include <common.h>
#include <ipc.h>
#include <memory.h>
#include <string.h>
#include <riscv.h>

#include "syscall.h"

#include <sys/types.h>
#include <mosstd.h>

#include "qmsg.h"

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}

struct stream{
    uint64_t flag;
    char buf[248];
};

struct stream *buffer;

void irq_handler(uint32_t sig, uint64_t irq)
{
    debug("[TASK ONE] irq handler sig %d irq %d\n", sig, irq);
//    signal_return();
}

int main()
{
    uint64_t qkey   = 0x01204E4F43535953;
    uint64_t qid    = 0;
    dm_msg_t *msg;
    uint64_t pid;

    char *hello = "Hello world\r\n";

    pid = getpid();

    debug("One App test\n");
//    signal_action(1, irq_handler);
    
    do {
       qid = shmget(qkey, 0, IPC_EXIST);
    } while (!qid);

    debug("Shmem ID 0x%lX\n", qid);
    buffer = (struct stream *)shmat(qid, NULL, 0);
    debug("Shmem addr 0x%lX\n", buffer);
    
    for (int i = 0; i < 0x20; i++){
        uint64_t f = __atomic_exchange_n (&buffer[i].flag, 1, __ATOMIC_ACQ_REL);
        if (f == 0){
            strcpy(buffer[i].buf, hello);
            break;
        }
    }

    

    for(;;);
}