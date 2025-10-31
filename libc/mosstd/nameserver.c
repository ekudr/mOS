#include <stdint.h>
#include <mosstd.h>
#include <nameserver.h>
#include <errno.h>
#include <cap.h>
#include <ipc.h>
#include <string.h>

/*
 * register capability in nameserver
 * Return error
 */
kerrno_t ns_register_cap(int cap_id, char *name, uint32_t rights)
{
    ns_msg_t msg;
    msg.type = NS_REGISTER;
    strncpy(msg.name, name, MAX_NAME_LEN);

    // Grant capability to nameserver
    int ns_cap = cap_grant(cap_id, CAP_NS, -1 , rights);
    if (ns_cap < 0) {
        debug("Error granting cap %d \n", ns_cap);
        for(;;);
    }
    msg.cap_id = ns_cap;

    int ret = ipc_call(1, &msg, &msg, sizeof(ns_msg_t));
    if (ret < 0) {
        debug("Error registring cap %d \n", ret);
//        for(;;);
    }
    if (msg.type == NS_REPLAY)
        ret = msg.cap_id;
    return ret;
}

/*
 * Lookup capability in nameserver
 * Return cap_id or error < 0
 */
int ns_lookup_cap(char *name)
{
    ns_msg_t msg, reply;
    msg.pid  = getpid();
    msg.type = NS_LOOKUP;
    strncpy(msg.name, name, sizeof(name));


    int ret = ipc_call(1, &msg, &reply, sizeof(ns_msg_t));
    if (ret < 0) {
        debug("Error registring cap %d \n", ret);
    }
    if (reply.type == NS_REPLAY)
        ret = reply.cap_id;
 
    return ret;
}