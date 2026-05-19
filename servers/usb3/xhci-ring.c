#include <libsys/common.h>
#include <string.h>
#include <libsys/timer.h>
#include <libsys/riscv.h>
#include <libsys/memory.h>
#include <cap.h>
#include <signals.h>
#include <libsys/barrier.h>
#include <sched.h>

#include "xhci.h"
#include "dma-pool.h"
#include <libsys/usb/usb.h>
#include <libsys/usb/usb-ipc.h>


static inline uint8_t xhci_ep_id(uint8_t ep_num, uint8_t ep_dir)
{
    if (ep_num == 0)
        return 1; // Endpoint 0 is always ID 1
    return (ep_num * 2) + ((ep_dir == USB_DIR_IN) ? 1 : 0 );
}

uint8_t xhci_get_endpoint_type_from_ep_descriptor(const usb_ep_desc_t *desc) 
{
    switch (desc->type) {
        case 0: {
            return CTRL_EP;
        }
        case 1: {
            return desc->ep_dir ? ISOC_IN_EP : ISOC_OUT_EP;
        }
        case 2: {
            return desc->ep_dir ? BULK_IN_EP : BULK_OUT_EP;
        }
        case 3: {
            return desc->ep_dir ? INT_IN_EP : INT_OUT_EP;
        }
        default: break;
    }

    return 0;
}

uint8_t xhci_enable_device_slot(void) 
{
    xhci_trb_t enable_slot_trb;
    memset(&enable_slot_trb, 0, sizeof(xhci_trb_t));

    enable_slot_trb.trb_type = TRB_ENABLE_SLOT;

    xhci_event_cmd_t *completion_trb = xhci_send_command(xhci_dev, &enable_slot_trb, 200);
    if (!completion_trb) {
        return 0;
    }

    if (GET_COMP_CODE(completion_trb->status) != COMP_SUCCESS) {
        debug("[XHCI] Enable slot failed with code %u\n", GET_COMP_CODE(completion_trb->status));
        return 0;
    }

//    debug("[XHCI] Device slot enabled: slot_id=%u status=0x%x\n", completion_trb->slot_id, GET_COMP_CODE(completion_trb->status));
    
    // Ring doorbell to notify controller of slot enable
//    _ring_doorbell(xhci_dev, completion_trb->slot_id, 0);
    
    return completion_trb->slot_id;
}

int xhci_configure_endpoint(uint8_t slot_id) 
{
    xhci_virt_dev_t *dev = xhci_dev->devs[slot_id];
    if (!dev) {
        debug("[XHCI] No device context for slot %u\n", slot_id);
        return -EINVAL;
    }

    xhci_trb_t config_ep_trb;
    memset(&config_ep_trb, 0, sizeof(xhci_trb_t)); 
            
    // Flush the input context to DMA memory
    wmb();
    dma_cache_flush(xhci_dev->xmem_pool, dev->in_ctx, 33 * 64); // 33 contexts of 64 bytes each

    // Parameter: Input Context Pointer
    config_ep_trb.parameter = dma_get_phys(xhci_dev->xmem_pool, dev->in_ctx);
    // Control: Slot ID, Endpoint Index and TRB Type
    config_ep_trb.control = SLOT_ID_FOR_TRB(slot_id) | TRB_TYPE(TRB_CONFIG_EP);

    xhci_event_cmd_t *completion_trb = xhci_send_command(xhci_dev, &config_ep_trb, 200);
    if (!completion_trb) {
        debug("[XHCI] Configure endpoint command failed for slot %u\n", slot_id);
        return -EIO;
    }
    if (GET_COMP_CODE(completion_trb->status) != COMP_SUCCESS) {
        debug("[XHCI] Configure endpoint failed with code %u for slot %u\n", 
              GET_COMP_CODE(completion_trb->status), slot_id);
        return -EIO;
    }
    debug("[XHCI] Endpoint configured successfully on slot %u\n", completion_trb->slot_id);   

    return SUCCESS;
}

int xhci_control_transfer(xhci_t *xhci, uint8_t slot_id, usb_control_request_t *req, 
                         void *data, size_t data_len)
{
    if (!xhci || !req || !slot_id || slot_id > xhci->max_slots) {
        return -EINVAL;
    }

    if (data_len > 0 && !data) {
        return -EINVAL;
    }
    size_t buffer_size = data_len > 0 ? data_len : 0;
    bool data_in = (req->bmRequestType & USB_DIR_IN) != 0;

    void *dma_buffer = NULL;
    if (buffer_size > 0) {
        // Allocate DMA buffer for the descriptor
        dma_buffer = dma_alloc(xhci->xmem_pool, buffer_size, 64);
        if (!dma_buffer) {
            debug("[XHCI] Failed to allocate DMA buffer for descriptor\n");
            return -ENOMEM;
        }

        wmb();
        dma_cache_flush(xhci->xmem_pool, dma_buffer, buffer_size);
    }
    // For now, only support EP0 control transfers
    int ret = xhci_ep0_control_transfer(xhci, slot_id, req, dma_buffer, buffer_size, data_in);

    if (ret != SUCCESS) {
        debug("\x1b[31m[xhci]\x1b[0m Transfer failed %u \n", ret); 
        if (dma_buffer) dma_free(xhci->xmem_pool, dma_buffer);
        return ret;
    }

    if (dma_buffer) {
        rmb();

        // Invalidate cache to ensure we read the latest data
        dma_cache_invalidate(xhci->xmem_pool, dma_buffer, buffer_size);   

        // Copy data from DMA buffer to user buffer 
        memcpy(data, dma_buffer, buffer_size);
        dma_free(xhci->xmem_pool, dma_buffer);
    }

    return SUCCESS;
}

/* Maximum bytes per Normal TRB (17-bit TRB_LEN field capped at 64 KiB) */
#define XFER_TRB_MAX_LEN    65536

int xhci_submit_bulk_transfer(xhci_t *xhci, uint8_t slot_id, uint8_t ep_id,
                               void *buf, uint32_t len, bool data_in,
                               uint32_t *out_residual)
{
    if (!xhci || !slot_id || ep_id == 0) return -EINVAL;

    xhci_virt_dev_t *dev = xhci->devs[slot_id];
    if (!dev) return -EINVAL;

    xhci_virt_ep_t *ep = dev->eps[ep_id - 1];
    if (!ep || !ep->tr_ring) return -EINVAL;

    xhci_ring_t *ring = ep->tr_ring;
    paddr_t buf_phys = dma_get_phys(xhci->xmem_pool, buf);

    uint32_t remaining = len;
    uint32_t offset    = 0;

    while (remaining > 0) {
        uint32_t trb_len = remaining > XFER_TRB_MAX_LEN ? XFER_TRB_MAX_LEN : remaining;
        bool     is_last = (remaining - trb_len == 0);

        xhci_trb_t *trb = &ring->trbs[ring->enqueue_ptr];
        memset(trb, 0, sizeof(xhci_trb_t));

        trb->parameter = buf_phys + offset;
        trb->status    = TRB_LEN(trb_len) | TRB_INTR_TARGET(0); /* TD_SIZE=0 */

        uint32_t ctrl = TRB_TYPE(TRB_NORMAL) | ring->rcs_bit;
        if (!is_last)
            ctrl |= TRB_CHAIN;
        else
            ctrl |= TRB_IOC;
        if (data_in)
            ctrl |= TRB_ISP;  /* interrupt on short packet */

        trb->control = ctrl;
        _inc_enqueue_ptr(ring);
        offset    += trb_len;
        remaining -= trb_len;
    }

    wmb();
    dma_cache_flush(xhci->xmem_pool, ring->trbs,
                    sizeof(xhci_trb_t) * ring->max_trb_count);

    __atomic_store_n(&ep->comp_done, 0, __ATOMIC_RELEASE);
    /* doorbell target = xHCI ep_id (1=EP0, N_OUT=N*2, N_IN=N*2+1) */
    _ring_doorbell(xhci, slot_id, ep_id);

    // Bounded wait: interrupt-IN endpoints that NAK (no data ready) would spin
    // here forever, freezing the whole usb-core IPC chain. After the budget
    // expires, return -ETIMEOUT so the class driver can retry. The pending TRB
    // stays on the ring; if data arrives later it still completes correctly.
#define XFER_YIELD_BUDGET 500
    int yield_count = 0;
    while (!__atomic_load_n(&ep->comp_done, __ATOMIC_ACQUIRE)) {
        if (++yield_count > XFER_YIELD_BUDGET) {
//            debug("[XHCI] bulk xfer timeout slot %u\n", slot_id);
            return -ETIMEOUT;
        }            
        sched_yield();
    }

    if (out_residual)
        *out_residual = ep->residual;

    return ep->comp_code;
}

int xhci_arm_bulk_transfer(xhci_t *xhci, uint8_t slot_id, uint8_t ep_id,
                            void *buf, uint32_t len, bool data_in)
{
    if (!xhci || !slot_id || ep_id == 0) return -EINVAL;

    xhci_virt_dev_t *dev = xhci->devs[slot_id];
    if (!dev) return -EINVAL;

    xhci_virt_ep_t *ep = dev->eps[ep_id - 1];
    if (!ep || !ep->tr_ring) return -EINVAL;

    xhci_ring_t *ring   = ep->tr_ring;
    paddr_t      buf_pa = dma_get_phys(xhci->xmem_pool, buf);

    uint32_t remaining = len;
    uint32_t offset    = 0;

    while (remaining > 0) {
        uint32_t trb_len = remaining > XFER_TRB_MAX_LEN ? XFER_TRB_MAX_LEN : remaining;
        bool     is_last = (remaining - trb_len == 0);

        xhci_trb_t *trb = &ring->trbs[ring->enqueue_ptr];
        memset(trb, 0, sizeof(xhci_trb_t));

        trb->parameter = buf_pa + offset;
        trb->status    = TRB_LEN(trb_len) | TRB_INTR_TARGET(0);

        uint32_t ctrl = TRB_TYPE(TRB_NORMAL) | ring->rcs_bit;
        if (!is_last)
            ctrl |= TRB_CHAIN;
        else
            ctrl |= TRB_IOC;
        if (data_in)
            ctrl |= TRB_ISP;

        trb->control = ctrl;
        _inc_enqueue_ptr(ring);
        offset    += trb_len;
        remaining -= trb_len;
    }

    wmb();
    dma_cache_flush(xhci->xmem_pool, ring->trbs,
                    sizeof(xhci_trb_t) * ring->max_trb_count);

    __atomic_store_n(&ep->comp_done, 0, __ATOMIC_RELEASE);
    _ring_doorbell(xhci, slot_id, ep_id);
    return 0;
}

int xhci_config_eps(xhci_t *xhci, uint8_t slot_id, uint8_t ep_nums, usb_ep_desc_t *ep_desc)
{
    if (!xhci || slot_id == 0 || ep_nums == 0 || !ep_desc) {
        return -EINVAL;
    }

    xhci_virt_dev_t *dev = xhci->devs[slot_id];
    if (!dev) {
        debug("[XHCI] No device found for slot %u\n", slot_id);
        return -ENOENT;
    }

    xhci_input_control_ctx_t *in_ctx = (xhci_input_control_ctx_t *)dev->in_ctx;
    uint32_t ctx_size = HCC_64BYTE_CONTEXT(xhci->hcc_params) ? 64 : 32;

    in_ctx->add_flags = SLOT_FLAG; // Add Slot Context
    in_ctx->drop_flags = 0;

    xhci_ep_ctx_t *slot_ctx = (xhci_ep_ctx_t *)((uint8_t *)dev->in_ctx + ctx_size);
    

    for (uint8_t i = 0; i < ep_nums; i++) {
        uint8_t ep_id = xhci_ep_id(ep_desc[i].ep_num, ep_desc[i].ep_dir);
        if (dev->eps[ep_id - 1]) {
            debug("[XHCI] Endpoint ID %u already configured on slot %u\n", ep_id, slot_id);
            continue;
        }

        xhci_virt_ep_t *ep = malloc(sizeof(xhci_virt_ep_t));
        if (!ep) {
            debug("[XHCI] Failed to allocate virtual endpoint structure\n");
            return -ENOMEM;
        }
        memset(ep, 0, sizeof(*ep));
        dev->eps[ep_id - 1] = ep;
        // Allocate transfer ring for the endpoint
        xhci_ring_t *ring = xhci_alloc_transfer_ring(xhci, 256);  // 256 TRBs for now
        if (!ring) {
            free(ep);
            debug("[XHCI] cannot allocate transfer ring for endpoint %u\n", ep_id);
            return -ENOMEM;
        }
        ep->tr_ring = ring;

        ep->max_packet = ep_desc[i].max_packet;

        slot_ctx->ep_info &= ~LAST_CTX_MASK;
        slot_ctx->ep_info |= LAST_CTX(ep_id);

        in_ctx->add_flags |= (1 << (ep_id));

        xhci_ep_ctx_t *ep_ctx = (xhci_ep_ctx_t *)((uint8_t *)dev->in_ctx + (1 + ep_id) * ctx_size);
        memset(ep_ctx, 0, sizeof(xhci_ep_ctx_t));

        ep_ctx->ep_info = EP_INTERVAL(ep_desc[i].interval) | EP_STATE_DISABLED;

        ep_ctx->ep_info2 = EP_TYPE(xhci_get_endpoint_type_from_ep_descriptor(&ep_desc[i]));
        ep_ctx->ep_info2 |= MAX_PACKET(ep->max_packet) | MAX_BURST(0) | ERROR_COUNT(3);

        ep_ctx->deq = dma_get_phys(xhci->xmem_pool, ring->trbs) | ring->rcs_bit;

        ep_ctx->tx_info = EP_MAX_ESIT_PAYLOAD_LO(ep->max_packet) | EP_AVG_TRB_LENGTH(ep->max_packet);


        debug("[XHCI] Configuring Endpoint 0x%02x: Type=%s, MaxPacket=%u, Interval=%u\n",
              ep_id,
              ep_desc[i].type == USB_ENDPOINT_XFER_BULK ? "Bulk" :
              ep_desc[i].type == USB_ENDPOINT_XFER_INT ? "Interrupt" :
              ep_desc[i].type == USB_ENDPOINT_XFER_ISOC ? "Isochronous" : "Control",
              ep_desc[i].max_packet,
              ep_desc[i].interval);

    }


    int ret = xhci_configure_endpoint(slot_id);
    if (ret < 0) {
        debug("[XHCI] Failed to configure endpoints for slot %u\n", slot_id);
        return ret;
    }

    return SUCCESS;
}