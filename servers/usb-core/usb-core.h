#ifndef __USB_CORE_H__
#define __USB_CORE_H__

#include <stdint.h>


/**
 * Panic function - prints error and halts
 * @param str Error message to display
 */
void panic(const char *str);

/**
 * Handle IPC messages from host controllers
 * @param sender Sender capability
 * @param info IPC message info
 */
void handle_host_ipc(uint64_t sender, uint64_t info);

/**
 * Handle host controller registration
 * @param sender Sender capability
 * @param info IPC message info
 */
void handle_host_register(uint64_t sender, uint64_t info);

/**
 * Handle IPC messages from USB devices
 * @param sender Sender capability
 * @param info IPC message info
 */
void handle_dev_ipc(uint64_t sender, uint64_t info);

int usb_core_init();

/**
 * Handle USB events such as port changes and device additions/removals
 */
void handle_usb_events(void);

/**
 * Handle IPC messages from class drivers (hub, hid, msc, …)
 * @param sender Sender capability
 * @param info IPC message info
 */
void handle_class_ipc(uint64_t sender, uint64_t info);


#endif /* __USB_CORE_H__ */
