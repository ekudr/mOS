#ifndef __USB3_MAIN_H__
#define __USB3_MAIN_H__

extern int usb_core_cap;
extern void *buf;
extern size_t buf_size;
extern int usb_host_id;
extern pid_t usb_core_pid;

void panic(const char *str);

void handle_ctrl_transfer_request(uint64_t sender, uint64_t info);
void handle_config_ep(uint64_t sender, uint64_t info);
void handle_enable_slot(uint64_t sender, uint64_t info);
void handle_new_device(uint64_t sender, uint64_t info);
void handle_address_device(uint64_t sender, uint64_t info);
void handle_update_ep0_mps(uint64_t sender, uint64_t info);
void handle_disable_slot(uint64_t sender, uint64_t info);
void handle_stop_ep(uint64_t sender, uint64_t info);
void handle_xfer_submit(uint64_t sender, uint64_t info);

#endif /* __USB3_MAIN_H__ */