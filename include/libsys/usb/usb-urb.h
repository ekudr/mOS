#ifndef __LIBSYS_USB_URB_H__
#define __LIBSYS_USB_URB_H__

#include <stdint.h>
#include <libsys/usb/usb-ipc.h>

typedef int cap_id_t;

typedef struct urb {
    uint8_t          slot_id;
    uint8_t          ep_id;
    uint8_t          dir;
    volatile uint8_t done;
    int8_t           status;
    uint8_t          _pad[3];
    uint32_t         actual;
    uint32_t         offset;
    uint32_t         len;
    struct urb      *next;
} urb_t;

void urb_subsystem_init(cap_id_t xhci_xfer_cap, cap_id_t notif_cap,
                        void *data_shm, urb_result_t *result_table);
int  urb_submit(urb_t *u);
int  urb_wait(urb_t *u);

#endif /* __LIBSYS_USB_URB_H__ */
