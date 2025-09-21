#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <ipc.h>
#include <common.h>
#include <memory.h>
#include <string.h>

#include "qmsg.h"

uint64_t qkey = 0x0152474E4D564544;
uint64_t qid;


dm_device_t *device_root;

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

    free(dev);
    return -1;
}

inline dm_device_t *get_dev(dm_msg_t *msg)
{
    dm_device_t *d  = malloc(sizeof(dm_device_t));
    memcpy(d, &msg->msg.device, sizeof(dm_device_t));
    return d;
}

void reply_status(uint64_t receiver, int status)
{
    dm_msg_t *resp = malloc(sizeof(dm_msg_t));
    if (status != SUCCESS)
    {
        resp->type     = DM_RESPONSE;
        resp->sender   = 1;
        resp->msg.resp = DM_FAIL;
    } else {
        resp->type     = DM_RESPONSE;
        resp->sender   = 1;
        resp->msg.resp = DM_SUCCESS;
    }

    snd_msg(qid, receiver, (uintptr_t)resp, sizeof(dm_msg_t),0);

    free(resp);
}

int main()
{
    dm_msg_t *msg;

    debug("Device manager ver. 0.0.1\n");

    device_root_init();

    qid = get_msg(qkey, 0);


    debug("Size of msg 0x%lX\n", sizeof(dm_msg_t));
    msg = (dm_msg_t *) malloc(sizeof(dm_msg_t));

    if(msg == NULL)
    {
        debug("ERROR: memory allocation\n");
        for(;;);
    }
        

    while (1)
    {

        if(rcv_msg(qid, 1, (uintptr_t)msg, sizeof(dm_msg_t), 0)){
            debug("Message received ... ");
            uint64_t sender = msg->sender;
            switch (msg->type)
            {
            case DM_REGISTER_DEVICE:
                int st = device_register(get_dev(msg));
                reply_status(msg->sender, st);
                break;
            
            default:
                break;
            }     
             
        }

    }
 //        free(msg);   
}