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

#define XHCI_CTX_TYPE_DEVICE  0x1
#define XHCI_CTX_TYPE_INPUT   0x2

void *xhci_alloc_container_ctx(xhci_t *xhci, int type)
{
	if ((type != XHCI_CTX_TYPE_DEVICE) && (type != XHCI_CTX_TYPE_INPUT))
		return NULL;
        
    size_t size = HCC_64BYTE_CONTEXT(xhci->hcc_params) ? 2048 : 1024;
	if (type == XHCI_CTX_TYPE_INPUT)
		size += CTX_SIZE(xhci->hcc_params);

    return dma_alloc(xhci->xmem_pool, size, PAGE_SIZE);
}

xhci_ring_t *xhci_alloc_transfer_ring(xhci_t *xhci, size_t trb_count)
{
    if (!xhci || !trb_count) return NULL;

    xhci_ring_t *ring = (xhci_ring_t *)malloc(sizeof(xhci_ring_t));
    if (!ring) return NULL;

    ring->max_trb_count = trb_count;
    ring->rcs_bit = 1;
    ring->denqueue_ptr = 0;
    ring->enqueue_ptr = 0;

    // Allocate TRBs for the transfer ring
    ring->trbs = dma_alloc(xhci->xmem_pool, sizeof(xhci_trb_t) * ring->max_trb_count, PAGE_SIZE);
    if (!ring->trbs) {
        debug("[XHCI] cannot allocate transfer ring TRBs\n");
        free(ring);
        return NULL;
    }

    // Set the last TRB as a link TRB to point back to the first TRB
    ring->trbs[ring->max_trb_count - 1].parameter = dma_get_phys(xhci->xmem_pool, ring->trbs);
    ring->trbs[ring->max_trb_count - 1].control = TRB_TYPE(TRB_LINK) | TRB_TC | ring->rcs_bit;

    wmb();
    // Flush the ring to DMA memory
    cache_flush((void *)dma_get_phys(xhci->xmem_pool, ring->trbs), sizeof(xhci_trb_t) * ring->max_trb_count);

    return ring;
}

int xhci_alloc_virt_device(xhci_t *xhci, int slot_id, uint8_t port)
{
    if (!slot_id || xhci->devs[slot_id]) {
        debug("[XHCI] Bad Slot ID %d\n", slot_id);
        return -EINVAL;
    }

    xhci_virt_dev_t *dev = malloc(sizeof(xhci_virt_dev_t));
    if (!dev)
        return -ENOMEM;

    dev->slot_id = slot_id;
    dev->port = port;

    dev->out_ctx = xhci_alloc_container_ctx(xhci, XHCI_CTX_TYPE_DEVICE);
    if (!dev->out_ctx)
        return -ENOMEM;

    // The output context is already zeroed by dma_alloc, but ensure it's flushed
    size_t out_ctx_size = HCC_64BYTE_CONTEXT(xhci->hcc_params) ? 2048 : 1024;
    cache_flush((void *)dma_get_phys(xhci->xmem_pool, dev->out_ctx), out_ctx_size);

    dev->in_ctx = xhci_alloc_container_ctx(xhci, XHCI_CTX_TYPE_INPUT);
    if (!dev->in_ctx)
        return -ENOMEM;

    xhci_virt_ep_t *ep0 = malloc(sizeof(xhci_virt_ep_t));
    if (!ep0)
        return -ENOMEM; 

    dev->eps[0] = ep0;

    // Allocate transfer ring for endpoint 0 (control endpoint)
    xhci_ring_t *ring = xhci_alloc_transfer_ring(xhci, 256);  // 256 TRBs for control transfers
    if (!ring) {
        debug("[XHCI] cannot allocate transfer ring for endpoint 0\n");
        return -ENOMEM;
    }
    dev->eps[0]->tr_ring = ring;

    // Initialize transfer ring TRBs to zero (except the link TRB)
    memset(ring->trbs, 0, sizeof(xhci_trb_t) * (ring->max_trb_count - 1));
    wmb();
    cache_flush((void *)dma_get_phys(xhci->xmem_pool, ring->trbs), 
                sizeof(xhci_trb_t) * ring->max_trb_count);

    // Initialize input context for Address Device command
    uint32_t *in_ctx = (uint32_t *)dev->in_ctx;
    uint32_t ctx_size = HCC_64BYTE_CONTEXT(xhci->hcc_params) ? 64 : 32;
    
    // Input Control Context - DWORD 0: Add Context flags
    // A0 = 1 (add slot context), A1 = 1 (add endpoint 0 context)
    in_ctx[0] = (1 << 0) | (1 << 1);  // A0 and A1 set
    
    // Clear the rest of input control context
    for (int i = 1; i < ctx_size/4; i++) {
        in_ctx[i] = 0;
    }
    
    // Slot Context (starts at ctx_size offset)
    uint32_t *slot_ctx = &in_ctx[ctx_size/4];
    xhci_slot_ctx_t *slot_context = (xhci_slot_ctx_t *)slot_ctx;
    
    // Get device speed from port
    uint32_t portsc = getreg32((uint64_t)&xhci->op_regs->port_status_base + (0x10 * port));
    dev->port_speed = DEV_PORT_SPEED(portsc);
    
    uint32_t speed_code;
    uint32_t max_pockets;
    switch (dev->port_speed) {
        case 1:     // Full Speed
            speed_code = 1; 
            max_pockets = 64;
            break;
        case 2:     // Low Speed
            speed_code = 2;
            max_pockets = 8;
            break;
        case 3: // High Speed
            speed_code = 3; 
            max_pockets = 64;
            break;
        case 4: // Super Speed
            speed_code = 4; 
            max_pockets = 512;
            break;
        case 5: // Super Speed Plus
            speed_code = 5; 
            max_pockets = 512;
            break;
        default: // Default to High Speed
            speed_code = 3; 
            max_pockets = 64;
            break;
    }
    
    // Slot Context DWORD 0: Route String (0), Speed, etc.
//    slot_ctx[0] = (speed_code << 20) | (1 << 27);  
    slot_context->dev_info = (speed_code << 20) | (1 << 27); // Speed (bits 23:20), Context Entries = 1 (bits 27:31)
    slot_context->dev_info2 = ((port + 1) << 16);  // Port number (1-based)
    // Slot Context DWORD 1: Root hub port number (bits 23:16)
    // slot_ctx[1] = ((port + 1) << 16);  // Port number (1-based)
    
    // Clear rest of slot context
    // for (int i = 2; i < ctx_size/4; i++) {
    //     slot_ctx[i] = 0;
    // }
    
    // Endpoint 0 Context (starts at 2*ctx_size offset)
    uint32_t *ep0_ctx = &in_ctx[2 * (ctx_size/4)];
    xhci_ep_ctx_t *ep0_context = (xhci_ep_ctx_t *)ep0_ctx;

    // Endpoint Context DWORD 0: State = Disabled
    ep0_ctx[0] = EP_INTERVAL(0) | (0 << 0) ;  // State=Disabled


    // Endpoint Context DWORD 1: Max Packet Size , Max Burst Size = 0, etc.
//    ep0_ctx[1] = EP_TYPE(CTRL_EP) | MAX_PACKET(max_pockets) | MAX_BURST(0) | ERROR_COUNT(3);
    ep0_context->ep_info2 = EP_TYPE(CTRL_EP) | MAX_PACKET(max_pockets) | MAX_BURST(0) | ERROR_COUNT(3);
    
    // Endpoint Context DWORD 2: TRB Pointer (Dequeue Pointer)
    // paddr_t ring_addr = dma_get_phys(xhci->xmem_pool, dev->eps[0]->trbs);
    // ep0_ctx[2] = (uint32_t)(ring_addr & 0xFFFFFFFF);  // Lower 32 bits
    // if (ctx_size == 64) {
    //     ep0_ctx[3] = (uint32_t)((ring_addr >> 32) & 0xFFFFFFFF);  // Upper 32 bits for 64-bit
    //     ep0_ctx[2] |= (dev->eps[0]->rcs_bit << 0);  // DCS bit in bit 0 of DWORD 2
    // } else {
    //     ep0_ctx[2] |= (dev->eps[0]->rcs_bit << 0);  // DCS bit in bit 0 of DWORD 2 for 32-bit
    // }

    ep0_context->deq = dma_get_phys(xhci->xmem_pool, ring->trbs) | ring->rcs_bit;
 
    // DWORD 4: Average TRB Length = 8 (typical for control transfers)
//    ep0_ctx[4] = 8;
    ep0_context->tx_info = 8;

    // Clear rest of endpoint context
    // int start_clear = ctx_size == 64 ? 4 : 3;
    // for (int i = start_clear; i < ctx_size/4; i++) {
    //     ep0_ctx[i] = 0;
    // }
    wmb();
    // Flush the input context to DMA memory
    cache_flush((void *)dma_get_phys(xhci->xmem_pool, dev->in_ctx), 3 * ctx_size);

    xhci->dcbaap[slot_id] = dma_get_phys(xhci->xmem_pool, dev->out_ctx);
    wmb();
    // Flush the DCBAAP update to DMA memory
    cache_flush((void *)dma_get_phys(xhci->xmem_pool, xhci->dcbaap), 64*4); // Assuming max 64 slots

    // Ring the doorbell to notify the controller of context update
    _ring_doorbell(xhci, slot_id, 0);

    xhci->devs[slot_id] = dev;

    // dupmp debug info input context and output context physical addresses
    // debug("[XHCI] input ctx phys: 0x%lX, output ctx phys: 0x%lX for slot %d\n",
    //       dma_get_phys(xhci->xmem_pool, dev->in_ctx),
    //       dma_get_phys(xhci->xmem_pool, dev->out_ctx),
    //       slot_id);

    // for (int i = 0; i < 3 * (ctx_size / 4); i++) {
    //     debug("[XHCI] in_ctx[%d] = 0x%X\n", i, ((uint32_t *)dev->in_ctx)[i]);
    // }
    return SUCCESS;
}

int xhci_mem_init(xhci_t *xhci)
{
    uint64_t pa;
    // max scratchpads+1 + DCBAAP + 2 Command ring + 2 event ring + 2 device context
    size_t size = 0x400000; // 4MB should be enough for initial allocations

    dma_pool_init(&xhci->xmem_pool, size);

 
    if (xhci->max_scratchpads) {
        if (xhci->max_scratchpads > 512) {
            debug("[XHCI] MAX Scratchpad more then 512 not supported\n");
            return -ENOSUPPORT;
        }

        xhci->scratchpad_array = dma_alloc_page(xhci->xmem_pool);
        if (!xhci->scratchpad_array) {
            debug("[XHCI] Cannot clloc scratchpad table page\n");
            return -EIO;
        }  

        for (int i = 0; i < xhci->max_scratchpads; i++) {
            void *page = dma_alloc_page(xhci->xmem_pool);
            if (!page) {
                debug("[XHCI] cannot allocate scratchpad page\n");
                return -ENOMEM;
            }            
            xhci->scratchpad_array[i] = dma_get_phys(xhci->xmem_pool, page);
        }

        wmb();
        // Flush the scratchpad array to DMA memory        
        dma_cache_flush(xhci->xmem_pool, xhci->scratchpad_array, PAGE_SIZE);        
    }


    size_t dcbaa_size = sizeof(uintptr_t) * (xhci->max_slots + 1);
    xhci->dcbaap = dma_alloc(xhci->xmem_pool, dcbaa_size, 64);
    if (!xhci->dcbaap) {
        debug("[XHCI] Cannot map dma block\n");
        return -EIO;
    }

    if (!xhci->max_scratchpads) {
        xhci->dcbaap[0] = 0;
    } else {   
        xhci->dcbaap[0] = dma_get_phys(xhci->xmem_pool, xhci->scratchpad_array);
    }

    wmb();
    // Flush the DCBAAP update to DMA memory
    cache_flush((void *)dma_get_phys(xhci->xmem_pool, xhci->dcbaap), 64*4); // Assuming max 64 slots

    
    xhci->op_regs->dcbaap = dma_get_phys(xhci->xmem_pool, xhci->dcbaap);

    xhci->max_trb_count = 256;
    xhci->rcs_bit = 1;
    xhci->enqueue_ptr = 0;

    xhci->trbs = dma_alloc(xhci->xmem_pool, sizeof(xhci_trb_t) * xhci->max_trb_count, PAGE_SIZE);
    if (!xhci->trbs) {
        debug("[XHCI] cannot allocate command ring page\n");
        return -ENOMEM;
    }

    // Set the last TRB as a link TRB to point back to the first TRB
    xhci->trbs[xhci->max_trb_count - 1].parameter = dma_get_phys(xhci->xmem_pool, xhci->trbs);
    xhci->trbs[xhci->max_trb_count - 1].control = TRB_TYPE(TRB_LINK) | TRB_TC | xhci->rcs_bit;

    xhci->op_regs->crcr = (uint64_t)dma_get_phys(xhci->xmem_pool, xhci->trbs) | xhci->rcs_bit;

    list_init(&xhci->competion_events);

//debug("[XHCI] allocated CRCR at 0x%p mapped at 0x%p <= 0x%lX\n", xhci->trbs_paddr, xhci->trbs, val);
    return SUCCESS;
}

static void _inc_enqueue_ptr(xhci_ring_t *ring)
{
    // Advance and possibly wrap the enqueue pointer if needed.
    // maxTrbCount - 1 accounts for the LINK_TRB.
    if (++ring->enqueue_ptr == ring->max_trb_count - 1) {
        // Update the Link TRB to reflect the current,
        // cycle state including the TC flag.
        ring->trbs[ring->max_trb_count - 1].control =
            TRB_TYPE(TRB_LINK) | TRB_TC | ring->rcs_bit;
        ring->enqueue_ptr = 0;
        ring->rcs_bit = !ring->rcs_bit;
    }
}

// Dump dma buffer content for debugging
static void dump_dma_buffer(xhci_t *xhci, void *dma_buffer, size_t length)
{
    uint8_t *buf = (uint8_t *)dma_buffer;
    debug("[XHCI] DMA Buffer Content at 0x%lX (length=%u):\n", (uint64_t)dma_buffer, length);
    for (size_t i = 0; i < length; i++) {
        debug("%02X ", buf[i]);
        if ((i + 1) % 16 == 0) {
            debug("\n");
        }
    }
    if (length % 16 != 0) {
        debug("\n");
    }
}

// EP0 Control Transfer function
int xhci_ep0_control_transfer(xhci_t *xhci, uint8_t slot_id, usb_control_request_t *req, 
                             void *data, size_t data_len, bool data_in)
{
    xhci_virt_dev_t *dev = xhci->devs[slot_id];
    if (!dev || !dev->eps[0]->tr_ring) {
        debug("[XHCI] Invalid device or EP0 ring for slot %u\n", slot_id);
        return -EINVAL;
    }

    void *transfer_status_buffer = dma_alloc(xhci->xmem_pool, 64, 64);
    if (!transfer_status_buffer) {
        debug("[XHCI] Failed to allocate transfer status buffer\n");
        return -ENOMEM;
    }

    xhci_ring_t *ring = dev->eps[0]->tr_ring;
    xhci_trb_t *trb;
//    int trb_count = 0;

    // SETUP stage TRB
    trb = &ring->trbs[ring->enqueue_ptr];
    memset(trb, 0, sizeof(xhci_trb_t));
    
    // Copy setup packet to parameter field (8 bytes)
    memcpy(&trb->parameter, req, 8);
    
    trb->status = 8;  // Setup packet is always 8 bytes

    // TRT (Transfer Type): 0=No Data, 1=OUT, 2=IN (2=OUT, 3=IN in XHCI spec)
    uint32_t trt = 0;
    if (req->wLength > 0) {
        trt = data_in ? 3 : 2;
    }

    trb->control = TRB_TYPE(TRB_SETUP) | (1 << 6) | (trt << 16) | /*(1 << 5) |*/ ring->rcs_bit;  // IDT=1, TRT, IOC=1

    _inc_enqueue_ptr(ring);
//    trb_count++;

    size_t wait_pos;

    // xhci_event_cmd_t *comp_event;
    // DATA stage TRB (if data transfer required)
    if (req->wLength > 0) {
        trb = &ring->trbs[ring->enqueue_ptr];
        memset(trb, 0, sizeof(xhci_trb_t));
        
        if (data) {
            trb->parameter = dma_get_phys(xhci->xmem_pool, data);
        }

        trb->status = 0 | req->wLength;
        uint32_t direction = data_in ? (1 << 16) : 0;  // DIR=1 for IN
        trb->control = TRB_TYPE(TRB_DATA) | direction | /*TRB_CHAIN |*/ ring->rcs_bit;

        _inc_enqueue_ptr(ring);

        // trb = &ring->trbs[ring->enqueue_ptr];
        // memset(trb, 0, sizeof(xhci_trb_t));

        // trb->parameter = dma_get_phys(xhci->xmem_pool, transfer_status_buffer);
        // trb->status = 0;   
        // trb->control = TRB_TYPE(TRB_EVENT_DATA) | direction | TRB_IOC | ring->rcs_bit;

        // wait_pos = ring->enqueue_ptr;
//        comp_event = (xhci_event_cmd_t *)trb;
//        _inc_enqueue_ptr(ring);

    //     wmb();
    //     // Flush the TRBs to DMA memory
    //     dma_cache_flush(xhci->xmem_pool, ring->trbs, sizeof(xhci_trb_t) * ring->max_trb_count);

    //     // Ring the endpoint doorbell
    //     _ring_control_endpoint_doorbell(xhci, slot_id);


    // //    debug("[XHCI] EP0 control transfer queued for slot %u (%d TRBs)\n", slot_id, trb_count);
    //     // Wait for completion
    //     size_t timeout_ms = 5000;  // 5 seconds timeout
    //     // Wait for the IRQ and let the host controller process the command
    //     uint64_t sleep_passed = 0;

    //     debug("\x1b[31m[xhci]\x1b[0m Waiting for DATA stage completion 0x%lx => 0x%lx...\n", 
    //             dma_get_phys(xhci->xmem_pool, &ring->trbs[wait_pos]),
    //         dma_get_phys(xhci->xmem_pool, transfer_status_buffer));
    //     while (__atomic_load_n(&ring->denqueue_ptr, __ATOMIC_RELAXED) != wait_pos) {
    // //        sys_wait_irq();
    //         udelay(10);
    //         sleep_passed += 10;
    //         if (sleep_passed > timeout_ms * 1000) {
    //             debug("\x1b[31m[xhci]\x1b[0m Data transfer timeout %u \n", sleep_passed); 
    //             break;
    //         }
    //     }


    }

    // STATUS stage TRB
    trb = &ring->trbs[ring->enqueue_ptr];
    memset(trb, 0, sizeof(xhci_trb_t));
    
    trb->parameter = 0;
    trb->status = 0;
    
    // DIR: opposite of DATA stage (1=IN if DATA was OUT, 0=OUT if DATA was IN)
    uint32_t direction = (req->wLength > 0 && data_in) ? 0 : (1 << 16);

    trb->control = TRB_TYPE(TRB_STATUS) | direction | (1 << 5) | ring->rcs_bit;  // IOC=1

    wait_pos = ring->enqueue_ptr;

    _inc_enqueue_ptr(ring);
//    trb_count++;

    // Set cycle bits for all TRBs
    // for (int i = 0; i < trb_count; i++) {
    //     int idx = (ring->enqueue_ptr - trb_count + i + (ring->max_trb_count - 1)) % (ring->max_trb_count - 1);
    //     ring->trbs[idx].control |= ring->rcs_bit;
    // }

    wmb();
    // Flush the TRBs to DMA memory
    dma_cache_flush(xhci->xmem_pool, ring->trbs, sizeof(xhci_trb_t) * ring->max_trb_count);

    // Ring the endpoint doorbell
    _ring_control_endpoint_doorbell(xhci, slot_id);


//    debug("[XHCI] EP0 control transfer queued for slot %u (%d TRBs)\n", slot_id, trb_count);
    // Wait for completion
    size_t timeout_ms = 5000;  // 5 seconds timeout
    // Wait for the IRQ and let the host controller process the command
    uint64_t sleep_passed = 0;

    // FIXME: Replace with atomic load and correct irq wait
    while (__atomic_load_n(&ring->denqueue_ptr, __ATOMIC_RELAXED) != wait_pos) {
//        sys_wait_irq();
        udelay(10);
        sleep_passed += 10;
        if (sleep_passed > timeout_ms * 1000) {
            debug("\x1b[31m[xhci]\x1b[0m Transfer timeout %u \n", sleep_passed); 
            break;
        }
    }

    // rmb();
    // // Invalidate cache to ensure we read the latest data
    //  dma_cache_invalidate(xhci->xmem_pool, transfer_status_buffer, 64);
    //  dump_dma_buffer(xhci, transfer_status_buffer, 64);

    // dma_cache_invalidate(xhci->xmem_pool, ring->trbs, sizeof(xhci_trb_t) * ring->max_trb_count);

    // debug("[XHCI] EP0 control transfer completed status %u\n", comp_event->completion_code);

    dma_free(xhci->xmem_pool, transfer_status_buffer);
//    debug("\x1b[31m[xhci]\x1b[0m Transfer time %u \n", sleep_passed); 
//    __atomic_store_n(&xhci->irq_completed, 0, __ATOMIC_ACQ_REL);
    return SUCCESS;
}



// Helper function for GET_DESCRIPTOR
int xhci_get_descriptor(xhci_t *xhci, uint8_t slot_id, uint8_t desc_type, 
                       uint8_t desc_index, uint16_t lang_id, void *buffer, 
                       size_t buffer_size, size_t *actual_length)
{
    usb_control_request_t req;
    memset(&req, 0, sizeof(req));
    
    req.bmRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
    req.bRequest = USB_REQ_GET_DESCRIPTOR;
    req.wValue = (desc_type << 8) | desc_index;
    req.wIndex = lang_id;  // Language ID (for string descriptors)
    req.wLength = buffer_size;
    
    // Allocate DMA buffer for the descriptor
    void *dma_buffer = dma_alloc(xhci->xmem_pool, buffer_size, PAGE_SIZE);
    if (!dma_buffer) {
        debug("[XHCI] Failed to allocate DMA buffer for descriptor\n");
        return -ENOMEM;
    }

    wmb();
    dma_cache_flush(xhci->xmem_pool, dma_buffer, buffer_size);

    // debug("[XHCI] GET_DESCRIPTOR (type=0x%x, index=%u) allocated DMA buffer at 0x%p for slot %u\n", 
    //       desc_type, desc_index, dma_get_phys(xhci->xmem_pool, dma_buffer), slot_id);
    int ret = xhci_ep0_control_transfer(xhci, slot_id, &req, dma_buffer, buffer_size, true);
    if (ret != SUCCESS) {
        debug("\x1b[31m[xhci]\x1b[0m Transfer failed %u \n", ret); 
        dma_free(xhci->xmem_pool, dma_buffer);
        return ret;
    }
    rmb();

    // Invalidate cache to ensure we read the latest data
    dma_cache_invalidate(xhci->xmem_pool, dma_buffer, buffer_size);    
  //  udelay(1000);  // Small delay to ensure data is ready
    // rmb();
    // dump_dma_buffer(xhci, dma_buffer, buffer_size);
    // Copy data from DMA buffer to user buffer
    memcpy(buffer, dma_buffer, buffer_size);

    
    // For now, assume we got the full descriptor
    // In a real implementation, we'd check the actual transfer length
    if (actual_length) {
        *actual_length = buffer_size;
    }
    
    dma_free(xhci->xmem_pool, dma_buffer);


    // debug("[XHCI] GET_DESCRIPTOR (type=0x%x, index=%u) completed for slot %u\n", 
    //       desc_type, desc_index, slot_id);
    
    return SUCCESS;
}

// Helper function for SET_CONFIGURATION
int xhci_set_configuration(xhci_t *xhci, uint8_t slot_id, uint8_t config_value)
{
    usb_control_request_t req;
    memset(&req, 0, sizeof(req));
    
    req.bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
    req.bRequest = USB_REQ_SET_CONFIGURATION;
    req.wValue = config_value;
    req.wIndex = 0;
    req.wLength = 0;  // No data stage
    
    int ret = xhci_ep0_control_transfer(xhci, slot_id, &req, NULL, 0, false);
    if (ret == SUCCESS) {
        debug("[XHCI] SET_CONFIGURATION (%u) completed for slot %u\n", config_value, slot_id);
    }
    
    return ret;
}

// Helper function for SET_ADDRESS
int xhci_set_address(xhci_t *xhci, uint8_t slot_id, uint8_t device_address)
{
    usb_control_request_t req;
    memset(&req, 0, sizeof(req));
    
    req.bmRequestType = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
    req.bRequest = USB_REQ_SET_ADDRESS;
    req.wValue = device_address;
    req.wIndex = 0;
    req.wLength = 0;  // No data stage
    
    int ret = xhci_ep0_control_transfer(xhci, slot_id, &req, NULL, 0, false);
    if (ret == SUCCESS) {
        debug("[XHCI] SET_ADDRESS (%u) completed for slot %u\n", device_address, slot_id);
    }
    
    return ret;
}