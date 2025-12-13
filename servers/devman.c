#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <ipc.h>
#include <libsys/ipc.h>
#include <common.h>
//#include <memory.h>
#include <string.h>
#include <nameserver.h>
#include <libsys/cap.h>
#include <mosstd.h>

#include <syscall.h>

#include <devman.h>


struct dev_entry {
    char     name[32];
    cap_id_t driver_cap;
};

static struct dev_entry devices[DM_MAX_DEVS];
static int dev_count = 0;

//uint64_t qkey = 0x0152474E4D564544;
//uint64_t qid;

int dm_cap;
//dm_device_t *device_root;

static  kerrno_t dm_do_register(const struct dm_register *req)
{
    if (dev_count >= DM_MAX_DEVS) return -ENOSPC;
//    debug("[DEVMEN] register driver %s cap %d\n", req->name, req->driver_cap);
    strncpy(devices[dev_count].name, req->name, sizeof(devices[dev_count].name));
    devices[dev_count].driver_cap = req->driver_cap;
    dev_count++;
    return SUCCESS;
}

static cap_id_t dm_do_lookup(const char *name)
{
    for (int i = 0; i < dev_count; i++) {
        if (strcmp(devices[i].name, name) == 0)
            return devices[i].driver_cap;
    }
    return -EINVAL;
}
/*
void device_root_init(void)
{  
    device_root = (dm_device_t *)malloc(sizeof(dm_device_t));
    
    device_root->type = D_ROOT;
    list_init(&device_root->devlist);
    list_init(&device_root->childlist);
    strcpy(device_root->name, "root\0");
}

int dev_add(dm_device_t *dev)
{
    list_add_tail(&device_root->devlist, &dev->devlist);
    return 0;
}

int device_register(dm_device_t *dev)
{
    dev_add(dev);

//    free(dev);
    return SUCCESS;
}

inline dm_device_t *get_dev(dm_msg_t *msg)
{
    dm_device_t *d  = malloc(sizeof(dm_device_t));
    memcpy(d, &msg->msg.device, sizeof(dm_device_t));
    return d;
}

void reply_status(int replay, int status)
{
    dm_msg_t resp;
    if (status != SUCCESS)
    {
        resp.type     = DM_RESPONSE;
        resp.sender   = 1;
        resp.msg.resp = status;
    } else {
        resp.type     = DM_RESPONSE;
        resp.sender   = 1;
        resp.msg.resp = DM_SUCCESS;
    }

    ipc_replay(replay, &resp, sizeof(resp));
}

int device_lookup_bby_type(dm_device_type_t type)
{
    dm_device_t *dev;
    list_for_each_entry(dev, &device_root->devlist, devlist) {
        if (dev->type == type) return SUCCESS;
    }
}
*/
int init_server()
{
    // Create Endpoint for capability
    dm_cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    if (dm_cap < 0) {
        debug("PANIC DEVMAN");
        for (;;);
    }     

    // ??? Create copy endpoint for share
    int ret = ns_register_cap(dm_cap, "devman", CRIGHT_SND | CRIGHT_GRANT);
    if (ret < 0) {
        debug("PANIC DEVMAN");
        for (;;);
    }  
    return SUCCESS;  
}

int main()
{
    debug("Device manager ver. 0.0.1\n");


    // Init server
    init_server();
           
    while (1)
    {
        dm_msg_t *msg = (dm_msg_t *)get_ipc_buffer()->msg;
        uint64_t info = ipc_recv(dm_cap, NULL);
        int ret = (int)label_from_msginfo_word(info);
        if (ret < 0 || !length_from_msginfo_word(info)) {
            debug("[DEVMAN] Error receiving message %d info 0x%lX\n", ret, info);
        }
        
//        debug("Message received from %d type %d\n", msg.sender, msg.type);
        uint32_t type =msg->type;
//        uint64_t sender = msg->sender;
        msg->u.dm_register.driver_cap = ipc_get_cap(0);

        switch (type)
        {
        case DM_REGISTER:
            const struct dm_register *r = &msg->u.dm_register;
            int err = dm_do_register(r);
            memset(msg, 0, sizeof(dm_msg_t));
            msg->u.dm_reply.err = err;
            msg->type = type;
//            debug("[DEVMAN] return %d\n",reply.u.dm_replay.err);
            info = msginfo_word_new(0,sizeof(dm_msg_t)/8, 0, 0);
            ipc_reply(info);
            break;
        
        case DM_LOOKUP:
            const struct dm_lookup *l = &msg->u.dm_lookup;
            cap_id_t drv = dm_do_lookup(l->name);
            memset(msg, 0, sizeof(dm_msg_t));
            if (drv == -EINVAL){
                msg->u.dm_reply.err = -1;
                info = msginfo_word_new(0,sizeof(dm_msg_t)/8, 0, 0);
            } else {
                msg->u.dm_reply.err = 0;
                // int g_drv = cap_grant(drv, sender, -1 , CRIGHT_SND);
                // if (g_drv < 0) msg->u.dm_reply.err = g_drv;   
                ipc_set_cap(0, drv);
                msg->u.dm_reply.driver_cap = drv;
                info = msginfo_word_new(0,sizeof(dm_msg_t)/8, 1, 0);
            }
            
            msg->type = type;
            
            ipc_reply(info);

        default:
            memset(msg, 0, sizeof(dm_msg_t));
            msg->u.dm_reply.err = -1;
            msg->type = type;
            info = msginfo_word_new(0,sizeof(dm_msg_t)/8, 0, 0);
            ipc_reply(info);
            break;
        }     
                    
    } 
}