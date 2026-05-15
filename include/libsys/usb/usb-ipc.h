#ifndef __LIBSYS_USB_IPC_H__
#define __LIBSYS_USB_IPC_H__

#include <stdint.h>

enum {
    IPC_SRC_HOST = 1,
    IPC_SRC_HUB  = 2,
    IPC_SRC_DEV  = 3,
};

enum {
    IPC_HOST_REGISTER = 1,
    IPC_DEVICE_CONNECT,
    IPC_NEW_DEVICE,     // for response
};


enum {
    IPC_HOST_CTRL_XFER = 1,
    IPC_HOST_NEW_DEVICE,
    IPC_HOST_ENABLE_SLOT,
    IPC_HOST_DISABLE_SLOT,
    IPC_HOST_ADDRESS_DEVICE,
    IPC_HOST_CONFIG_EP,
};

typedef struct usb_ep_desc {
    uint8_t  ep_num;
    uint8_t  ep_dir;
    uint8_t  type;
    uint16_t max_packet;
    uint8_t  interval;
} usb_ep_desc_t;


typedef struct usb_host_cmd {
    uint64_t cmd_type;
    union {
        struct {
            uint8_t  slot_id;
            uint8_t  setup_pkt[8];
            uint32_t data_len;
            uint64_t data_buf;
        } ctrl;
        struct {
            uint8_t  slot_id;
            uint8_t  ep_nums;
            usb_ep_desc_t ep_desc[1];
        } config_ep;
        struct {
            uint8_t  host_id;
            uint8_t  slot_id;
            uint8_t  address;
            uint8_t  speed;
            uint8_t  port;
            uint8_t  state;
            uint16_t max_packege;
        } new_dev;
    };

} usb_host_cmd_t;


typedef struct usb_core_cmd {
    uint64_t src;
    uint64_t cmd_type;
    union {
        struct {
            uint8_t  host_id;
            uint8_t  slot_id;
            uint8_t  address;
            uint8_t  speed;
            uint8_t  port;
            uint8_t  state;
        } new_dev;
    };
} usb_core_cmd_t;

typedef struct usb_host_signal
{
    union
    {
        struct
        {
            uint8_t host_id;
            uint8_t port_id;
            uint8_t speed;
            uint8_t state;
            uint32_t padding;
        };
        uint64_t signal;
    };
    
} __attribute__((packed)) usb_host_signal_t;

_Static_assert((sizeof(usb_host_signal_t) == 8), "size of usb_host_signal");


#endif /* __LIBSYS_USB_IPC_H__ */