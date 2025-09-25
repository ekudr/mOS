#include <stdint.h>
#include <cap.h>
#include <syscall.h>
#include <libsys/syscall.h>

/*
 *  Create capability
 */ 
int create_capability(cap_type_t type, cap_rights_t rights)
{
    return __syscall(SYS_cap_crt, (uint64_t)type, (uint64_t)rights);
}


