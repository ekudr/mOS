#include <mosstd.h>
#include <syscall.h>
#include <libsys/syscall.h>

pid_t getpid(void)
{
	return __syscall(SYS_getpid);
}