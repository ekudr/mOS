#ifndef __LIBSYS_USB_IPC_H__
#define __LIBSYS_USB_IPC_H__

#include <stdint.h>

enum {
    IPC_SRC_HOST  = 1,
    IPC_SRC_CLASS = 2,   // any class driver (hub, hid, msc, …)
    IPC_SRC_DEV   = 3,
};

enum usb_class_id {
    USB_CLASS_ID_HUB = 1,
    USB_CLASS_ID_HID = 2,
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
    IPC_HOST_UPDATE_EP0_MPS,
    IPC_HOST_STOP_EP,
    IPC_HOST_XFER_SUBMIT,
    IPC_HOST_REGISTER_CLIENT   = 10,
    IPC_HOST_UNREGISTER_CLIENT = 11,
    IPC_XFER_DIRECT_SUBMIT     = 20,
};

// Class driver ↔ usb-core IPC commands (cmd_type field).
// Pull-based to avoid mutual ipc_call deadlock.
enum {
    IPC_CLASS_BIND            = 1,  // unused — kept for ABI stability
    IPC_CLASS_PORT_CONNECT    = 2,  // hub-only: child device connected
    IPC_CLASS_PORT_DISCONNECT = 3,  // hub-only: child device removed
    IPC_CLASS_DEV_CTRL_XFER   = 4,  // class→usb-core: proxy ctrl xfer
    IPC_CLASS_DEV_XFER_SUBMIT = 5,  // class→usb-core: proxy bulk/int xfer
    IPC_CLASS_POLL_BIND       = 6,  // class→usb-core: pull next pending bind
};

typedef struct urb_result {
    volatile uint8_t  valid;
    int8_t            status;
    uint16_t          _pad;
    uint32_t          actual;
} urb_result_t;

#define URB_TABLE_MAX_SLOTS  64
#define URB_TABLE_MAX_EPS    32
#define URB_TABLE_SIZE       (URB_TABLE_MAX_SLOTS * URB_TABLE_MAX_EPS * sizeof(urb_result_t))
#define URB_RES_IDX(slot, ep) ((slot) * URB_TABLE_MAX_EPS + (ep))

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
            uint8_t  parent_slot;   /* 0 = root-attached */
            uint8_t  tt_slot;       /* TT hub slot (LS/FS through HS hub); 0 = none */
            uint8_t  hub_port;      /* downstream port on parent hub (1-based) */
            uint8_t  _pad;
            uint32_t route_string;
        } new_dev;
        struct {
            uint8_t  slot_id;
            uint16_t mps;
        } update_ep0_mps;
        struct {
            uint8_t  slot_id;
            uint8_t  ep_id;
        } stop_ep;
        struct {
            uint8_t  slot_id;
            uint8_t  ep_id;   /* xHCI ep_id: EP0=1, EP_N_OUT=N*2, EP_N_IN=N*2+1 */
            uint8_t  dir;     /* USB_DIR_IN or USB_DIR_OUT */
            uint32_t len;
        } xfer;
        struct {
            uint8_t  slot_id;
            uint8_t  _pad[7];
            uint64_t notif_cap;
        } register_client;
        struct {
            uint8_t  slot_id;
            uint8_t  ep_id;
            uint8_t  dir;
            uint8_t  _pad;
            uint32_t len;
            uint32_t offset;
        } direct_xfer;
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

// Class driver ↔ usb-core IPC message.
// MR[0] = src (IPC_SRC_CLASS), MR[1] = cmd_type, rest = union payload.
typedef struct usb_class_cmd {
    uint64_t src;       // IPC_SRC_CLASS
    uint64_t cmd_type;
    union {
        struct {
            uint8_t  class_id;  // USB_CLASS_ID_* filter for IPC_CLASS_POLL_BIND
        } poll_bind;
        struct {
            uint8_t  slot_id;
            uint8_t  hub_port;      // downstream port (1-based)
            uint8_t  speed;
            uint8_t  _pad;
            uint32_t route_string;  // pre-computed child route string
        } port_connect;
        struct {
            uint8_t  slot_id;
            uint8_t  hub_port;
        } port_disconnect;
        struct {
            uint8_t  slot_id;
            uint8_t  host_id;
            uint8_t  setup_pkt[8];
            uint32_t data_len;
        } ctrl;
        struct {
            uint8_t  slot_id;
            uint8_t  host_id;
            uint8_t  ep_id;
            uint8_t  dir;
            uint32_t len;
        } xfer;
    };
} __attribute__((packed)) usb_class_cmd_t;

enum {
    USB_PORT_STATE_DISCONNECTED = 0,
    USB_PORT_STATE_CONNECTED    = 1,
    USB_PORT_STATE_ENABLED      = 2,
    USB_PORT_STATE_SUSPENDED    = 3,
};

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