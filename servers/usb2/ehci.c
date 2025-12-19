#include <common.h>
#include <riscv.h>
#include <libsys/memory.h>
#include <string.h>
#include <libsys/cap.h>
#include <signals.h>
#include <libsys/timer.h>

#include "ehci.h"
#include "k1x.h"
#include "phy.h"

#define EHCI_BASE   0xc0980000ULL
#define PHY_BASE    0xc09c0000ULL

usb2_dev_t  k1x_dev;
usb_phy_t   usbp1_phy;

signal_action_t irqhand;

static void mv_ehci_enable(usb2_dev_t *dev, bool enable)
{
	uint32_t temp;

	temp = getreg32((uint64_t)apmu_base + PMU_SD_ROT_WAKE_CLR);
	if (enable)
		putreg32(PMU_SD_ROT_WAKE_CLR_VBUS_DRV | temp, (uint64_t)apmu_base + PMU_SD_ROT_WAKE_CLR);
	else
		putreg32(temp & ~PMU_SD_ROT_WAKE_CLR_VBUS_DRV , (uint64_t)apmu_base + PMU_SD_ROT_WAKE_CLR);
}

/*
 * ehci_handshake - spin reading hc until handshake completes or fails
 * @ptr: address of hc register to be read
 * @mask: bits to look at in result of read
 * @done: value of those bits when handshake succeeds
 * @usec: timeout in microseconds
 *
 * Returns negative errno, or zero on success
 *
 * Success happens when the "mask" bits have the specified value (hardware
 * handshake done).  There are two failure modes:  "usec" have passed (major
 * hardware flakeout), or the register reads as all-ones (hardware removed).
 *
 * That last failure should_only happen in cases like physical cardbus eject
 * before driver shutdown. But it also seems to be caused by bugs in cardbus
 * bridge shutdown:  shutting down the bridge before the devices using it.
 */
int ehci_handshake(void *ptr,
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


/*
 Force HC to halt state from unknown (EHCI spec section 2.3).
 * Must be called with interrupts enabled and the lock not held.
 */
static int ehci_halt (usb2_dev_t *d)
{
	uint32_t temp;

//	spin_lock_irq(&ehci->lock);

	/* disable any irqs left enabled by previous code */
	putreg32(0, (uint64_t)&d->hcor->or_usbintr);

	// if (ehci_is_TDI(ehci) && !tdi_in_host_mode(ehci)) {
	// 	spin_unlock_irq(&ehci->lock);
	// 	return 0;
	// }

	/*
	 * This routine gets called during probe before ehci->command
	 * has been initialized, so we can't rely on its value.
	 */
	putreg32(getreg32((uint64_t)&d->hcor->or_usbcmd) & ~CMD_RUN, (uint64_t)&d->hcor->or_usbcmd);
	temp = getreg32((uint64_t)&d->hcor->or_usbcmd);
	temp &= ~(CMD_RUN | CMD_IAAD);
	putreg32(temp, (uint64_t)&d->hcor->or_usbcmd);

//	spin_unlock_irq(&ehci->lock);
//	synchronize_irq(ehci_to_hcd(ehci)->irq);

	return ehci_handshake(&d->hcor->or_usbsts,
			  STS_HALT, STS_HALT, 16 * 125);
}


static void ehci_resetHC(usb2_dev_t *d)
{
//    debug(" Reset");
    /*
    Intel Intel® 82801EB (ICH5), 82801ER (ICH5R), and 82801DB (ICH4)
    Enhanced Host Controller Interface (EHCI) Programmer’s Reference Manual (PRM) April 2003

    To initiate a host controller reset system software must:
    */

    // 1. Stop the host controller.
    //    System software must program the USB2CMD.Run/Stop bit to 0 to stop the host controller.
    d->hcor->or_usbcmd &= ~CMD_RUN;            // set Run-Stop-Bit to 0
debug("\x1b[31m[USB2]\x1b[0m 0x%X\n", d->hcor->or_usbcmd);
    // 2. Wait for the host controller to halt.
    //    To determine when the host controller has halted, system software must read the USB2STS.HCHalted bit;
    //    the host controller will set this bit to 1 as soon as
    //    it has successfully transitioned from a running state to a stopped state (halted).
    //    Attempting to reset an actively running host controller will result in undefined behavior.
    while (!(d->hcor->or_usbsts & STS_HALT))
    {
        udelay(2000); // wait at least 16 microframes (= 16*125 micro-sec = 2 ms)
    }
debug("\x1b[31m[USB2]\x1b[0m 0x%X\n", d->hcor->or_usbsts);
    // 3. Program the USB2CMD.HostControllerReset bit to a 1.
    //    This will cause the host controller to begin the host controller reset.
    d->hcor->or_usbcmd |= CMD_RESET;              // set Reset-Bit to 1

    // 4. Wait until the host controller has completed its reset.
    // To determine when the reset is complete, system software must read the USB2CMD.HostControllerReset bit;
    // the host controller will set this bit to 0 upon completion of the reset.
//    WAIT_FOR_CONDITION((e->OpRegs->USBCMD & CMD_HCRESET) == 0, 30, 10, "Timeout Error: HC Reset-Bit still set to 1\n");
    while (d->hcor->or_usbcmd & CMD_RESET)
    {
        udelay(10); 
    }
}

static int ehci_reset(usb2_dev_t *d)
{
	uint32_t cmd;
	int ret = 0;

	cmd = getreg32((uint64_t)&d->hcor->or_usbcmd);
	cmd = (cmd & ~CMD_RUN) | CMD_RESET;
	putreg32(cmd, (uint64_t)&d->hcor->or_usbcmd);
	ret = ehci_handshake((uint32_t *)&d->hcor->or_usbcmd,
			CMD_RESET, 0, 250 * 1000);
	if (ret < 0) {
		debug("\x1b[31m[USB2]\x1b[0m EHCI reset failed %d\n", ret);
		goto out;
	}

	// if (ehci_is_TDI())
	// 	ctrl->ops.set_usb_mode(ctrl);

// #ifdef CONFIG_USB_EHCI_TXFIFO_THRESH
// 	cmd = ehci_readl(&ctrl->hcor->or_txfilltuning);
// 	cmd &= ~TXFIFO_THRESH_MASK;
// 	cmd |= TXFIFO_THRESH(CONFIG_USB_EHCI_TXFIFO_THRESH);
// 	ehci_writel(&ctrl->hcor->or_txfilltuning, cmd);
// #endif
out:
	return ret;
}


// initialize Periodic list
static int ehci_init_periodic_list(usb2_dev_t *d)
{
       
    uint64_t pa;
    int dma_cap = cap_dmamem_create(0x1000, CRIGHT_MAP, &pa);
    if (dma_cap < 0) {
        debug("[USB2] Cannot allocate DMA mem block\n");
        return -ENOMEM;
    }

    debug("[USB2] allocat dma cap id 0%p at 0x%p\n", dma_cap, pa);

    uint32_t cmd = (getreg32((uint64_t)&d->hcor->or_usbcmd) & CMD_FRAMELIST_SIZE) | CMD_FRAMELIST_1024;
    putreg32(cmd, (uint64_t)&d->hcor->or_usbcmd);
    putreg32((uint32_t)((pa >> 32) & 0xFFFFFFFF), (uint64_t)&d->hcor->or_ctrldssegment);
    putreg32((uint32_t)(pa & 0xFFFFFFFF), (uint64_t)&d->hcor->or_periodiclistbase);
    return SUCCESS;
}

void irq_handler(uint32_t sig, uint64_t irq)
{
    debug("\x1b[31m[USB2]\x1b[0m irq handler sig %d irq %d\n", sig, irq);

}

int ehci_init()
{   
    memset(&k1x_dev, 0, sizeof(k1x_dev));
    memset(&usbp1_phy, 0, sizeof(usbp1_phy));

    k1x_dev.base = mmap(NULL, 0x4000, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)EHCI_BASE);
    if (!k1x_dev.base) return -EIO;

    k1x_dev.irq = 118;

    ccu_init();
    
    k1x_dev.clk = get_clk_by_id(CLK_USB_P1);
    if (!k1x_dev.clk) return -EINVAL;

    k1x_dev.rst = get_reset_by_id(RESET_USBP1_AXI);
    if (!k1x_dev.rst) return -EINVAL;    

    usbp1_phy.base = mmap(NULL, 0x1000, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)PHY_BASE);
    if (!usbp1_phy.base) return -EIO;

    usbp1_phy.clk = get_clk_by_id(CLK_USB_P1);
    if (!usbp1_phy.clk) return -EINVAL;


    k1x_dev.phy = &usbp1_phy;

    clock_enable(k1x_dev.clk);
    

    reset_deassert(k1x_dev.rst);

    usb_phy_init(k1x_dev.phy);


    k1x_dev.hccr = (struct ehci_hccr *)((uintptr_t)k1x_dev.base + 0x100);
    // debug("[USB2] HCCR 0x%lX HC lenght 0x%X HC version 0x%X\n", k1x_dev.hccr, 
    //         HC_LENGTH(getreg32((uint64_t)&k1x_dev.hccr->cr_capbase)),
    //         HC_VERSION(getreg32((uint64_t)&k1x_dev.hccr->cr_capbase)));
    k1x_dev.hcor = (struct ehci_hcor *)((uintptr_t)k1x_dev.hccr +
					HC_LENGTH(getreg32((uint64_t)&k1x_dev.hccr->cr_capbase)));

//    debug("[USB2] HCCR 0x%lX HCOR 0x%lX\n", k1x_dev.hccr, k1x_dev.hcor);

    mv_ehci_enable(&k1x_dev, true);

    
//------------------------
    // set irq handler
    irqhand.handler = irq_handler;
    signal_action(1, &irqhand);

    irq_set(118, 0);

    // int ret = ehci_halt (&k1x_dev);
    // if (ret < 0) debug("\x1b[31m[USB2]\x1b[0m ehci_halt ret %d\n", ret);
    // reset HC
//    ehci_resetHC(&k1x_dev);
    ehci_reset(&k1x_dev);
 
    ehci_init_periodic_list(&k1x_dev);

    // set events for irq
    putreg32(0x0000103F, (uint64_t)&k1x_dev.hcor->or_usbsts); // Note: Always insure that the USB2STS registers bits 5:0 are clear
    putreg32(INTR_UE | INTR_UEE | INTR_PCE | INTR_SEE | INTR_AAE, (uint64_t)&k1x_dev.hcor->or_usbintr); 

// 4. Program the USB2CMD.InterruptThresholdControl bits to set the desired interrupt threshold
//    k1x_dev.hcor->or_usbcmd |= CMD_8_MICROFRAME;


    //    and turn the host controller ON via setting the USB2CMD.Run/Stop bit. Setting the Run/Stop
    //    bit with both the periodic and asynchronous schedules disabled will still allow interrupts and
    //    enabled port events to be visible to software
    if (getreg32((uint64_t)&k1x_dev.hcor->or_usbsts) & STS_HALT)
    {
        putreg32(getreg32((uint64_t)&k1x_dev.hcor->or_usbcmd) | CMD_RUN, (uint64_t)&k1x_dev.hcor->or_usbcmd); // set Run-Stop-Bit
    }

    // 5. Write a 1 to CONFIGFLAG register to default-route all ports to the EHCI. The EHCI can temporarily release control
    //    of the port to a cHC by setting the PortOwner bit in the PORTSC register to a one
 //   putreg32(FLAG_CF, (uint64_t)&k1x_dev.hcor->or_configflag); // if zero, EHCI is not enabled and all usb devices go to the cHC
// debug("\x1b[31m[USB2]\x1b[0m CMD 0x%X\n", getreg32((uint64_t)&k1x_dev.hcor->or_usbcmd));

    debug("[USB2] USB STATUS 0x%X\n", getreg32((uint64_t)&k1x_dev.hcor->or_usbsts));

    debug("[USB2] USB port 0\n");
    ehci_checkPortLineStatus(&k1x_dev, 0);



/* Start the host controller. */
	uint32_t cmd = ehci_readl(&k1x_dev.hcor->or_usbcmd);
	/*
	 * Philips, Intel, and maybe others need CMD_RUN before the
	 * root hub will detect new devices (why?); NEC doesn't
	 */
	cmd &= ~(CMD_LRESET|CMD_IAAD|CMD_PSE|/*CMD_ASE|*/CMD_RESET);
	cmd |= CMD_RUN;
	ehci_writel(&k1x_dev.hcor->or_usbcmd, cmd);

	// if (!(tweaks & EHCI_TWEAK_NO_INIT_CF)) {
	// 	/* take control over the ports */
	// 	cmd = ehci_readl(&k1x_dev.hcor->or_configflag);
	// 	cmd |= FLAG_CF;
	// 	ehci_writel(&k1x_dev.hcor->or_configflag, cmd);
	// }

ehci_writel(&k1x_dev.hcor->or_configflag, FLAG_CF);

	/* unblock posted write */
	cmd = ehci_readl(&k1x_dev.hcor->or_usbcmd);
    debug("\x1b[31m[USB2]\x1b[0m CMD 0x%X\n", cmd);
	udelay(5000);
	uint32_t reg = HC_VERSION(ehci_readl(&k1x_dev.hccr->cr_capbase));
	debug("[USB2] USB EHCI %x.%02x\n", reg >> 8, reg & 0xff);






    return SUCCESS;

err:
    // reset_assert(k1x_dev.rst);
    // clock_disable(k1x_dev.clk);

    return -EIO;
}

// static void ehci_resetPort(usb2_dev_t *d, uint8_t j)
// {

//     // This field is zero if Port Power is zero.
// //    e->OpRegs->PORTSC[j] |= PSTS_POWERON;

//     /*
//      The HCHalted bit in the USBSTS register should be a zero before software attempts to use this bit.
//      The host controller may hold Port Reset asserted to a one when the HCHalted bit is a one.
//     */
//     // if (e->OpRegs->USBSTS & STS_HCHALTED) // TEST
//     // {
//     //     printfe("\nHCHalted set to 1 (Not OK!)");
//     //     ehci_showUSBSTS(e);
//     // }

//     /*
//      When software writes a one to this bit (from a zero), the bus reset sequence as defined in the USB Specification Revision 2.0 is started.
//      Software writes a zero to this bit to terminate the bus reset sequence.
//      Software must keep this bit at a one long enough to ensure the reset sequence, as specified in the USB Specification Revision 2.0, completes.
//      Note: when software writes this bit to a one, it must also write a zero to the Port Enable bit.
//     */
//     // start reset sequence
//     e->OpRegs->PORTSC[j] = (e->OpRegs->PORTSC[j] & ~PSTS_ENABLED) | PSTS_PORT_RESET;
//     sleepMilliSeconds(200);                   // do not delete this wait (freeBSD: 250 ms, spec: 50 ms) <== 200 ms at 250 Hz!!!
//     e->OpRegs->PORTSC[j] &= ~PSTS_PORT_RESET; // stop reset sequence

//     // wait and check, whether really zero
//     WAIT_FOR_CONDITION((e->OpRegs->PORTSC[j] & PSTS_PORT_RESET) == 0, 25, 5, "\nTimeour Error: Port %u did not reset! Port Status: %Xh", j+1, e->OpRegs->PORTSC[j]);
// }

// static void ehci_portCheck(usb2_dev_t *d)
// {

//     for (uint8_t j=0; j<e->hc.rootPortCount; j++)
//     {
//         if (e->OpRegs->PORTSC[j] & PSTS_CONNECTED_CHANGE)
//         {
//             e->OpRegs->PORTSC[j] |= PSTS_CONNECTED_CHANGE; // reset interrupt
//             if (e->OpRegs->PORTSC[j] & PSTS_CONNECTED)
//             {
//                 ehci_checkPortLineStatus(e, j);
//             }
//             else
//             {
//                 writeInfo(0, "Port: %u, no device attached", j+1);

//                 if (e->hc.ports.data[j]->device)
//                 {
//                     usb_destroyDevice(e->hc.ports.data[j]->device);
//                     e->hc.ports.data[j]->device = 0;
//                     e->hc.ports.data[j]->connected = false;
//                 }

//             }
//         }
//     }
//     textColor(IMPORTANT);
//     printf("\n>>> Press key to close this console. <<<");
//     textColor(TEXT);
//     getch();
// }

static void ehci_checkPortLineStatus(usb2_dev_t *d, uint8_t j)
{
    uint32_t portsts = ehci_readl(&d->hcor->or_portsc[j]);
    // if (!(getreg32((uint64_t)&d->hcor->or_portsc[j]) & 1))
    // {
    //     debug("[USB2] line state: -");
    //     return;
    // }

//    sleepMilliSeconds(100); // Wait 100ms until power is stable (USB 2.0, 9.1.2)

    uint8_t lineStatus = (getreg32((uint64_t)&d->hcor->or_portsc[j])>>10) & 3; // bits 11:10

    static const char* const state[] = {"SE0", "K-state", "J-state", "undefined"};
//   #ifdef _EHCI_DIAGNOSIS_
//     // field lineStatus is valid only when the port enable bit is zero and the current connect status bit is set to a one
//     printf("\nehci_checkPortLineStatus:  PSTS_ENABLED: %u  PSTS_CONNECTED: %u", e->OpRegs->PORTSC[j] & PSTS_ENABLED, e->OpRegs->PORTSC[j] & PSTS_CONNECTED);
//     textColor(LIGHT_CYAN);
//     printf("\nport %u: %xh, line: %yh (%s) ", j+1, e->OpRegs->PORTSC[j], lineStatus, state[lineStatus]);
//   #else
    debug("[USB2] line state: %s  0x%X\n", state[lineStatus], portsts);
//  #endif

    // switch (lineStatus)
    // {
    //     // case 1: // K-state, release ownership of port, because a low speed device is attached
    //     //     if(getreg32((uint64_t)&d->hccr->cr_hcsparams & 0xF000))
    //     //         e->OpRegs->PORTSC[j] |= PSTS_COMPANION_HC_OWNED; // release it to the cHC (if there is one)
    //     //     break;
    //     case 0: // SE0
    //     case 2: // J-state
    //     case 3: // undefined
    //         ehci_detectDevice(e, j); // cf. spec: reset necessary for SE0, J-state, undefined
    //         break;
    // }
}

// static void ehci_detectDevice(usb2_dev_t *d, uint8_t j)
// {
//     // ehci_resetPort(e,j);
//     // if (e->enabledPortFlag && (e->OpRegs->PORTSC[j] & PSTS_POWERON)) // power on
//     // {
//     //     // ehci only set PSTS_ENABLED when the reset sequence determines that the attached device is a high-speed device
//     //     if (e->OpRegs->PORTSC[j] & PSTS_ENABLED)
//     //     {
//     //         writeInfo(0, "Port: %u, hi-speed device attached", j+1);
//     //         hc_setupUSBDevice(&e->hc, j, USB_HIGHSPEED);
//     //     }
//     //     else // Full speed
//     //     {
//     //         if (e->CapRegs->HCSPARAMS & 0xF000)
//     //             e->OpRegs->PORTSC[j] |= PSTS_COMPANION_HC_OWNED; // release it to the cHC (if there is one)
//     //     }
//     // }
// }