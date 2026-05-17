#include <stdint.h>
#include <mosstd.h>
#include <string.h>
#include <sched.h>
#include <devman.h>


#include <libsys/ipc.h>
#include <nameserver.h>

int dm_cap = 0;

static int get_dm_cap()
{
    if (!dm_cap) {
        while(1) {
            dm_cap = ns_lookup_cap("devman"); 
            if (dm_cap > 0) break;
            sched_yield();  
        }        
    } 
    
    return dm_cap;
}

kerrno_t devman_register(const char *name, cap_id_t driver_cap)
{
    if (!driver_cap || !name) return -EINVAL;

    int pid = getpid();
    dm_msg_t *msg = (dm_msg_t *)get_ipc_buffer()->msg;
    
    // Grant capability to devman
    int ns_cap = cap_grant(driver_cap, pid, -1, CRIGHT_SND | CRIGHT_GRANT, 0);
    if (ns_cap < 0) {
        debug("Error granting cap %d \n", ns_cap);
        for(;;);
    }

    int cap_id = get_dm_cap();

    memset(msg, 0, sizeof(*msg));
    
    msg->type = DM_REGISTER;
    strncpy(msg->u.dm_register.name, name, sizeof(msg->u.dm_register.name)-1);
    ipc_set_cap(0, ns_cap);
//    msg->u.dm_register.driver_cap = ns_cap;
//    debug("[DEVMAN] send name %s cap %d\n", msg.u.dm_register.name, msg.u.dm_register.driver_cap);

    uint64_t info = msginfo_word_new(0, sizeof(dm_msg_t)/8, 1, 0);
    info = ipc_call(cap_id, info);

    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0 || !length_from_msginfo_word(info)) {
        debug("[DMLIB] Error registring cap %d info 0x%lX\n", ret, info);
    }

    return msg->u.dm_reply.err;    
}

kerrno_t devman_lookup(const char *name)
{    
    if (!name) return -EINVAL;

    int cap_id = get_dm_cap();
    dm_msg_t *msg = (dm_msg_t *)get_ipc_buffer()->msg;

    memset(msg, 0, sizeof(*msg));
    
    msg->type     = DM_LOOKUP;

    strncpy(msg->u.dm_lookup.name, name, sizeof(msg->u.dm_lookup.name)-1);
    msg->sender = getpid();

    uint64_t info = msginfo_word_new(0, sizeof(dm_msg_t)/8, 0, 0);
    info = ipc_call(cap_id, info);

    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0 || !length_from_msginfo_word(info)) {
        debug("[DMLIB] Error looking up cap %d info 0x%lX\n", ret, info);
    }    

    if (msg->u.dm_reply.err < 0) return -EINVAL;        
    return ipc_get_cap(0);
}