#include <common.h>
#include <ipc.h>
#include <nameserver.h>
#include <string.h>
#include <cap.h>
#include <memory.h>

#define MAX_NAMES    64

struct entry
{
    char name[MAX_NAME_LEN];
    int  cap_id;
};

static struct entry registry[MAX_NAMES];
static int count = 0;


int main()
{
    debug("Name server v0.0.1\n");
//    int ns_cap = ipc_endpoint_create(CRIGHT_RCV);
//    debug("Capabbility is created %d\n", ns_cap);
    for (;;) {
        ns_msg_t msg;
        int sender;
//        debug("NS srvr resv msg 0x%lX size %d\n", msg, sizeof(msg));
        int rpl_id = ipc_receive(0, &msg, sizeof(msg));
        sender = msg.pid;
        debug("[NAMESERVER] resved msg type %d cap id %d\n", msg.type, msg.cap_id);
        if (msg.type == NS_REGISTER) {
            if (count < MAX_NAMES) {
                
                strncpy(registry[count].name, msg.name, sizeof(registry[count].name)-1);
                debug("[NAMESERVER] received %s cap %d\n",msg.name, msg.cap_id);
                registry[count].cap_id = msg.cap_id;
                debug("[NAMESERVER] register %s cap %d\n", registry[count].name, registry[count].cap_id);
                count++;
                
                msg.type = NS_REPLAY;
                msg.cap_id = 0;
                int ret = ipc_replay(rpl_id, &msg, sizeof(msg));
//                debug("Replay ret %d\n", ret);
            } else {
                msg.type = NS_REPLAY;
                msg.cap_id = -1;
                ipc_replay(rpl_id, &msg, sizeof(msg));
            }

            
        } else if (msg.type == NS_LOOKUP) {
            int found = -1;
            debug("Looking for %s...\n", msg.name);
            for (int i=0; i < count; i++) {
                if (strcmp(registry[i].name, msg.name) == 0){
                    found = registry[i].cap_id;
                    debug("[NAMESERVER] found %s id %d\n", registry[i].name, registry[i].cap_id);
                    break;
                }
            }
            if (found) {
                if (sender <= 0)
                    return -EINVAL;
                // Grant capability to requster
                int g_cap = cap_grant(found, sender, -1 , CRIGHT_SND);
                if (g_cap < 0) {
                    debug("Error granting cap %d \n", g_cap);
                    for(;;);
                }
                found = g_cap;
            }
                  
            msg.type = NS_REPLAY;
            msg.cap_id = found;
            ipc_replay(rpl_id, &msg, sizeof(msg));
        }
       
    }
}
