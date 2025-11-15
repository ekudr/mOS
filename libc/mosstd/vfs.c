#include <stdint.h>
#include <mosstd.h>
#include <string.h>
#include <errno.h>

#include <vfs.h>

#include <cap.h>
#include <libsys/ipc.h>
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
    if (!path) return -EINVAL;

    int cap_id = get_vfs_cap();

    struct vfs_msg *msg = (struct vfs_msg *)get_ipc_buffer()->msg;
    
    memset(msg, 0, sizeof(struct vfs_msg));
    
    msg->type    = VFS_OPEN;
    msg->pid     = getpid();

    strncpy(msg->path, path, sizeof(msg->path)-1);

    uint64_t info = msginfo_word_new(0, sizeof(struct vfs_msg)/8, 0, 0);


    info = ipc_call(cap_id, info);
    
    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0 || !length_from_msginfo_word(info)) {
        debug("[VFS] Error open node %d \n", ret);
    }

    if (msg->ret < 0) return -EINVAL;        
    return msg->ret;
}

int vfs_create(const char *path, inode_type_t type, int cap_id)
{
    if (!path || !cap_id) return -EINVAL;
    struct vfs_msg *msg = (struct vfs_msg *)get_ipc_buffer()->msg;

    int vfsid = get_vfs_cap();


    int tr_cap = cap_transfer(cap_id, vfsid, CRIGHT_SND | CRIGHT_GRANT);
    if (tr_cap < 0) return tr_cap;

    memset(msg, 0, sizeof(struct vfs_msg));    
    msg->type    = VFS_CREATE;
    msg->cap_id  = tr_cap;
    msg->inode_type = type;
    strncpy(msg->path, path, sizeof(msg->path)-1);

    uint64_t info = msginfo_word_new(0, sizeof(struct vfs_msg)/8, 0, 0);
    info = ipc_call(vfsid, info);
    
    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0 || !length_from_msginfo_word(info)) {
        debug("[VFS] Error creating node %d \n", ret);
    }

    return msg->ret;
}
