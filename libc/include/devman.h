#ifndef __DEVMAN_H__
#define __DEVMAN_H__

#include <cap.h>
#include <libsys/list.h>

#define DM_MAX_DEVS 32

// Message types
enum dm_cmd{
    DM_REGISTER = 1,
    DM_LOOKUP,

};
/*
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
*/

struct dm_register {
    char     name[32];
    cap_id_t driver_cap;
};

struct dm_lookup {
    char name[32];
};

struct dm_reply {
    int err;
    cap_id_t driver_cap;
};

typedef struct dm_msg
{
    uint32_t   type;
    uint64_t   sender;
    union
    {
        struct dm_register  dm_register;
        struct dm_lookup    dm_lookup;
        struct dm_reply    dm_reply;
    } u;
     
} dm_msg_t;

kerrno_t devman_register(const char *name, cap_id_t driver_cap);
kerrno_t devman_lookup(const char *name);

#endif /* __DEVMAN_H__ */