#ifndef __NAMESERVER_H__
#define __NAMESERVER_H__

#define MAX_NAME_LEN 32

#include <errno.h>

enum {
    NS_REGISTER = 1,
    NS_LOOKUP,
    NS_REPLAY,
};


typedef struct ns_msg
{
    uint32_t type;
    char     name[MAX_NAME_LEN];
    int      cap_id;
    uint64_t pid;
} ns_msg_t;

kerrno_t ns_register_cap(int cap_id, char *name, uint32_t rights);
int ns_lookup_cap(char *name);

#endif /* __NAMESERVER_H__ */