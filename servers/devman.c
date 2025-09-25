#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <ipc.h>
#include <libsys/ipc.h>
#include <common.h>
#include <memory.h>
#include <string.h>
#include <nameserver.h>
#include <cap.h>
#include <mosstd.h>

#include <syscall.h>

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
    ns_msg_t nsmsg;

    debug("Device manager ver. 0.0.1\n");

    // int my_cap = ipc_endpoint_create(CRIGHT_SND | CRIGHT_SND | CRIGHT_GRANT);
    // nsmsg.type = NS_REGISTER;
    // strcpy(nsmsg.name, "devman");

    // int ns_cap = cap_grant(my_cap, 1, -1 ,CRIGHT_SND | CRIGHT_GRANT);
    // if (ns_cap < 0) {
    //     debug("Error granting cap %d \n", ns_cap);
    //     for(;;);
    // }
    // nsmsg.cap_id = ns_cap;
    // debug("[DEVMAN] Cap transferred to %d\n", nsmsg.cap_id);
    // int ret = ipc_send(1, &nsmsg, sizeof(ns_msg_t));
    // if (ret < 0) {
    //     debug("Error registring cap %d \n", ret);
    //     for(;;);
    // }

    // Create Endpoint for capability
    int cap = ipc_endpoint_create(CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    if (cap < 0)
        return cap;

    ns_register_cap(cap, "devman", CRIGHT_SND | CRIGHT_GRANT);

    device_root_init();

    qid = get_msg(qkey, 0);


    debug("Size of msg 0x%lX\n", sizeof(dm_msg_t));
    msg = (dm_msg_t *) malloc(sizeof(dm_msg_t));

    if(msg == NULL)
    {
        debug("ERROR: memory allocation\n");
        for(;;);
    }

    uint64_t a,b,c,d,e,f;
    int r =  __fast_call_7(SYS_fast_call,1, 2, 3, 4, 5, 6, 7, &a, &b, &c, &d, &e, &f);
    debug("[DEVMAN] ipc call returned r 0x%lX a 0x%lX b 0x%lX c 0x%lX d 0x%lX e 0x%lX f 0x%lX\n",
            r,a,b,c,d,e,f);
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