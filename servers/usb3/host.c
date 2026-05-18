#include <libsys/common.h>
#include <string.h>
#include <libsys/timer.h>
#include <libsys/riscv.h>
#include <libsys/memory.h>
#include <cap.h>
#include <signals.h>
#include <libsys/barrier.h>
#include <libsys/ipc.h>
#include <sched.h>

#include "main.h"
#include "xhci.h"
#include "dma-pool.h"
#include <libsys/usb/usb.h>
#include <libsys/usb/usb-ipc.h>


xhci_t *xhci_dev;


const char* _usb_speed_to_string(uint8_t speed) {
    static const char* speed_string[7] = {
        "Invalid",
        "Full Speed (12 MB/s - USB2.0)",
        "Low Speed (1.5 Mb/s - USB 2.0)",
        "High Speed (480 Mb/s - USB 2.0)",
        "Super Speed (5 Gb/s - USB3.0)",
        "Super Speed Plus (10 Gb/s - USB 3.1)",
        "Undefined"
    };

    return speed_string[speed];
}

int xhci_handshake(void *ptr,
		   uint32_t mask, uint32_t done, int usec)
{
	uint32_t	result;

	do {
		result = getreg32((uint64_t)ptr);
		if (result == ~(uint32_t)0)		/* card removed */
			return -ENOENT;
		result &= mask;
		if (result == done)
			return 0;
		udelay (1);
		usec--;
	} while (usec > 0);
	return -ETIMEOUT;
}

// Helper function to print UTF-16 strings, skipping zeros
void print_utf16_string(const uint16_t *utf16_str, size_t max_chars)
{
    if (!utf16_str) return;
    
    for (size_t i = 0; i < max_chars; i++) {
        uint16_t ch = utf16_str[i];
        
        // Skip null characters
        if (ch == 0) {
            break;
        }
        
        // For now, handle only ASCII characters (0x0000-0x007F)
        // UTF-16 ASCII characters have the high byte as 0
        if (ch <= 0x007F) {
            debug("%c", (char)ch);
        } else {
            // For non-ASCII characters, print a placeholder
            debug("?");
        }
    }
    debug("\n");
}

int xhci_reset(xhci_t *xhci, uint64_t timeout_us)
{
    uint32_t state, cmd;
    int ret;

//     // Make sure we clear the Run/Stop bit
//     cmd = xhci->op_regs->usbcmd;
//     cmd &= ~CMD_RUN;
//     xhci->op_regs->usbcmd = cmd;
// //    putreg32(cmd, (uint64_t)&xhci->op_regs->usbcmd);
//     ret = xhci_handshake(&xhci->op_regs->usbsts, STS_HCE, 0, timeout_us);
// 	if (ret)
// 		return ret;
    
    state = xhci->op_regs->usbsts;

    if ((state & STS_HALT) == 0) {
        debug("[XHCI] Host controller not halted, aborting reset. State = 0x%X\n", state);
        return SUCCESS;
    }
//    debug("[XHCI] Reset HC\n");
    cmd = xhci->op_regs->usbcmd;
	cmd |= CMD_RESET;
//	xhci->op_regs->usbcmd = cmd;
    putreg32(cmd, (uint64_t)&xhci->op_regs->usbcmd);

    udelay(1000);

	ret = xhci_handshake(&xhci->op_regs->usbcmd, CMD_RESET, 0, timeout_us);
	if (ret < 0)
		return ret;

    ret = xhci_handshake(&xhci->op_regs->usbsts, STS_CNR, 0, timeout_us);

//    debug("[XHCI] Reset done\n");

    return ret;
}

static void xhci_power_off_all_roothub_ports(xhci_t *xhci)
{
	u32 offset;
	uint32_t reg;
	int i;

    for (i = 0; i < xhci->max_ports; i++) {
        offset = 0x10 * i;
        reg = getreg32((uint64_t)&xhci->op_regs->port_status_base + offset);
 //       debug("[XHCI] port %d POWER at 0x%lX => 0x%X\n", i+1, (uint64_t)&xhci->op_regs->port_power_base + offset, reg);
        reg &= ~PORT_POWER;
        putreg32(reg, (uint64_t)&xhci->op_regs->port_power_base + offset);
    }

}

int xhci_reset_port(uint8_t port_num, bool is_usb3) 
{
    uint32_t portsc = _read_portsc_reg(port_num);


    // Power on the port if necessary
    if ((portsc & PORT_POWER) == 0) {
        portsc |= PORT_POWER;
        _write_portsc_reg(portsc, port_num);
        udelay(20000); // Wait for power stabilization
        portsc = _read_portsc_reg(port_num);

        if ((portsc & PORT_POWER) == 0) {
            debug("Port %i: Failed to power on port\n", port_num);
            return -EIO;
        }
    }
  
    // Clear any lingering status change bits before initiating the reset
    portsc |= PORT_CSC; // Clear connect status change
    portsc |= PORT_PEC; // Clear port enable/disable change
    portsc |= PORT_RC; // Clear port reset change
    _write_portsc_reg(portsc, port_num);


    // Initiate the port reset
    if (is_usb3) {
        portsc |= PORT_WR; // Warm reset for USB 3.0
    } else {
        portsc |= PORT_RESET; // Standard port reset for USB 2.0
    }
     
    _write_portsc_reg(portsc, port_num);

    // Wait for the reset to complete
    int timeout = 1000;
    while (timeout > 0) {
        portsc = _read_portsc_reg(port_num);

        if ((is_usb3 && (portsc & PORT_WR)) || (!is_usb3 && (portsc & PORT_RESET))) {
            break; // Reset has completed
        }

        timeout--;
        udelay(1000);
    }

    if (timeout == 0) {
        debug("Port %i: Port reset timed out\n", port_num);
        return -ETIMEOUT;
    }

    udelay(3000); // Give the hardware time to settle

 
    // Clear the reset completion and status change bits
    portsc |= PORT_RC; // Clear port reset change
    portsc |= PORT_WR; // Clear warm reset change (USB 3.0)
    portsc |= PORT_CSC; // Clear connect status change
    portsc |= PORT_PEC; // Clear port enable/disable change
    portsc &= ~PORT_PE; // Don't clear the PED bit
    _write_portsc_reg(portsc, port_num);

    udelay(3000);

    // Re-read the register to check if the port is enabled
    portsc = _read_portsc_reg(port_num);

    // This case could happen when the port has been reset after
    // a device disconnect event, and no device has connected since.
    if ((portsc & PORT_PE) == 0) {
        return -EIO;
    }

    return SUCCESS;
}

static void _log_op_regs(xhci_t *xhci)
{
    debug("===== xHCI Operational Registers (0x%llx) =====\n", (uint64_t)xhci->op_regs);
    debug("    usbcmd     : 0x%x\n", xhci->op_regs->usbcmd);
    debug("    usbsts     : 0x%x\n", xhci->op_regs->usbsts);
    debug("    pagesize   : 0x%x\n", xhci->op_regs->pagesize);
    debug("    dnctrl     : 0x%x\n", xhci->op_regs->dnctrl);
    debug("    crcr       : 0x%lx\n", xhci->op_regs->crcr);
    debug("    dcbaap     : 0x%lx\n", xhci->op_regs->dcbaap);
    debug("    config     : 0x%x\n", xhci->op_regs->config);
    debug("\n");
}

static void _log_usbsts() 
{
    uint32_t status = xhci_dev->op_regs->usbsts;
    debug("===== USBSTS =====\n");
    if (status & STS_HALT)  debug("    Host Controlled Halted\n");
    if (status & STS_FATAL)  debug("    Host System Error\n");
    if (status & STS_EINT) debug("    Event Interrupt\n");
    if (status & STS_PORT)  debug("    Port Change Detect\n");
    if (status & STS_SAVE)  debug("    Save State Status\n");
    if (status & STS_RESTORE)  debug("    Restore State Status\n");
    if (status & STS_SRE)  debug("    Save/Restore Error\n");
    if (status & STS_CNR)  debug("    Controller Not Ready\n");
    if (status & STS_HCE)  debug("    Host Controller Error\n");
    debug("\n");
}

static void _xhci_update_erdp(xhci_ring_t *event_ring)
{
    volatile xhci_intr_reg_t *ir = &xhci_dev->run_regs->ir_set[0];
    paddr_t denqueue = dma_get_phys(xhci_dev->xmem_pool, event_ring->trbs) + (event_ring->denqueue_ptr * sizeof(xhci_trb_t));
    ir->erst_dequeue = denqueue;
}

static xhci_trb_t *_dequeue_trb(xhci_ring_t *event_ring)
{
    if (event_ring->trbs[event_ring->denqueue_ptr].cycle_bit != event_ring->rcs_bit) {
        debug("[XHCI] Event Ring attempted to dequeue an invalid TRB, returning nullptr!\n");
        return NULL;        
    }

    xhci_trb_t *ret = &event_ring->trbs[event_ring->denqueue_ptr];

    // Advance and possibly wrap the dequeue pointer if needed
    if (++event_ring->denqueue_ptr == event_ring->max_trb_count) {
        event_ring->denqueue_ptr = 0;
        event_ring->rcs_bit = !event_ring->rcs_bit;
    } 

    return ret;
}

static void _enqueue_trb(xhci_t *xhci, xhci_trb_t* trb) {
    // Adjust the TRB's cycle bit to the current RCS
    trb->cycle_bit = xhci->rcs_bit;

    // Insert the TRB into the ring
    xhci->trbs[xhci->enqueue_ptr] = *trb;

    // Advance and possibly wrap the enqueue pointer if needed.
    // maxTrbCount - 1 accounts for the LINK_TRB.
    if (++xhci->enqueue_ptr == xhci->max_trb_count - 1) {
        // Update the Link TRB to reflect the current,
        // cycle state including the TC flag.
        xhci->trbs[xhci->max_trb_count - 1].control =
            TRB_TYPE(TRB_LINK) | TRB_TC | xhci->rcs_bit;

        xhci->enqueue_ptr = 0;
        xhci->rcs_bit = !xhci->rcs_bit;
    }
    wmb();
    cache_flush((void *)dma_get_phys(xhci->xmem_pool, xhci->trbs), sizeof(xhci_trb_t) * xhci->max_trb_count);
}

xhci_ring_t *xhci_event_ring_alloc(xhci_t *xhci, size_t trbs, volatile xhci_intr_reg_t *ir)
{
    if (!xhci || !ir) return NULL;

    xhci_ring_t *event_ring = (xhci_ring_t *)malloc(sizeof(xhci_ring_t));
    if (!event_ring) return NULL;

    event_ring->max_trb_count = 256;
    event_ring->rcs_bit = 1;
    event_ring->denqueue_ptr = 0;
//    event_ring->interrupter = ir;

    // allocate 1 segment for 256 trbs = 1 PAGE
    event_ring->trbs = dma_alloc(xhci->xmem_pool, sizeof(xhci_trb_t) * event_ring->max_trb_count, 64);
     if (!event_ring->trbs) {
        debug("[XHCI] cannot allocate event ring page\n");
        return NULL;
    }  
  
    cache_flush((void *)dma_get_phys(xhci_dev->xmem_pool, event_ring->trbs), sizeof(xhci_trb_t) * event_ring->max_trb_count);

    // allocate Page for segment table

    event_ring->seg_table = dma_alloc(xhci->xmem_pool, 64, 64);
     if (!event_ring->seg_table) {
        debug("[XHCI] cannot allocate segnment table for event ring page\n");
        return NULL;
    } 
 
    xhci_erst_entry_t entry;
    entry.seg_addr = dma_get_phys(xhci->xmem_pool, event_ring->trbs);
    entry.seg_size = event_ring->max_trb_count;
    entry.rsvd     = 0;

    event_ring->seg_table[0] = entry;

    cache_flush((void *)dma_get_phys(xhci->xmem_pool, event_ring->seg_table), 64);

    ir->erst_size = 1;

    _xhci_update_erdp(event_ring);

    ir->erst_base = dma_get_phys(xhci->xmem_pool, event_ring->seg_table);

    return event_ring;
}

static void _acknowledge_irq(xhci_t *xhci, uint8_t ir)
{
    xhci->op_regs->usbsts = STS_EINT;

    volatile xhci_intr_reg_t *irr = &xhci->run_regs->ir_set[ir];

    uint32_t iman = irr->iman;
    iman |= IMAN_IP;
    irr->iman = iman;
}

xhci_event_cmd_t *xhci_send_command(xhci_t *xhci, xhci_trb_t *trb, uint32_t timeout_ms)
{
    xhci->completion_trb = 0;
//    xhci->irq_completed = 0;
    __atomic_store_n(&xhci->irq_completed, 0, __ATOMIC_ACQUIRE);

    // Enqueue the TRB
    _enqueue_trb(xhci, trb);

    // Ring the command doorbell
    _ring_command_doorbell(xhci);

    // Wait for the IRQ and let the host controller process the command
    uint64_t sleep_passed = 0;
    // rmb();
    while (!__atomic_load_n(&xhci->irq_completed, __ATOMIC_RELAXED)) {
        udelay(10);
        sleep_passed += 10;
 //       rmb();
 //   debug("%d", xhci_dev->irq_completed);
        if (sleep_passed > timeout_ms * 1000) {
            break;
        }
    }
//debug("\x1b[31m[xhci]\x1b[0m slot id %u\n", xhci_dev->completion_trb->slot_id);
//    debug("[XHCI] CMD completed trd 0x%lX time 0x%lX\n", xhci->completion_trb, sleep_passed);
//    xhci->irq_completed = 0;
    __atomic_store_n(&xhci->irq_completed, 0, __ATOMIC_ACQ_REL);
    return xhci->completion_trb;
}



int xhci_address_device(uint8_t slot_id, bool bsr) 
{
    xhci_virt_dev_t *dev = xhci_dev->devs[slot_id];
    if (!dev) {
        debug("[XHCI] No device context for slot %u\n", slot_id);
        return -EINVAL;
    }

    xhci_trb_t addr_dev_trb;
    memset(&addr_dev_trb, 0, sizeof(xhci_trb_t));

    // Parameter: Input Context Pointer
    addr_dev_trb.parameter = dma_get_phys(xhci_dev->xmem_pool, dev->in_ctx);
    
    uint32_t trb_bsr = bsr ? TRB_BSR : 0;
    // Control: Slot ID and TRB Type
    addr_dev_trb.control = SLOT_ID_FOR_TRB(slot_id) | TRB_TYPE(TRB_ADDR_DEV) | trb_bsr;

    // dump trb info
    // debug("[XHCI] Address Device TRB: param=0x%lX control=0x%lX\n", addr_dev_trb.parameter,
    //      addr_dev_trb.control);
        
    xhci_event_cmd_t *completion_trb = xhci_send_command(xhci_dev, &addr_dev_trb, 200);
    if (!completion_trb) {
        debug("[XHCI] Address device command failed for slot %u\n", slot_id);
        return -EIO;
    }

    if (GET_COMP_CODE(completion_trb->status) != COMP_SUCCESS) {
        debug("[XHCI] Address device failed with code %u for slot %u\n", 
              GET_COMP_CODE(completion_trb->status), slot_id);
        return -EIO;
    }

    debug("[XHCI] Device addressed successfully on slot %u\n", completion_trb->slot_id);

    // uint32_t ctx_size = HCC_64BYTE_CONTEXT(xhci_dev->hcc_params) ? 64 : 32;
    // for (int i = 0; i < 2 * (ctx_size / 4); i++) {
    //     debug("[XHCI] out_ctx[%d] = 0x%X\n", i, ((uint32_t *)dev->out_ctx)[i]);
    // }
    return SUCCESS;
}


int xhci_evaluate_context(uint8_t slot_id) 
{
    xhci_virt_dev_t *dev = xhci_dev->devs[slot_id];
    if (!dev) {
        debug("[XHCI] No device context for slot %u\n", slot_id);
        return -EINVAL;
    }

    xhci_trb_t eval_ctx_trb;
    memset(&eval_ctx_trb, 0, sizeof(xhci_trb_t));   

    // Parameter: Input Context Pointer
    eval_ctx_trb.parameter = dma_get_phys(xhci_dev->xmem_pool, dev->in_ctx);
    // Control: Slot ID and TRB Type
    eval_ctx_trb.control = SLOT_ID_FOR_TRB(slot_id) | TRB_TYPE(TRB_EVAL_CONTEXT);

    xhci_event_cmd_t *completion_trb = xhci_send_command(xhci_dev, &eval_ctx_trb, 200);
    if (!completion_trb) {
        debug("[XHCI] Evaluate context command failed for slot %u\n", slot_id);
        return -EIO;
    }
    if (GET_COMP_CODE(completion_trb->status) != COMP_SUCCESS) {
        debug("[XHCI] Evaluate context failed with code %u for slot %u\n", 
              GET_COMP_CODE(completion_trb->status), slot_id);
        return -EIO;
    }
    debug("[XHCI] Device context evaluated successfully on slot %u\n", completion_trb->slot_id);   

    return SUCCESS;
}

int xhci_disable_device_slot(uint8_t slot_id)
{
    xhci_trb_t trb;
    memset(&trb, 0, sizeof(trb));
    trb.control = SLOT_ID_FOR_TRB(slot_id) | TRB_TYPE(TRB_DISABLE_SLOT);

    xhci_event_cmd_t *comp = xhci_send_command(xhci_dev, &trb, 200);
    if (!comp) {
        debug("[XHCI] Disable Slot command failed for slot %u\n", slot_id);
        return -EIO;
    }
    if (GET_COMP_CODE(comp->status) != COMP_SUCCESS) {
        debug("[XHCI] Disable Slot failed code %u slot %u\n",
              GET_COMP_CODE(comp->status), slot_id);
        return -EIO;
    }
    debug("[XHCI] Slot %u disabled\n", slot_id);
    return SUCCESS;
}

int xhci_reset_ep(uint8_t slot_id, uint8_t ep_id)
{
    xhci_virt_dev_t *dev = xhci_dev->devs[slot_id];
    if (!dev || !dev->eps[ep_id - 1]) return -EINVAL;

    xhci_trb_t trb;
    memset(&trb, 0, sizeof(trb));
    trb.control = SLOT_ID_FOR_TRB(slot_id) | ((ep_id & 0x1f) << 16) | TRB_TYPE(TRB_RESET_EP);

    xhci_event_cmd_t *comp = xhci_send_command(xhci_dev, &trb, 200);
    if (!comp) {
        debug("[XHCI] Reset EP command failed slot %u ep %u\n", slot_id, ep_id);
        return -EIO;
    }
    if (GET_COMP_CODE(comp->status) != COMP_SUCCESS) {
        debug("[XHCI] Reset EP failed code %u slot %u ep %u\n",
              GET_COMP_CODE(comp->status), slot_id, ep_id);
        return -EIO;
    }
    debug("[XHCI] Reset EP %u on slot %u\n", ep_id, slot_id);
    return SUCCESS;
}

int xhci_stop_ep(uint8_t slot_id, uint8_t ep_id)
{
    xhci_virt_dev_t *dev = xhci_dev->devs[slot_id];
    if (!dev || !dev->eps[ep_id - 1] || !dev->eps[ep_id - 1]->tr_ring)
        return -EINVAL;

    xhci_trb_t trb;
    memset(&trb, 0, sizeof(trb));
    /* bits 20:16 = EP ID, bits 31:24 = Slot ID */
    trb.control = SLOT_ID_FOR_TRB(slot_id) | ((ep_id & 0x1f) << 16) | TRB_TYPE(TRB_STOP_RING);

    xhci_event_cmd_t *comp = xhci_send_command(xhci_dev, &trb, 200);
    if (!comp) {
        debug("[XHCI] Stop Endpoint command failed slot %u ep %u\n", slot_id, ep_id);
        return -EIO;
    }

    /* Signal any pending waiter with -ENODEV */
    xhci_virt_ep_t *ep = dev->eps[ep_id - 1];
    if (ep->waiter_pid) {
        ep->comp_code = -ENOENT;
        wmb();
        __atomic_store_n(&ep->comp_done, 1, __ATOMIC_RELEASE);
        ep->waiter_pid = 0;
    }

    /* Drain any in-flight async URB with ENOENT */
    pending_urb_t *pu = &ep->pending_urb;
    if (pu->valid) {
        class_client_t *cc = pu->client;
        if (pu->dma_buf) {
            dma_free(xhci_dev->xmem_pool, pu->dma_buf);
            pu->dma_buf = NULL;
        }
        urb_result_t *r = &cc->result_table[URB_RES_IDX(slot_id, ep_id)];
        r->status = -ENOENT;
        r->actual  = 0;
        wmb();
        __atomic_store_n(&r->valid, 1, __ATOMIC_RELEASE);
        notif_signal(cc->notif_cap);
        pu->valid = 0;
    }

    debug("[XHCI] Stopped endpoint %u on slot %u\n", ep_id, slot_id);
    return SUCCESS;
}

static void xhci_config_runtime_regs(xhci_t *xhci)
{
    volatile xhci_intr_reg_t *ir = &xhci->run_regs->ir_set[0];

    uint32_t iman = ir->iman;
    iman |= IMAN_IE;
    ir->iman = iman;

    xhci->event_ring = xhci_event_ring_alloc(xhci, 256, ir);

    debug("IMOD    : 0x%llx\n", ir->imod);
    debug("ERSTSZ  : 0x%llx\n", ir->erst_size);
    debug("ERSTBA  : 0x%llx\n", ir->erst_base);
    debug("ERDP    : 0x%llx\n", ir->erst_dequeue);

    _acknowledge_irq(xhci, 0);
}

static void handle_port_status(xhci_t *xhci, xhci_trb_t *trb)
{
    if (GET_COMP_CODE(trb->status) != COMP_SUCCESS)
        debug("\x1b[31m[xhci]\x1b[0m xHC returned failed port status event 0x%x\n", trb->status);
    
    uint32_t port_id = GET_PORT_ID(trb->parameter);
    uint32_t portsc = _read_portsc_reg(port_id-1);

    /* Reset any port that shows a new connection (CSC + CCS). */
    if ((portsc & PORT_CSC) && (portsc & PORT_CONNECT)) {
        portsc = xhci_port_state_to_neutral(portsc);
        portsc |= PORT_RESET;
        _write_portsc_reg(portsc, port_id-1);

        int timeout = 1000;
        while (timeout > 0) {
            portsc = _read_portsc_reg(port_id-1);
            if (!(portsc & PORT_RESET))
                break;
            timeout--;
            udelay(1000);
        }
        /* Clear all status-change bits after reset */
        portsc = xhci_port_state_to_neutral(portsc);
        portsc |= PORT_RC | PORT_PEC | PORT_CSC;
        _write_portsc_reg(portsc, port_id-1);
    } else {
        /* Disconnection or other change — clear CSC only */
        portsc = xhci_port_state_to_neutral(portsc);
        portsc |= PORT_CSC;
        _write_portsc_reg(portsc, port_id-1);
    }

    portsc = _read_portsc_reg(port_id-1);
    if (portsc & PORT_CONNECT) {
        xhci->hw_ports[port_id-1].device_connected    = 1;
        xhci->hw_ports[port_id-1].device_disconnected = 0;
        xhci->active_port = port_id - 1;
        xhci->hub_events |= XHCI_HUB_EVENT_PORT_CHANGE;
    } else {
        xhci->hw_ports[port_id-1].device_connected    = 0;
        xhci->hw_ports[port_id-1].device_disconnected = 1;
        xhci->hub_events |= XHCI_HUB_EVENT_PORT_DISCONNECT;
    }
    
    debug("\x1b[31m[xhci]\x1b[0m port 0x%x speed: %s\n", port_id, 
                _usb_speed_to_string(_get_port_speed(port_id-1)));  
                
    debug("\x1b[31m[xhci]\x1b[0m port 0x%x status: 0x%x\n", port_id, portsc); 

//    debug("\x1b[31m[xhci]\x1b[0m event status 0x%x\n", trb->status);
}

static void handle_transfer_event(xhci_t *xhci, xhci_trb_t *trb)
{
    xhci_transfer_event_t *transfer_event = (xhci_transfer_event_t *)trb;
    uint8_t slot_id = transfer_event->slot_id;
    xhci_virt_dev_t *dev = xhci->devs[slot_id];
    if (!dev) {
        debug("\x1b[31m[xhci]\x1b[0m No device for slot %u in transfer event\n", slot_id);
        return;
    }

    xhci_virt_ep_t *ep = dev->eps[transfer_event->ep_id - 1];
    xhci_ring_t *ring = ep->tr_ring;
    if (!ring) {
        debug("\x1b[31m[xhci]\x1b[0m No ring for slot %u EP%u in transfer event\n",
              slot_id, transfer_event->ep_id-1);
        return;
    }
    // Update the ring's dequeue pointer based on the TD pointer in the event
    size_t offset = transfer_event->buffer - dma_get_phys(xhci->xmem_pool, ring->trbs);
    ring->denqueue_ptr = offset / sizeof(xhci_trb_t);

    ep->comp_code = transfer_event->complition_code;
    ep->residual  = transfer_event->transfer_length;
    wmb();
    __atomic_store_n(&ep->comp_done, 1, __ATOMIC_RELEASE);

    pending_urb_t *pu = &ep->pending_urb;
    if (pu->valid) {
        class_client_t *cc = pu->client;
        uint8_t  comp  = (uint8_t)transfer_event->complition_code;
        bool     ok    = (comp == COMP_SUCCESS || comp == COMP_SHORT_PACKET);
        uint32_t actual = ok ? (pu->len - transfer_event->transfer_length) : 0;

        if (pu->dir == USB_DIR_IN && ok) {
            rmb();
            dma_cache_invalidate(xhci->xmem_pool, pu->dma_buf, pu->len);
            memcpy((uint8_t *)cc->data_shm + pu->offset, pu->dma_buf, actual);
        }
        dma_free(xhci->xmem_pool, pu->dma_buf);
        pu->dma_buf = NULL;

        urb_result_t *r = &cc->result_table[URB_RES_IDX(slot_id, transfer_event->ep_id)];
        r->status = ok ? 0 : (int8_t)(comp == COMP_STOPPED ? -ENOENT : -EIO);
        r->actual  = actual;
        wmb();
        __atomic_store_n(&r->valid, 1, __ATOMIC_RELEASE);
        notif_signal(cc->notif_cap);
        pu->valid = 0;
    }
    // debug("\x1b[31m[xhci]\x1b[0m Transfer Event: slot %u EP%u status 0x%x length %u\n", 
    //       slot_id, transfer_event->ep_id-1, transfer_event->complition_code, transfer_event->transfer_length);

    //  debug("\x1b[31m[xhci]\x1b[0m Transfer Event: trb 0x%lX TD Pointer 0x%lX\n", dma_get_phys(xhci->xmem_pool, transfer_event), transfer_event->buffer);

    // uint8_t ep_index = GET_EP_ID(trb->parameter);
    // xhci_endpoint_t *ep = dev->eps[ep_index];
    // if (!ep) {
    //     debug("\x1b[31m[xhci]\x1b[0m No endpoint %u for slot %u in transfer event\n", 
    //           ep_index, slot_id);
    //     return;
    // }

    // // Process the transfer completion
    // ep->last_transfer_status = GET_COMP_CODE(trb->status);
    // ep->last_transfer_length = trb->transfer_len;

    // debug("\x1b[31m[xhci]\x1b[0m Transfer Event: slot %u ep %u status %u length %u\n", 
    //       slot_id, ep_index, ep->last_transfer_status, ep->last_transfer_length);

    // // Signal any waiting threads that the transfer is complete
    // ep->transfer_completed = 1;
    // wmb();
    // sched_wake_up(&ep->wait_queue);
}

static void _xhci_irq_handler(uint32_t sig, uint64_t irq)
{
    _acknowledge_irq(xhci_dev, 0);

 //   debug("\x1b[31m[USB3]\x1b[0m irq handler sig %d irq %d\n", sig, irq);
    xhci_ring_t *event_ring = xhci_dev->event_ring;

    xhci_trb_t *trb;

    dma_cache_invalidate(xhci_dev->xmem_pool, event_ring->trbs, 
                            sizeof(xhci_trb_t) * event_ring->max_trb_count);
    rmb();


    while (_has_unprocessed_events(event_ring)) {               
        trb = _dequeue_trb(event_ring);
        // debug("\x1b[31m[xhci]\x1b[0m get trb 0x%lX\n", trb);  
        // debug("\x1b[31m[xhci]\x1b[0m event status 0x%x\n", trb->status);
        // debug("\x1b[31m[xhci]\x1b[0m event type 0x%x\n", trb->trb_type);
        if (!trb) {
            break;
        }
        switch (trb->trb_type) {
        case TRB_COMPLETION: {
//            debug("\x1b[31m[xhci]\x1b[0m event status %d\n", GET_COMP_CODE(trb->status));
            xhci_dev->completion_trb = (xhci_event_cmd_t *)trb;
//            debug("\x1b[31m[xhci]\x1b[0m slot id %d\n", xhci_dev->completion_trb->slot_id);
            break;
        }
        case TRB_PORT_STATUS: {
            handle_port_status(xhci_dev, trb);       
            break;
        }
        case TRB_TRANSFER: {
            handle_transfer_event(xhci_dev, trb);
//            debug("\x1b[31m[xhci]\x1b[0m Transfer Event TRB received\n");
            break;
        }
        default: break;
        }


    }
            
    __atomic_store_n(&xhci_dev->irq_completed, 1, __ATOMIC_RELEASE);

    wmb();
        _xhci_update_erdp(event_ring);

        // Clear the EHB (Event Handler Busy) bit
        volatile xhci_intr_reg_t *ir = &xhci_dev->run_regs->ir_set[0];
        uint64_t erdp = ir->erst_dequeue;
        erdp |= ERST_EHB;
        ir->erst_dequeue = erdp;
       
   irq_act(irq, 0);
}

static void xhci_roothub_ports_status(xhci_t *xhci)
{
	u32 offset;
	uint32_t st, pw, lnk;
	int i;

    for (i = 0; i < xhci->max_ports; i++) {
        offset = 0x10 * i;
        st = getreg32((uint64_t)&xhci->op_regs->port_status_base + offset);
        pw = getreg32((uint64_t)&xhci->op_regs->port_power_base + offset);
        lnk = getreg32((uint64_t)&xhci->op_regs->port_link_base + offset);
        debug("[XHCI] port %d Status 0x%X Power 0x%X Link 0x%X\n", i, st, pw, lnk);

    }

}


int xhci_init(void *base_addr, int irq)
{
    int ret;

    if (!base_addr || !irq) return -EINVAL;

    xhci_dev = (xhci_t *)malloc(sizeof(xhci_t));
    memset(xhci_dev, 0, sizeof(xhci_t));

    xhci_dev->cap_regs = (volatile struct xhci_cap_regs *)base_addr;
    uint32_t len = HC_LENGTH(xhci_dev->cap_regs->hc_capbase);
    debug("[XHCI] caps len 0x%X\n", len);
    xhci_dev->op_regs = (volatile struct xhci_op_regs *)(base_addr + len);

    xhci_dev->run_regs = (volatile struct xhci_run_regs *)(base_addr + xhci_dev->cap_regs->run_regs_off);

    xhci_dev->dba = (volatile struct xhci_doorbell_array *)(base_addr + xhci_dev->cap_regs->db_off);

    xhci_dev->hcs_params1 = xhci_dev->cap_regs->hcs_params1;
    xhci_dev->hcs_params2 = xhci_dev->cap_regs->hcs_params2;
    xhci_dev->hcs_params3 = xhci_dev->cap_regs->hcs_params3;
    xhci_dev->hcc_params = xhci_dev->cap_regs->hcc_params;
    xhci_dev->hcc_params2 = xhci_dev->cap_regs->hcc_params2;

    xhci_dev->max_ports = HCS_MAX_PORTS(xhci_dev->hcs_params1);
    xhci_dev->max_slots = HCS_MAX_SLOTS(xhci_dev->hcs_params1);

    debug("[XHCI] operational regs at 0x%lX\n", xhci_dev->op_regs);
    debug("[XHCI] init host\n");
    debug("[XHCI] xHCI version 0x%X\n", HC_VERSION(xhci_dev->cap_regs->hc_capbase));

    debug("[XHCI] xHCI max ports %d\n", xhci_dev->max_ports);
	debug("[XHCI] xHCI max slots %d\n", xhci_dev->max_slots);
	debug("[XHCI] xHCI max intps %d\n", HCS_MAX_INTRS(xhci_dev->hcs_params1));

    xhci_dev->max_scratchpads = HCS_MAX_SCRATCHPAD(xhci_dev->hcs_params1);
    debug("[XHCI] xHCI max sctatchpads %d\n", xhci_dev->max_scratchpads);

    xhci_dev->max_erst = HCS_ERST_MAX(xhci_dev->hcs_params2); 
    debug("[XHCI] xHCI max ERST %d\n", xhci_dev->max_erst);  

    debug("[XHCI] Device context is %u bytes\n", HCC_64BYTE_CONTEXT(xhci_dev->hcc_params) ? 64 : 32);
    xhci_dev->head_xcap_ptr = (volatile uint32_t *)(base_addr + (HCC_EXT_CAPS(xhci_dev->hcc_params) << 2));
    debug("[XHCI] Extra capatilities start at 0x%lX\n", xhci_dev->head_xcap_ptr);  

 //   _log_op_regs(xhci_dev);

    xhci_power_off_all_roothub_ports(xhci_dev);

    ret = xhci_reset(xhci_dev, XHCI_RESET_LONG_USEC);
    if (ret < 0)
		return ret;    

    // Enable device notifications 
    xhci_dev->op_regs->dnctrl = 0xffff;
    
    /*
	 * Program the Number of Device Slots Enabled field in the CONFIG
	 * register with the max value of slots the HC can handle.
	 */
	uint32_t val = xhci_dev->op_regs->config;
	val |= (xhci_dev->max_slots & HCS_SLOTS_MASK);
    xhci_dev->op_regs->config = val;
//	putreg32(val, (uint64_t)&xhci_dev->op_regs->config);

    ret = xhci_mem_init(xhci_dev);
    if (ret < 0)
		return ret; 

    xhci_config_runtime_regs(xhci_dev);

    // set irq handler
    signal_action_t irqhand;
    irqhand.handler = _xhci_irq_handler;
    signal_action(1, &irqhand);
    debug("[XHCI] set IRQ %d\n", irq);
    irq_set(irq, 0);

    ret = xhci_start_host();
    if (ret < 0)
		return ret;

 //   xhci_roothub_ports_status(xhci_dev);
 //   _log_op_regs(xhci_dev);
    return SUCCESS;
}



static int _parce_extended_caps()
{
    volatile struct xhci_protocol_caps *ptr = (volatile struct xhci_protocol_caps *)xhci_dev->head_xcap_ptr;
    uint32_t next = 0;

    // Allocate port structures
    xhci_port_t *ports = (xhci_port_t *)malloc(sizeof(xhci_port_t) * xhci_dev->max_ports);
    if (!ports)
        return -ENOMEM;

    memset(ports, 0, sizeof(xhci_port_t) * xhci_dev->max_ports);

    for (int i = 0; i < xhci_dev->max_ports; i++) {
        ports[i].addr = (uint32_t *)&xhci_dev->op_regs->port_status_base + (0x10 * i);
    }

    do {
        ptr = (volatile struct xhci_protocol_caps *)((uint64_t)ptr + (next << 2));
 //       debug("[XHCI] xCap ID %u next 0x%X\n", XHCI_EXT_CAPS_ID(ptr->revision), XHCI_EXT_CAPS_NEXT(ptr->revision));
        if (XHCI_EXT_CAPS_ID(ptr->revision) == XHCI_EXT_CAPS_PROTOCOL) {
            for (int i = 0; i < XHCI_EXT_PORT_COUNT(ptr->port_info); i++) {
                if ((XHCI_EXT_PORT_OFF(ptr->port_info)-1 + i) > xhci_dev->max_ports)
                        break;
                ports[(XHCI_EXT_PORT_OFF(ptr->port_info)-1 + i)].hw_portnum = i;        
                ports[(XHCI_EXT_PORT_OFF(ptr->port_info)-1 + i)].maj_rev = XHCI_EXT_PORT_MAJOR(ptr->revision);
                ports[(XHCI_EXT_PORT_OFF(ptr->port_info)-1 + i)].min_rev = XHCI_EXT_PORT_MINOR(ptr->revision);
 
                debug("[XHCI] Ext Cap USB%u.%u port %u\n", XHCI_EXT_PORT_MAJOR(ptr->revision), XHCI_EXT_PORT_MINOR(ptr->revision),
                            (XHCI_EXT_PORT_OFF(ptr->port_info)-1 + i));                
            }
        }            
        next = XHCI_EXT_CAPS_NEXT(ptr->revision);                
    } while(next);   

    xhci_dev->hw_ports = ports;
    
    return SUCCESS;
}



int xhci_start_host()
{

    uint32_t cmd = xhci_dev->op_regs->usbcmd;
    cmd |= CMD_RUN;
    cmd |= CMD_EIE;
    xhci_dev->op_regs->usbcmd = cmd;

    int ret = xhci_handshake(&xhci_dev->op_regs->usbsts, STS_HALT, 0, XHCI_MAX_HALT_USEC);
    if (ret < 0) return ret;

    ret = xhci_handshake(&xhci_dev->op_regs->usbsts, STS_CNR, 0, XHCI_MAX_HALT_USEC);
    if (ret < 0)
            return ret;

    _parce_extended_caps();

    return ret;
}

void _dump_device_context(xhci_virt_dev_t *dev)
{
    uint32_t ctx_size = HCC_64BYTE_CONTEXT(xhci_dev->hcc_params) ? 64 : 32;
    dma_cache_invalidate(xhci_dev->xmem_pool, dev->in_ctx, 
                            32 * ctx_size);
    dma_cache_invalidate(xhci_dev->xmem_pool, dev->out_ctx, 
                            32 * ctx_size);
    rmb();
    

    debug("[XHCI] Device Context Dump:\n");
    debug("Input Context:\n");
    for (int i = 0; i < 6 * (ctx_size / 4); i++) {
        debug(" in_ctx[%d] = 0x%X\n", i, ((uint32_t *)dev->in_ctx)[i]);
    }

    debug("Output Context:\n");
    for (int i = 0; i < 6 * (ctx_size / 4); i++) {
        debug(" out_ctx[%d] = 0x%X\n", i, ((uint32_t *)dev->out_ctx)[i]);
    }
}

int xhci_enumerate_device(uint8_t slot_id)
{
    usb_device_descriptor_t dev_desc;
    usb_config_descriptor_t config_desc;
    int ret;
    
    debug("[XHCI] Starting device enumeration for slot %u\n", slot_id);
    
    // Get the first 8 bytes of the device descriptor to determine max packet size
    ret = xhci_get_descriptor(xhci_dev, slot_id, USB_DT_DEVICE, 0, 0, 
                             &dev_desc, 8, NULL);
    if (ret != SUCCESS) {
        debug("[XHCI] Failed to get initial device descriptor for slot %u\n", slot_id);
        return ret;
    }

    uint16_t max_packet_size = (dev_desc.bMaxPacketSize0 == 9) ? 512 : dev_desc.bMaxPacketSize0;

    debug("[XHCI] Initial Device Descriptor for slot %u:\n", slot_id);
    debug("  Max Packet Size EP0: %u\n", max_packet_size); 
    

    // Update the endpoint 0 max packet size
    xhci_virt_dev_t *dev = xhci_dev->devs[slot_id];
    if (!dev) {
        debug("[XHCI] No device context for slot %u\n", slot_id);
        return -EINVAL;
    }

    uint32_t *in_ctx = (uint32_t *)dev->in_ctx;
    uint32_t ctx_size = HCC_64BYTE_CONTEXT(xhci_dev->hcc_params) ? 64 : 32;
    
// //    in_ctx[0] = 1<<1; 

    // Endpoint 0 Context (starts at 2*ctx_size offset)
    uint32_t *ep0_ctx = &in_ctx[2 *(ctx_size/4)];
    xhci_ep_ctx_t *ep0_context = (xhci_ep_ctx_t *)ep0_ctx;

    ep0_context->ep_info2 &= ~MAX_PACKET_MASK;
    ep0_context->ep_info2 |= MAX_PACKET(max_packet_size);

    wmb();
    // Flush the input context to DMA memory
    cache_flush((void *)dma_get_phys(xhci_dev->xmem_pool, dev->in_ctx), 3 * ctx_size);

//     xhci_evaluate_context(slot_id);

    ret = xhci_address_device(slot_id, false);
    if (ret != SUCCESS) {
        debug("[XHCI] Failed to address device for slot %u\n", slot_id);
        return ret;
    }
//    _dump_device_context(xhci_dev->devs[slot_id]);


    // Get device descriptor
    ret = xhci_get_descriptor(xhci_dev, slot_id, USB_DT_DEVICE, 0, 0, 
                             &dev_desc, sizeof(dev_desc), NULL);
    if (ret != SUCCESS) {
        debug("[XHCI] Failed to get device descriptor for slot %u\n", slot_id);
        return ret;
    }
    
    // Print device descriptor information
    debug("[XHCI] Device Descriptor for slot %u:\n", slot_id);
    debug("  USB Version: %x.%02x\n", (dev_desc.bcdUSB >> 8) & 0xFF, dev_desc.bcdUSB & 0xFF);
    debug("  Device Class: 0x%02x\n", dev_desc.bDeviceClass);
    debug("  Device SubClass: 0x%02x\n", dev_desc.bDeviceSubClass);
    debug("  Device Protocol: 0x%02x\n", dev_desc.bDeviceProtocol);
    debug("  Max Packet Size EP0: %u\n", dev_desc.bMaxPacketSize0);
    debug("  Vendor ID: 0x%04x\n", dev_desc.idVendor);
    debug("  Product ID: 0x%04x\n", dev_desc.idProduct);
    debug("  Device Version: %x.%02x\n", (dev_desc.bcdDevice >> 8) & 0xFF, dev_desc.bcdDevice & 0xFF);
    debug("  Manufacturer String: %u\n", dev_desc.iManufacturer);
    debug("  Product String: %u\n", dev_desc.iProduct);
    debug("  Serial Number String: %u\n", dev_desc.iSerialNumber);
    debug("  Number of Configurations: %u\n", dev_desc.bNumConfigurations);
    
    // Get and print string descriptors
    uint16_t lang_id = 0x0409; // Default to English (US)
    
    // Try to get supported languages from string descriptor 0
    usb_string_descriptor_t lang_desc;
    ret = xhci_get_descriptor(xhci_dev, slot_id, USB_DT_STRING, 0, 0, 
                             &lang_desc, sizeof(lang_desc), NULL);

    if (ret == SUCCESS && lang_desc.bLength >= 4) {
        // Use the first language ID from the list
        lang_id = lang_desc.wData[0];
        debug("[XHCI] Using language ID: 0x%04x\n", lang_id);
    }
    
    // Get manufacturer string
    if (dev_desc.iManufacturer != 0) {
        char manufacturer[256];
        ret = xhci_get_descriptor(xhci_dev, slot_id, USB_DT_STRING, dev_desc.iManufacturer, lang_id,
                                 manufacturer, sizeof(manufacturer), NULL);
        if (ret == SUCCESS) {
            debug("[XHCI] Manufacturer String: ");
            print_utf16_string((const uint16_t*)(manufacturer + 2), (sizeof(manufacturer) - 2) / 2);
        } else {
            debug("[XHCI] Failed to get manufacturer string\n");
        }
    }
    
    // Get product string
    if (dev_desc.iProduct != 0) {
        char product[256];
        ret = xhci_get_descriptor(xhci_dev, slot_id, USB_DT_STRING, dev_desc.iProduct, lang_id,
                                 product, sizeof(product), NULL);
        if (ret == SUCCESS) {
            debug("[XHCI] Product String: ");
            print_utf16_string((const uint16_t*)(product + 2), (sizeof(product) - 2) / 2);
        } else {
            debug("[XHCI] Failed to get product string\n");
        }
    }
    
    // Get serial number string
    if (dev_desc.iSerialNumber != 0) {
        char serial[256];
        ret = xhci_get_descriptor(xhci_dev, slot_id, USB_DT_STRING, dev_desc.iSerialNumber, lang_id,
                                 serial, sizeof(serial), NULL);
        if (ret == SUCCESS) {
            debug("[XHCI] Serial Number String: ");
            print_utf16_string((const uint16_t*)(serial + 2), (sizeof(serial) - 2) / 2);
        } else {
            debug("[XHCI] Failed to get serial number string\n");
        }
    }
    
    // Get configuration descriptor (first one)
    if (dev_desc.bNumConfigurations > 0) {
        ret = xhci_get_descriptor(xhci_dev, slot_id, USB_DT_CONFIG, 0, 0,
                                 &config_desc, sizeof(config_desc), NULL);
        if (ret != SUCCESS) {
            debug("[XHCI] Failed to get configuration descriptor for slot %u\n", slot_id);
            return ret;
        }
        
        // Print configuration descriptor information
        debug("[XHCI] Configuration Descriptor for slot %u:\n", slot_id);
        debug("  Total Length: %u\n", config_desc.wTotalLength);
        debug("  Number of Interfaces: %u\n", config_desc.bNumInterfaces);
        debug("  Configuration Value: %u\n", config_desc.bConfigurationValue);
        debug("  Configuration String: %u\n", config_desc.iConfiguration);
        debug("  Attributes: 0x%02x\n", config_desc.bmAttributes);
        debug("  Max Power: %u mA\n", config_desc.bMaxPower * 2);

   
        // Get the full configuration descriptor including all interfaces and endpoints
        uint8_t *config_buffer = (uint8_t *)malloc(config_desc.wTotalLength);
        if (!config_buffer) {
            debug("[XHCI] Failed to allocate buffer for full configuration descriptor\n");
            return -ENOMEM;
        }
        
        ret = xhci_get_descriptor(xhci_dev, slot_id, USB_DT_CONFIG, 0, 0,
                                 config_buffer, config_desc.wTotalLength, NULL);
        if (ret != SUCCESS) {
            debug("[XHCI] Failed to get full configuration descriptor for slot %u\n", slot_id);
            free(config_buffer);
            return ret;
        }
        
        // Parse interfaces and endpoints
        uint8_t *ptr = config_buffer + sizeof(usb_config_descriptor_t);
        uint16_t remaining = config_desc.wTotalLength - sizeof(usb_config_descriptor_t);
        
        while (remaining >= 2) {  // Need at least length and type
            uint8_t desc_length = ptr[0];
            uint8_t desc_type = ptr[1];
            
            if (desc_length < 2 || desc_length > remaining) {
                break;  // Invalid descriptor
            }
            
            if (desc_type == USB_DT_INTERFACE) {
                usb_interface_descriptor_t *iface_desc = (usb_interface_descriptor_t *)ptr;
                debug("[XHCI] Interface %u: Class=0x%02x, SubClass=0x%02x, Protocol=0x%02x, Endpoints=%u\n",
                      iface_desc->bInterfaceNumber, iface_desc->bInterfaceClass,
                      iface_desc->bInterfaceSubClass, iface_desc->bInterfaceProtocol,
                      iface_desc->bNumEndpoints);
            } else if (desc_type == USB_DT_ENDPOINT) {
                usb_endpoint_descriptor_t *ep_desc = (usb_endpoint_descriptor_t *)ptr;
                uint8_t ep_addr = ep_desc->bEndpointAddress;
                uint8_t ep_num = ep_addr & 0x0F;

                uint8_t ep_type = ep_desc->bmAttributes & 0x03;
                
                debug("[XHCI] Endpoint 0x%02x: Type=%s, MaxPacket=%u, Interval=%u\n",
                      ep_addr,
                      ep_type == USB_ENDPOINT_XFER_BULK ? "Bulk" :
                      ep_type == USB_ENDPOINT_XFER_INT ? "Interrupt" :
                      ep_type == USB_ENDPOINT_XFER_ISOC ? "Isochronous" : "Control",
                      ep_desc->wMaxPacketSize, ep_desc->bInterval);
                
                // Allocate transfer ring for bulk endpoints
                if (ep_type == USB_ENDPOINT_XFER_BULK) {
                    xhci_virt_dev_t *dev = xhci_dev->devs[slot_id];
                    if (dev && ep_num > 0 && ep_num < EP_CTX_PER_DEV) {
                        if (!dev->eps[ep_num]->tr_ring) {
                            dev->eps[ep_num]->tr_ring = xhci_alloc_transfer_ring(xhci_dev, 256);
                            if (dev->eps[ep_num]->tr_ring) {
                                debug("[XHCI] Allocated transfer ring for EP%u on slot %u\n", ep_num, slot_id);
                            } else {
                                debug("[XHCI] Failed to allocate transfer ring for EP%u on slot %u\n", ep_num, slot_id);
                            }
                        }
                    }
                }
            }    
            ptr += desc_length;
            remaining -= desc_length;
         
        }
        
        free(config_buffer);

        // Set the first configuration
        ret = xhci_set_configuration(xhci_dev, slot_id, config_desc.bConfigurationValue);
        if (ret != SUCCESS) {
            debug("[XHCI] Failed to set configuration %u for slot %u\n", 
                  config_desc.bConfigurationValue, slot_id);
            return ret;
        }
        
        debug("[XHCI] Successfully set configuration %u for slot %u\n", 
              config_desc.bConfigurationValue, slot_id);
    }
    
    debug("[XHCI] Device enumeration completed for slot %u\n", slot_id);
    return SUCCESS;
}

void xhci_hub_events(void)
{
    int ret;

    if (xhci_dev->hub_events & XHCI_HUB_EVENT_PORT_CHANGE) {
        for (int i = 0; i < xhci_dev->max_ports; i++) {
            if (xhci_dev->hw_ports[i].device_connected) {
                usb_host_signal_t sig = {
                    .host_id = usb_host_id,
                    .port_id = i,
                    .state   = USB_PORT_STATE_CONNECTED,
                };
                ret = signal_send(usb_core_pid, SIGNAL_USER_BASE, sig.signal);
                if (ret < 0)
                    return;
                debug("[XHCI] Port %u device connected\n", i + 1);
                xhci_dev->hw_ports[i].device_connected = 0;
            }
        }
        xhci_dev->hub_events &= ~XHCI_HUB_EVENT_PORT_CHANGE;
    }

    if (xhci_dev->hub_events & XHCI_HUB_EVENT_PORT_DISCONNECT) {
        for (int i = 0; i < xhci_dev->max_ports; i++) {
            if (xhci_dev->hw_ports[i].device_disconnected) {
                usb_host_signal_t sig = {
                    .host_id = usb_host_id,
                    .port_id = i,
                    .state   = USB_PORT_STATE_DISCONNECTED,
                };
                ret = signal_send(usb_core_pid, SIGNAL_USER_BASE, sig.signal);
                if (ret < 0)
                    return;
                debug("[XHCI] Port %u device disconnected\n", i + 1);
                xhci_dev->hw_ports[i].device_disconnected = 0;
            }
        }
        xhci_dev->hub_events &= ~XHCI_HUB_EVENT_PORT_DISCONNECT;
    }
}
