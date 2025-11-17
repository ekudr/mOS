#include <common.h>
#include <libsys/ipc.h>
#include <nameserver.h>
#include <string.h>
#include <libsys/cap.h>
//#include <memory.h>

#define MAX_NAMES    64

struct entry
{
    char name[MAX_NAME_LEN];
    int  cap_id;
};

static struct entry registry[MAX_NAMES];
static int count = 0;

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}

int main()
{
    debug("Name server v0.0.1\n");
//    int ns_cap = ipc_endpoint_create(CRIGHT_RCV);
//    debug("Capabbility is created %d\n", ns_cap);
    for (;;) {
        ns_msg_t *msg = (ns_msg_t *)get_ipc_buffer()->msg;
        int sender;
//        debug("NS srvr resv msg 0x%lX size %d\n", msg, sizeof(msg));
        uint64_t info = ipc_recv(0, NULL);
        int ret = (int)label_from_msginfo_word(info);
        if (ret < 0 || !length_from_msginfo_word(info)) {
            debug("[NS] Error receiving message %d info 0x%lX\n", ret, info);
        }
        sender = msg->pid;
//        debug("[NAMESERVER] resved msg type %d cap id %d\n", msg.type, msg.cap_id);
        if (msg->type == NS_REGISTER) {
            if (count < MAX_NAMES) {
                
                strncpy(registry[count].name, msg->name, sizeof(registry[count].name)-1);
//                debug("[NAMESERVER] received %s cap %d\n",msg.name, msg.cap_id);
                if (ipc_get_cap(0) < 0) panic("[NS] received no cap");
                registry[count].cap_id = ipc_get_cap(0);

                count++;

                memset(msg, 0, sizeof(ns_msg_t)); 
                msg->type = NS_REPLAY;
                msg->cap_id = 0;
                
            } else {
                msg->type = NS_REPLAY;
                msg->cap_id = -1;
            }
            info = msginfo_word_new(0,sizeof(ns_msg_t)/8, 0, 0);
            ipc_reply(info);

        } else if (msg->type == NS_LOOKUP) {
            int found = -1;
//            debug("Looking for %s...\n", msg.name);
            for (int i=0; i < count; i++) {
                if (strcmp(registry[i].name, msg->name) == 0){
                    found = registry[i].cap_id;
//                    debug("[NAMESERVER] found %s id %d\n", registry[i].name, registry[i].cap_id);
                    break;
                }
            }
            if (found) {
                if (sender <= 0)
                    return -EINVAL;
                // Grant capability to requster
                int g_cap = cap_grant(found, sender, -1 , CRIGHT_SND);
                if (g_cap < 0) {
//                    debug("Error granting cap %d \n", g_cap);
                    for(;;);
                }
                found = g_cap;
            }
            memset(msg, 0, sizeof(ns_msg_t));     
            msg->type = NS_REPLAY;
            msg->cap_id = found;
            info = msginfo_word_new(0,sizeof(ns_msg_t)/8, 0, 0);
            ipc_reply(info);
        }
       
    }
}
