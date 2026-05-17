#include <stdint.h>
#include <mosstd.h>
#include <libsys/ipc.h>
#include <nameserver.h>
#include <errno.h>
#include <libsys/cap.h>
#include <ipc.h>
#include <string.h>

/*
 * register capability in nameserver
 * Return error
 */
kerrno_t ns_register_cap(int cap_id, char *name, uint32_t rights)
{
    int pid = getpid();
    ns_msg_t *msg = (ns_msg_t *)get_ipc_buffer()->msg;

    memset(msg, 0, sizeof(ns_msg_t));
    msg->type = NS_REGISTER;
    strncpy(msg->name, name, sizeof(msg->name)-1);

    // Copy cap to nameserver
    int ns_cap = cap_grant(cap_id, pid, -1, rights, 0);
    if (ns_cap < 0) {
        debug("Error granting cap %d \n", ns_cap);
        for(;;);
    }
//    debug("[NSLIB] Granted cap %d \n", ns_cap);
    // int ns_cap = cap_grant(cap_id, CAP_NS, -1 , rights);
    // if (ns_cap < 0) {
    //     debug("Error granting cap %d \n", ns_cap);
    //     for(;;);
    // }
    // msg->cap_id = ns_cap;

    ipc_set_cap(0, ns_cap);

    uint64_t info = msginfo_word_new(0, sizeof(ns_msg_t)/8, 1, 0);
    info = ipc_call(0x100, info);

    int ret = (int)label_from_msginfo_word(info);
    if (ret < 0 || !length_from_msginfo_word(info)) {
        debug("Error registring cap %d info 0x%lX\n", ret, info);
    }
    if (msg->type == NS_REPLAY)
        ret = msg->cap_id;
    return ret;
}

/*
 * Lookup capability in nameserver
 * Return cap_id or error < 0
 */
int ns_lookup_cap(char *name)
{
    
    ns_msg_t *msg = (ns_msg_t *)get_ipc_buffer()->msg;

    msg->pid  = getpid();
    msg->type = NS_LOOKUP;
    strncpy(msg->name, name, sizeof(msg->name)-1);

    uint64_t info = msginfo_word_new(0, sizeof(ns_msg_t)/8, 0, 0);
    info = ipc_call(0x100, info);

    int ret = (int)label_from_msginfo_word(info);
//        debug("\x1b[31m[NSLIB]\x1b[0m lookup ret %d info 0x%lX\n", ret, info);
    if (ret < 0 || !length_from_msginfo_word(info)) {
        debug("Error registring cap %d \n", ret);
    }

    ret = msg->cap_id;
    if (msg->type == NS_REPLAY && msg->cap_id > 0)
        ret = ipc_get_cap(0);
 
    return ret;
}