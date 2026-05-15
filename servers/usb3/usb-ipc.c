#include <mosstd.h>
#include <cap.h>
#include <libsys/ipc.h>
#include <vfs.h>
#include <devman.h>
#include <sched.h>
#include <signals.h>
#include <string.h>

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

    debug("[XHCI] Control Transfer Request from sender 0x%lX: slot_id %u, data_len %u\n",
          sender, cmd->ctrl.slot_id, cmd->ctrl.data_len);

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

int _handle_new_device(uint8_t port_id)
{
    debug("\x1b[31m[xhci]\x1b[0m New device connected on port 0x%x speed: %s\n", port_id, 
                _usb_speed_to_string(_get_port_speed(port_id)));  

    uint8_t slot_id = xhci_enable_device_slot();
    if (slot_id)
            debug("[XHCI] slot_id:%u enabled\n", slot_id);
    debug("[XHCI] slot_id:%u active port:%u allocating device context\n",
                     slot_id, xhci_dev->active_port);
    int ret = xhci_alloc_virt_device(xhci_dev, slot_id, port_id);
    if (ret < 0) {
            debug("[XHCI] slot_id:%u can not allocate device context\n", slot_id);
            xhci_dev->hub_events &= ~XHCI_HUB_EVENT_PORT_CHANGE;
            return ret;
    }

    debug("\x1b[31m[xhci]\x1b[0m Allocated slot ID %u for port 0x%x\n", slot_id, port_id);  

    ret = xhci_address_device(slot_id, false);
    if (ret != SUCCESS) {
        debug("\x1b[31m[xhci]\x1b[0m Failed to address device on slot %u\n", slot_id);  
        return ret;
    }

    debug("\x1b[31m[xhci]\x1b[0m Device addressed on slot %u\n", slot_id);  

    xhci_slot_ctx_t *slot_ctx = xhci_dev->devs[slot_id]->out_ctx;

    uint8_t dev_address = slot_ctx->dev_state & DEV_ADDR_MASK;

    usb_core_cmd_t *cmd = (usb_core_cmd_t *)get_ipc_buffer()->msg;

    // Send IPC to usb-core to register new device
    cmd->src = IPC_SRC_HOST;
    cmd->cmd_type = IPC_DEVICE_CONNECT;
    cmd->new_dev.host_id = usb_host_id;
    cmd->new_dev.slot_id = slot_id;
    cmd->new_dev.address = dev_address;
    cmd->new_dev.speed   = _get_port_speed(port_id);
    cmd->new_dev.port    = port_id;
    cmd->new_dev.state   = GET_SLOT_STATE(slot_ctx->dev_state);

    msg_info_t info = msginfo_word_new(0, sizeof(usb_core_cmd_t)/8, 0, 0);

    signal_send(usb_core_pid, SIGNAL_USER_BASE, ((uint64_t)usb_host_id << 8 | port_id));
    
 //   info = ipc_call(usb_core_cap, info);
    ipc_send(usb_core_cap, info); 
    // ret = (int)label_from_msginfo_word(info);
    // if (ret < 0 || !length_from_msginfo_word(info)) {
    //     debug("[XHCI] Error registring host %d info 0x%lX\n", ret, info);
    //     return ret;
    // }
    // ret = ipc_getMR(0);
    //     if (ret < 0) {
    //         debug("[XHCI] usb-core failed to register new device on slot %u\n", slot_id);
    //         return ret;
    //     }
    // debug("\x1b[31m[xhci]\x1b[0m Device registration with usb-core on host ID %d slot ID %d\n",
    //             usb_host_id, slot_id);

    debug("[XHCI] Registration request sent to usb-core for slot ID %d\n", slot_id);
    return ret;

}

void handle_new_device(uint64_t sender, uint64_t info)
{
    int ret;
    msg_info_t ret_info;

    usb_host_cmd_t *cmd = (usb_host_cmd_t *)get_ipc_buffer()->msg;

    uint8_t slot_id = cmd->new_dev.slot_id;
    uint8_t port_id = cmd->new_dev.port;
    
    ret = xhci_alloc_virt_device(xhci_dev, slot_id, port_id);
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
