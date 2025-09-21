
#ifndef _SYSCALL_H
#define _SYSCALL_H

// System call numbers
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_pipe    4
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_fstat   8
#define SYS_chdir   9
#define SYS_dup    10
#define SYS_getpid 11
#define SYS_sbrk   12
#define SYS_sleep  13
#define SYS_uptime 14
#define SYS_open   15
#define SYS_write  16
#define SYS_mknod  17
#define SYS_unlink 18
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21


#define SYS_debug   22
#define SYS_mmap    23

#define SYS_get_msg 30
#define SYS_snd_msg 31
#define SYS_rcv_msg 32

#define SYS_shm_get 33
#define SYS_shm_att 34
#define SYS_shm_det 35
#define SYS_shm_ctl 36

#define SYS_sig_act 40
#define SYS_sig_snd 41
#define SYS_sig_ret 42

#define SYS_irq_set 50
#define SYS_irq_act 51



#endif /* _SYSCALL_H */