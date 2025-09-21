#ifndef __DEVMAN_QMSG_H__
#define __DEVMAN_QMSG_H__

#include <stdint.h>
#include <list.h>

// Message types
typedef enum {
    DM_REGISTER_DEVICE = 1,
    DM_FIND_DEVICE_BY_NAME,
    DM_FIND_DEVICE_BY_TYPE,
    DM_RESPONSE,
} dm_msg_type_t;

typedef enum {
    D_ROOT = 1,
    D_TTY,
    D_BLOCK,
} dm_device_type_t;

typedef enum {
    DM_SUCCESS,
    DM_FAIL,
} dm_msg_rsp_r;

typedef struct 
{
    uint64_t portno;
    uint64_t qid;
    

 } dm_port_t;

 typedef struct dm_device
 {
    dm_device_type_t type;
    char name[16];
    dm_port_t *port;
    list_head_t devlist;
    list_head_t childlist;
 } dm_device_t;

typedef struct dm_msg
{
    dm_msg_type_t   type;
    uint64_t        sender;
    union
    {
        dm_device_t     device;
        dm_device_type_t type;
        dm_msg_rsp_r    resp;
    } msg;
     
} dm_msg_t;

#endif /* __DEVMAN_QMSG_H__ */
