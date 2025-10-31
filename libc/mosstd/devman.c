#include <stdint.h>
#include <mosstd.h>
#include <string.h>
#include <errno.h>

#include <devman.h>


#include <ipc.h>
#include <nameserver.h>

int dm_cap = 0;

static int get_dm_cap()
{
    if (!dm_cap) {
        do {
          dm_cap = ns_lookup_cap("devman");  
        } while (dm_cap <= 0);        
    } 

    return dm_cap;
}

kerrno_t devman_register(const char *name, cap_id_t driver_cap)
{
    dm_msg_t msg, reply;
    if (!driver_cap || !name) return -EINVAL;

    // Grant capability to devman
    int ns_cap = cap_transfer(driver_cap, get_dm_cap(), CRIGHT_SND | CRIGHT_GRANT);
    if (ns_cap < 0) {
        debug("Error granting cap %d \n", ns_cap);
        for(;;);
    }

    memset(&msg, 0, sizeof(msg));
    
    msg.type = DM_REGISTER;
    strncpy(msg.u.dm_register.name, name, sizeof(msg.u.dm_register.name));
    msg.u.dm_register.driver_cap = ns_cap;
    debug("[DEVMAN] send name %s cap %d\n", msg.u.dm_register.name, msg.u.dm_register.driver_cap);
    ipc_call(get_dm_cap(), &msg, &reply, sizeof(msg));

    return reply.u.dm_reply.err;    
}

kerrno_t devman_lookup(const char *name)
{
    dm_msg_t msg, reply;
    if (!name) return -EINVAL;

    memset(&msg, 0, sizeof(msg));
    
    msg.type     = DM_LOOKUP;

    strncpy(msg.u.dm_lookup.name, name, sizeof(msg.u.dm_lookup.name));
    msg.sender = getpid();
    ipc_call(get_dm_cap(), &msg, &reply, sizeof(msg));

    if (reply.u.dm_reply.err < 0) return -EINVAL;        
    return reply.u.dm_reply.driver_cap;
}