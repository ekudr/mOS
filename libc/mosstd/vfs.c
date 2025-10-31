#include <stdint.h>
#include <mosstd.h>
#include <string.h>
#include <errno.h>

#include <vfs.h>

#include <cap.h>
#include <ipc.h>
#include <nameserver.h>

int vfs_cap = 0;

static int get_vfs_cap()
{
    if (!vfs_cap) {
        do {
          vfs_cap = ns_lookup_cap("vfs");  
        } while (vfs_cap <= 0);        
    } 

    return vfs_cap;
}
/*
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
    int res = ipc_call(get_dm_cap(), &msg, &reply, sizeof(msg));
//    debug("[IPC_CALL] returned %d result %d\n", res, reply.u.dm_reply.err);
    return reply.u.dm_reply.err;    
}
*/
int vfs_open(const char *path)
{
    struct vfs_msg msg, reply;
    if (!path) return -EINVAL;

    memset(&msg, 0, sizeof(msg));
    
    msg.type    = VFS_OPEN;
    msg.pid     = getpid();

    strncpy(msg.path, path, sizeof(msg.path));
//    msg.sender = getpid();
    ipc_call(get_vfs_cap(), &msg, &reply, sizeof(msg));

    if (reply.ret < 0) return -EINVAL;        
    return reply.ret;
}

int vfs_create(const char *path, inode_type_t type, int cap_id)
{
    struct vfs_msg msg, reply;
    if (!path || !cap_id) return -EINVAL;

    int tr_cap = cap_transfer(cap_id, get_vfs_cap(), CRIGHT_SND | CRIGHT_GRANT);
    if (tr_cap < 0) return tr_cap;

    memset(&msg, 0, sizeof(msg));    
    msg.type    = VFS_CREATE;
    msg.cap_id  = tr_cap;
    msg.inode_type = type;
    strncpy(msg.path, path, sizeof(msg.path));

    ipc_call(get_vfs_cap(), &msg, &reply, sizeof(msg));
    
//    if (reply.ret < 0) return -EINVAL;        
    return reply.ret;
}