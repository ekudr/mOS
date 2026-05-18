#include <mosstd.h>
#include <cap.h>
#include <libsys/ipc.h>
#include <ipc.h>
#include <vfs.h>
#include <devman.h>
#include <sched.h>
#include <signals.h>
#include <string.h>
#include <libsys/barrier.h>

#include <libsys/usb/usb-ipc.h>
#include "xhci.h"
#include "main.h"


void handle_ctrl_transfer_request(uint64_t sender, uint64_t info)
{
    int ret;
    msg_info_t ret_info;
    // Handle control transfer request from USB device
    // Extract necessary information from IPC message

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;

    // debug("[XHCI] Control Transfer Request from sender 0x%lX: slot_id %u, data_len %u\n",
    //       sender, cmd->ctrl.slot_id, cmd->ctrl.data_len);

    if (cmd->ctrl.data_len > buf_size) {
        debug("[XHCI] Control transfer data length %u exceeds buffer size %u\n",
              cmd->ctrl.data_len, buf_size);
        // Reply with error
        ret = -EINVAL; // Error code
        goto reply_error;
    }

    usb_control_request_t *req = (usb_control_request_t *)cmd->ctrl.setup_pkt;

    ret = xhci_control_transfer(xhci_dev, cmd->ctrl.slot_id, req, 
                            buf, cmd->ctrl.data_len);
    if (ret < 0) {
        debug("[XHCI] Control transfer failed for slot %u\n", cmd->ctrl.slot_id);
        // Reply with error
        goto reply_error;
    }

//    _dump_device_context(xhci_dev->devs[cmd->ctrl.slot_id]);

    // For now, just reply with success
    ipc_setMR(0, 0); // Success
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
    return;

reply_error:
        ipc_setMR(0, ret); // Error code
        ret_info = msginfo_word_new(0, 1, 0, 0);
        ipc_reply(ret_info);
}

// int _handle_new_device(uint8_t port_id)
// {
//     debug("\x1b[31m[xhci]\x1b[0m New device connected on port 0x%x speed: %s\n", port_id, 
//                 _usb_speed_to_string(_get_port_speed(port_id)));  

//     uint8_t slot_id = xhci_enable_device_slot();
//     if (slot_id)
//             debug("[XHCI] slot_id:%u enabled\n", slot_id);
//     debug("[XHCI] slot_id:%u active port:%u allocating device context\n",
//                      slot_id, xhci_dev->active_port);
//     int ret = xhci_alloc_virt_device(xhci_dev, slot_id, port_id, 0, 0, 0);
//     if (ret < 0) {
//             debug("[XHCI] slot_id:%u can not allocate device context\n", slot_id);
//             xhci_dev->hub_events &= ~XHCI_HUB_EVENT_PORT_CHANGE;
//             return ret;
//     }

//     debug("\x1b[31m[xhci]\x1b[0m Allocated slot ID %u for port 0x%x\n", slot_id, port_id);  

//     ret = xhci_address_device(slot_id, false);
//     if (ret != SUCCESS) {
//         debug("\x1b[31m[xhci]\x1b[0m Failed to address device on slot %u\n", slot_id);  
//         return ret;
//     }

//     debug("\x1b[31m[xhci]\x1b[0m Device addressed on slot %u\n", slot_id);  

//     xhci_slot_ctx_t *slot_ctx = xhci_dev->devs[slot_id]->out_ctx;

//     uint8_t dev_address = slot_ctx->dev_state & DEV_ADDR_MASK;

//     usb_core_cmd_t *cmd = (usb_core_cmd_t *)get_ipc_buffer()->msg;

//     // Send IPC to usb-core to register new device
//     cmd->src = IPC_SRC_HOST;
//     cmd->cmd_type = IPC_DEVICE_CONNECT;
//     cmd->new_dev.host_id = usb_host_id;
//     cmd->new_dev.slot_id = slot_id;
//     cmd->new_dev.address = dev_address;
//     cmd->new_dev.speed   = _get_port_speed(port_id);
//     cmd->new_dev.port    = port_id;
//     cmd->new_dev.state   = GET_SLOT_STATE(slot_ctx->dev_state);

//     msg_info_t info = msginfo_word_new(0, sizeof(usb_core_cmd_t)/8, 0, 0);

//     signal_send(usb_core_pid, SIGNAL_USER_BASE, ((uint64_t)usb_host_id << 8 | port_id));
    
//  //   info = ipc_call(usb_core_cap, info);
//     ipc_send(usb_core_cap, info); 
//     // ret = (int)label_from_msginfo_word(info);
//     // if (ret < 0 || !length_from_msginfo_word(info)) {
//     //     debug("[XHCI] Error registring host %d info 0x%lX\n", ret, info);
//     //     return ret;
//     // }
//     // ret = ipc_getMR(0);
//     //     if (ret < 0) {
//     //         debug("[XHCI] usb-core failed to register new device on slot %u\n", slot_id);
//     //         return ret;
//     //     }
//     // debug("\x1b[31m[xhci]\x1b[0m Device registration with usb-core on host ID %d slot ID %d\n",
//     //             usb_host_id, slot_id);

//     debug("[XHCI] Registration request sent to usb-core for slot ID %d\n", slot_id);
//     return ret;

// }

void handle_new_device(uint64_t sender, uint64_t info)
{
    int ret;
    msg_info_t ret_info;

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;

    uint8_t  slot_id      = cmd->new_dev.slot_id;
    uint8_t  port_id      = cmd->new_dev.port;
    uint8_t  parent_slot  = cmd->new_dev.parent_slot;
    uint32_t route_string = cmd->new_dev.route_string;
    uint8_t  speed        = cmd->new_dev.speed;
    uint8_t  tt_slot      = cmd->new_dev.tt_slot;
    uint8_t  tt_port      = cmd->new_dev.hub_port;

    ret = xhci_alloc_virt_device(xhci_dev, slot_id, port_id, parent_slot, route_string, speed, tt_slot, tt_port);
    if (ret < 0) {
        debug("[XHCI] slot_id:%u can not allocate device context\n", slot_id);
        goto reply_error;
    }

    ret = xhci_address_device(slot_id, true);
    if (ret != SUCCESS) {
        debug("\x1b[31m[xhci]\x1b[0m Failed to address device on slot %u\n", slot_id);  
        goto reply_error;
    }


    xhci_slot_ctx_t *slot_ctx = xhci_dev->devs[slot_id]->out_ctx;

    memset(cmd, 0, sizeof(*cmd));

    // Send IPC reply to usb-core
    cmd->cmd_type = IPC_NEW_DEVICE;
    cmd->new_dev.host_id = usb_host_id;
    cmd->new_dev.slot_id = slot_id;
    cmd->new_dev.speed   = _get_port_speed(port_id);
    cmd->new_dev.port    = port_id;
    cmd->new_dev.state   = GET_SLOT_STATE(slot_ctx->dev_state);

    ret_info = msginfo_word_new(0, sizeof(usb_host_cmd_t)/8, 0, 0);
    ipc_reply(ret_info);
    return;

reply_error:
    ipc_setMR(0, ret); // Error code
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
}

void handle_enable_slot(uint64_t sender, uint64_t info)
{
    int ret;
    msg_info_t ret_info;

    uint8_t slot_id = xhci_enable_device_slot();
    if (slot_id == 0) {
        debug("[XHCI] Enable slot failed\n");
        ret = -EIO;
        goto reply_error;
    }

    debug("[XHCI] Enabled device slot ID %u\n", slot_id);

    ipc_setMR(0, slot_id); // Success, return slot ID
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
    return;

reply_error:
        ipc_setMR(0, ret); // Error code
        ret_info = msginfo_word_new(0, 1, 0, 0);
        ipc_reply(ret_info);
}

void handle_disable_slot(uint64_t sender, uint64_t info)
{
    int ret;
    msg_info_t ret_info;

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    uint8_t slot_id = cmd->new_dev.slot_id;

    ret = xhci_disable_device_slot(slot_id);
    if (ret < 0) {
        debug("[XHCI] Disable slot failed for slot %u\n", slot_id);
        goto reply_error;
    }

    xhci_free_virt_device(xhci_dev, slot_id);

    ipc_setMR(0, 0);
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
    return;

reply_error:
    ipc_setMR(0, ret);
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
}

void handle_stop_ep(uint64_t sender, uint64_t info)
{
    int ret;
    msg_info_t ret_info;

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    uint8_t slot_id = cmd->stop_ep.slot_id;
    uint8_t ep_id   = cmd->stop_ep.ep_id;

    ret = xhci_stop_ep(slot_id, ep_id);
    if (ret < 0)
        debug("[XHCI] Stop EP %u failed for slot %u\n", ep_id, slot_id);
    /* reply even on failure — usb-core must proceed with teardown */

    ipc_setMR(0, ret);
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
}

void handle_address_device(uint64_t sender, uint64_t info)
{
    int ret;
    msg_info_t ret_info;

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    uint8_t slot_id = cmd->new_dev.slot_id;

    ret = xhci_address_device(slot_id, false);
    if (ret != SUCCESS) {
        debug("[XHCI] Address Device (BSR=false) failed for slot %u\n", slot_id);
        goto reply_error;
    }

    debug("[XHCI] Device addressed (BSR=false) slot %u\n", slot_id);
    ipc_setMR(0, 0);
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
    return;

reply_error:
    ipc_setMR(0, ret);
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
}

void handle_update_ep0_mps(uint64_t sender, uint64_t info)
{
    int ret;
    msg_info_t ret_info;

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    uint8_t slot_id = cmd->update_ep0_mps.slot_id;
    uint16_t mps    = cmd->update_ep0_mps.mps;

    xhci_virt_dev_t *dev = xhci_dev->devs[slot_id];
    if (!dev) {
        debug("[XHCI] No device context for slot %u (update_ep0_mps)\n", slot_id);
        ret = -EINVAL;
        goto reply_error;
    }

    uint32_t ctx_size = HCC_64BYTE_CONTEXT(xhci_dev->hcc_params) ? 64 : 32;

    xhci_input_control_ctx_t *in_ctrl = (xhci_input_control_ctx_t *)dev->in_ctx;
    in_ctrl->drop_flags = 0;
    in_ctrl->add_flags  = EP0_FLAG;   /* evaluate EP0 context only */

    xhci_ep_ctx_t *ep0_ctx = (xhci_ep_ctx_t *)((uint8_t *)dev->in_ctx + 2 * ctx_size);
    ep0_ctx->ep_info2 = (ep0_ctx->ep_info2 & ~MAX_PACKET_MASK) | MAX_PACKET(mps);

    wmb();
    dma_cache_flush(xhci_dev->xmem_pool, dev->in_ctx, 3 * ctx_size);

    ret = xhci_evaluate_context(slot_id);
    if (ret != SUCCESS) {
        debug("[XHCI] Evaluate Context failed for slot %u mps %u\n", slot_id, mps);
        goto reply_error;
    }

    debug("[XHCI] EP0 mps updated to %u for slot %u\n", mps, slot_id);
    ipc_setMR(0, 0);
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
    return;

reply_error:
    ipc_setMR(0, ret);
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
}

void handle_xfer_submit(uint64_t sender, uint64_t info)
{
    int ret;
    msg_info_t ret_info;

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    uint8_t  slot_id = cmd->xfer.slot_id;
    uint8_t  ep_id   = cmd->xfer.ep_id;
    uint8_t  data_in = (cmd->xfer.dir == USB_DIR_IN);
    uint32_t len     = cmd->xfer.len;

    if (!len || len > buf_size) {
        debug("[XHCI] XFER_SUBMIT invalid len %u (buf_size %u)\n", len, buf_size);
        ret = -EINVAL;
        goto reply_error;
    }

    void *dma_buf = dma_alloc(xhci_dev->xmem_pool, len, 64);
    if (!dma_buf) {
        debug("[XHCI] XFER_SUBMIT cannot alloc DMA buf len %u\n", len);
        ret = -ENOMEM;
        goto reply_error;
    }

    if (!data_in) {
        /* OUT: copy data from shared buffer into DMA buffer */
        memcpy(dma_buf, buf, len);
        wmb();
        dma_cache_flush(xhci_dev->xmem_pool, dma_buf, len);
    } else {
        /* IN: clear DMA buffer, flush so device sees zeroes before DMA */
        memset(dma_buf, 0, len);
        wmb();
        dma_cache_flush(xhci_dev->xmem_pool, dma_buf, len);
    }

    uint32_t residual = 0;
    int comp = xhci_submit_bulk_transfer(xhci_dev, slot_id, ep_id,
                                         dma_buf, len, (bool)data_in, &residual);

    if (comp == COMP_STALL_ERROR) {
        debug("[XHCI] XFER_SUBMIT STALL slot %u ep %u — resetting EP\n", slot_id, ep_id);
        xhci_reset_ep(slot_id, ep_id);
        dma_free(xhci_dev->xmem_pool, dma_buf);
        ipc_setMR(0, -EBUSY);
        ipc_setMR(1, 0);
        ret_info = msginfo_word_new(0, 2, 0, 0);
        ipc_reply(ret_info);
        return;
    }

    if (comp < 0) {
        /* Device disconnected or other error set by stop_ep */
        dma_free(xhci_dev->xmem_pool, dma_buf);
        ret = comp;
        goto reply_error;
    }

    bool ok = (comp == COMP_SUCCESS || comp == COMP_SHORT_PACKET);
    uint32_t actual = ok ? (len - residual) : 0;

    if (data_in && ok) {
        rmb();
        dma_cache_invalidate(xhci_dev->xmem_pool, dma_buf, len);
        memcpy(buf, dma_buf, actual);
    }

    // debug("[XHCI] XFER_SUBMIT slot %u ep %u dir %s len %u actual %u comp %d\n",
    //       slot_id, ep_id, data_in ? "IN" : "OUT", len, actual, comp);

    dma_free(xhci_dev->xmem_pool, dma_buf);

    ipc_setMR(0, ok ? 0 : -EIO);
    ipc_setMR(1, actual);
    ret_info = msginfo_word_new(0, 2, 0, 0);
    ipc_reply(ret_info);
    return;

reply_error:
    ipc_setMR(0, ret);
    ipc_setMR(1, 0);
    ret_info = msginfo_word_new(0, 2, 0, 0);
    ipc_reply(ret_info);
}

static class_client_t *find_client_by_slot(uint8_t slot_id)
{
    for (int i = 0; i < MAX_CLASS_CLIENTS; i++) {
        if (xhci_dev->class_clients[i].slot_id == slot_id)
            return &xhci_dev->class_clients[i];
    }
    return NULL;
}

void handle_register_client(uint64_t sender, uint64_t info)
{
    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    uint8_t  slot_id    = cmd->register_client.slot_id;
    // notif_cap is granted by usb-core with badge=1 (not IPC-transferred)
    cap_id_t notif_cap  = (cap_id_t)cmd->register_client.notif_cap;

    cap_id_t data_shm_cap   = (cap_id_t)ipc_get_cap(0);
    cap_id_t result_shm_cap = (cap_id_t)ipc_get_cap(1);

    for (int i = 0; i < MAX_CLASS_CLIENTS; i++) {
        class_client_t *cc = &xhci_dev->class_clients[i];
        if (cc->slot_id != 0)
            continue;

        cc->slot_id        = slot_id;
        cc->notif_cap      = notif_cap;
        cc->data_shm_cap   = data_shm_cap;
        cc->data_shm       = ipc_shm_attach(data_shm_cap, NULL, 0);
        cc->result_shm_cap = result_shm_cap;
        cc->result_table   = (urb_result_t *)ipc_shm_attach(result_shm_cap, NULL, 0);

        debug("[XHCI] Registered class client slot=%u idx=%d notif=0x%x data=%p result=%p\n",
              slot_id, i, notif_cap, cc->data_shm, cc->result_table);
        ipc_setMR(0, 0);
        ipc_reply(msginfo_word_new(0, 1, 0, 0));
        return;
    }

    debug("[XHCI] No free class client slot\n");
    ipc_setMR(0, -ENOSPC);
    ipc_reply(msginfo_word_new(0, 1, 0, 0));
}

void handle_unregister_client(uint64_t sender, uint64_t info)
{
    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    uint8_t slot_id = cmd->register_client.slot_id;

    class_client_t *cc = find_client_by_slot(slot_id);
    if (cc) {
        memset(cc, 0, sizeof(*cc));
        debug("[XHCI] Unregistered class client slot=%u\n", slot_id);
    }

    ipc_setMR(0, 0);
    ipc_reply(msginfo_word_new(0, 1, 0, 0));
}

void handle_direct_xfer_submit(uint64_t sender, uint64_t info)
{
    int ret;
    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;
    uint8_t  slot_id = cmd->direct_xfer.slot_id;
    uint8_t  ep_id   = cmd->direct_xfer.ep_id;
    uint8_t  dir     = cmd->direct_xfer.dir;
    uint32_t len     = cmd->direct_xfer.len;
    uint32_t offset  = cmd->direct_xfer.offset;

    class_client_t *cc = find_client_by_slot(slot_id);
    if (!cc) {
        ipc_setMR(0, -EPERM);
        ipc_reply(msginfo_word_new(0, 1, 0, 0));
        return;
    }

    xhci_virt_dev_t *dev = xhci_dev->devs[slot_id];
    if (!dev || !dev->eps[ep_id - 1] || !dev->eps[ep_id - 1]->tr_ring) {
        ipc_setMR(0, -EINVAL);
        ipc_reply(msginfo_word_new(0, 1, 0, 0));
        return;
    }

    xhci_virt_ep_t *ep = dev->eps[ep_id - 1];
    if (ep->pending_urb.valid) {
        ipc_setMR(0, -EBUSY);
        ipc_reply(msginfo_word_new(0, 1, 0, 0));
        return;
    }

    void *dma_buf = dma_alloc(xhci_dev->xmem_pool, len, 64);
    if (!dma_buf) {
        ipc_setMR(0, -ENOMEM);
        ipc_reply(msginfo_word_new(0, 1, 0, 0));
        return;
    }

    if (dir == USB_DIR_OUT) {
        memcpy(dma_buf, (uint8_t *)cc->data_shm + offset, len);
        wmb();
        dma_cache_flush(xhci_dev->xmem_pool, dma_buf, len);
    } else {
        memset(dma_buf, 0, len);
        wmb();
        dma_cache_flush(xhci_dev->xmem_pool, dma_buf, len);
    }

    ep->pending_urb.valid   = 1;
    ep->pending_urb.slot_id = slot_id;
    ep->pending_urb.ep_id   = ep_id;
    ep->pending_urb.dir     = dir;
    ep->pending_urb.len     = len;
    ep->pending_urb.offset  = offset;
    ep->pending_urb.dma_buf = dma_buf;
    ep->pending_urb.client  = cc;

    ret = xhci_arm_bulk_transfer(xhci_dev, slot_id, ep_id, dma_buf, len,
                                  (dir == USB_DIR_IN));
    if (ret < 0) {
        dma_free(xhci_dev->xmem_pool, dma_buf);
        ep->pending_urb.valid = 0;
    }

    ipc_setMR(0, ret);
    ipc_reply(msginfo_word_new(0, 1, 0, 0));
}

void handle_config_ep(uint64_t sender, uint64_t info)
{
    int ret;
    msg_info_t ret_info;

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;

    xhci_virt_dev_t *dev = xhci_dev->devs[cmd->config_ep.slot_id];
    if (!dev) {
        debug("[XHCI] No device context for slot %u\n", cmd->config_ep.slot_id);
        // Reply with error
        ret = -EINVAL;
        goto reply_error;
    }

//_dump_device_context(dev);
    

    debug("[XHCI] Configure Endpoints Request from sender 0x%lX: slot_id %u, ep_nums %u\n",
          sender, cmd->config_ep.slot_id, cmd->config_ep.ep_nums);

    ret = xhci_config_eps(xhci_dev, cmd->config_ep.slot_id,
                                   cmd->config_ep.ep_nums,
                                   cmd->config_ep.ep_desc);
    if (ret < 0) {
        debug("[XHCI] Configure endpoints failed for slot %u\n", cmd->config_ep.slot_id);
        // Reply with error
        goto reply_error;
    }

//   _dump_device_context(dev);

    xhci_slot_ctx_t *slot_ctx = (xhci_slot_ctx_t *)dev->out_ctx;
    dma_cache_invalidate(xhci_dev->xmem_pool, dev->out_ctx, sizeof(xhci_slot_ctx_t)*31);
    debug("[XHCI] Device slot %u configured, state: 0x%X, address: %u\n",
          cmd->config_ep.slot_id, GET_SLOT_STATE(slot_ctx->dev_state), slot_ctx->dev_state & DEV_ADDR_MASK);
    
    ipc_setMR(0, GET_SLOT_STATE(slot_ctx->dev_state)); // Success
    ret_info = msginfo_word_new(0, 1, 0, 0);
    ipc_reply(ret_info);
    return;

reply_error:
        ipc_setMR(0, ret); // Error code
        ret_info = msginfo_word_new(0, 1, 0, 0);
        ipc_reply(ret_info);
}
