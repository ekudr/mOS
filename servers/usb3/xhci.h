#ifndef __XHCI_H__
#define __XHCI_H__

#include <stdint.h>
#include <list.h>
#include <libsys/usb/usb.h>
#include <libsys/usb/usb-ipc.h>

#include "dma-pool.h"
#include "xhci-caps.h"
#include "xhci-port.h"


#define XHCI_PORTSC_BASE	0x400

/* HC should halt within 16 ms, but use 32 ms as some hosts take longer */
#define XHCI_MAX_HALT_USEC	(32 * 1000)

/* Max number of USB devices for any host controller - limit in section 6.1 */
#define MAX_HC_SLOTS		256
/* Section 5.3.3 - MaxPorts */
#define MAX_HC_PORTS		127

struct xhci_cap_regs {
	uint32_t	hc_capbase;
	uint32_t	hcs_params1;
	uint32_t	hcs_params2;
	uint32_t	hcs_params3;
	uint32_t	hcc_params;
	uint32_t	db_off;
	uint32_t	run_regs_off;
	uint32_t	hcc_params2;
};

struct xhci_port_regs {
	uint32_t	portsc;
	uint32_t	portpmsc;
	uint32_t	portli;
	uint32_t	porthlmpc;
};

/* Number of registers per port */
#define	NUM_PORT_REGS	4

#define PORTSC		0
#define PORTPMSC	1
#define PORTLI		2
#define PORTHLPMC	3


struct xhci_op_regs {
    uint32_t usbcmd;        // USB Command
    uint32_t usbsts;        // USB Status
    uint32_t pagesize;      // Page Size
    uint32_t reserved0[2];
    uint32_t dnctrl;        // Device Notification Control
    uint64_t crcr;          // Command Ring Control
    uint32_t reserved1[4];
    uint64_t dcbaap;        // Device Context Base Address Array Pointer
    uint32_t config;        // Configure
	/* rsvd: offset 0x3C-3FF */
	uint32_t	reserved4[241];
	/* port 1 registers, which serve as a base address for other ports */
	uint32_t	port_status_base;
	uint32_t	port_power_base;
	uint32_t	port_link_base;
	uint32_t	reserved5;
	/* registers for ports 2-255 */
	uint32_t	reserved6[NUM_PORT_REGS*254];
};


/* Number of registers per port */
#define	NUM_PORT_REGS	4

#define PORTSC		0
#define PORTPMSC	1
#define PORTLI		2
#define PORTHLPMC	3

/* USBCMD - USB command - command bitmasks */
/* start/stop HC execution - do not write unless HC is halted*/
#define CMD_RUN		(1 << 0)
/* Reset HC - resets internal HC state machine and all registers (except
 * PCI config regs).  HC does NOT drive a USB reset on the downstream ports.
 * The xHCI driver must reinitialize the xHC after setting this bit.
 */
#define CMD_RESET	(1 << 1)
/* Event Interrupt Enable - a '1' allows interrupts from the host controller */
#define CMD_EIE		(1 << 2)
/* Host System Error Interrupt Enable - get out-of-band signal for HC errors */
#define CMD_HSEIE	(1 << 3)
/* bits 4:6 are reserved (and should be preserved on writes). */
/* light reset (port status stays unchanged) - reset completed when this is 0 */
#define CMD_LRESET	(1 << 7)
/* host controller save/restore state. */
#define CMD_CSS		(1 << 8)
#define CMD_CRS		(1 << 9)
/* Enable Wrap Event - '1' means xHC generates an event when MFINDEX wraps. */
#define CMD_EWE		(1 << 10)
/* MFINDEX power management - '1' means xHC can stop MFINDEX counter if all root
 * hubs are in U3 (selective suspend), disconnect, disabled, or powered-off.
 * '0' means the xHC can power it off if all ports are in the disconnect,
 * disabled, or powered-off state.
 */
#define CMD_PM_INDEX	(1 << 11)
/* bit 14 Extended TBC Enable, changes Isoc TRB fields to support larger TBC */
#define CMD_ETE		(1 << 14)
/* bits 15:31 are reserved (and should be preserved on writes). */

#define XHCI_RESET_LONG_USEC		(10 * 1000 * 1000)
#define XHCI_RESET_SHORT_USEC		(250 * 1000)

/* IMAN - Interrupt Management Register */
#define IMAN_IE		(1 << 1)
#define IMAN_IP		(1 << 0)

/* USBSTS - USB status - status bitmasks */
/* HC not running - set to 1 when run/stop bit is cleared. */
#define STS_HALT	(1<<0)
/* serious error, e.g. PCI parity error.  The HC will clear the run/stop bit. */
#define STS_FATAL	(1 << 2)
/* event interrupt - clear this prior to clearing any IP flags in IR set*/
#define STS_EINT	(1 << 3)
/* port change detect */
#define STS_PORT	(1 << 4)
/* bits 5:7 reserved and zeroed */
/* save state status - '1' means xHC is saving state */
#define STS_SAVE	(1 << 8)
/* restore state status - '1' means xHC is restoring state */
#define STS_RESTORE	(1 << 9)
/* true: save or restore error */
#define STS_SRE		(1 << 10)
/* true: Controller Not Ready to accept doorbell or op reg writes after reset */
#define STS_CNR		(1 << 11)
/* true: internal Host Controller Error - SW needs to reset and reinitialize */
#define STS_HCE		(1 << 12)

/*
 * DNCTRL - Device Notification Control Register - dev_notification bitmasks
 * Generate a device notification event when the HC sees a transaction with a
 * notification type that matches a bit set in this bit field.
 */
#define	DEV_NOTE_MASK		(0xffff)
#define ENABLE_DEV_NOTE(x)	(1 << (x))
/* Most of the device notification types should only be used for debug.
 * SW does need to pay attention to function wake notifications.
 */
#define	DEV_NOTE_FWAKE		ENABLE_DEV_NOTE(1)

/* CRCR - Command Ring Control Register - cmd_ring bitmasks */
/* bit 0 is the command ring cycle state */
/* stop ring operation after completion of the currently executing command */
#define CMD_RING_PAUSE		(1 << 1)
/* stop ring immediately - abort the currently executing command */
#define CMD_RING_ABORT		(1 << 2)
/* true: command ring is running */
#define CMD_RING_RUNNING	(1 << 3)
/* bits 4:5 reserved and should be preserved */
/* Command Ring pointer - bit mask for the lower 32 bits. */
#define CMD_RING_RSVD_BITS	(0x3f)

/* CONFIG - Configure Register - config_reg bitmasks */
/* bits 0:7 - maximum number of device slots enabled (NumSlotsEn) */
#define MAX_DEVS(p)	((p) & 0xff)
/* bit 8: U3 Entry Enabled, assert PLC when root port enters U3, xhci 1.1 */
#define CONFIG_U3E		(1 << 8)
/* bit 9: Configuration Information Enable, xhci 1.1 */
#define CONFIG_CIE		(1 << 9)
/* bits 10:31 - reserved and should be preserved */

/* PORTSC - Port Status and Control Register - port_status_base bitmasks */
/* true: device connected */
#define PORT_CONNECT	(1 << 0)
/* true: port enabled */
#define PORT_PE		(1 << 1)
/* bit 2 reserved and zeroed */
/* true: port has an over-current condition */
#define PORT_OC		(1 << 3)
/* true: port reset signaling asserted */
#define PORT_RESET	(1 << 4)
/* Port Link State - bits 5:8
 * A read gives the current link PM state of the port,
 * a write with Link State Write Strobe set sets the link state.
 */
#define PORT_PLS_MASK	(0xf << 5)
#define XDEV_U0		(0x0 << 5)
#define XDEV_U1		(0x1 << 5)
#define XDEV_U2		(0x2 << 5)
#define XDEV_U3		(0x3 << 5)
#define XDEV_DISABLED	(0x4 << 5)
#define XDEV_RXDETECT	(0x5 << 5)
#define XDEV_INACTIVE	(0x6 << 5)
#define XDEV_POLLING	(0x7 << 5)
#define XDEV_RECOVERY	(0x8 << 5)
#define XDEV_HOT_RESET	(0x9 << 5)
#define XDEV_COMP_MODE	(0xa << 5)
#define XDEV_TEST_MODE	(0xb << 5)
#define XDEV_RESUME	(0xf << 5)

/* true: port has power (see HCC_PPC) */
#define PORT_POWER	(1 << 9)
/* bits 10:13 indicate device speed:
 * 0 - undefined speed - port hasn't be initialized by a reset yet
 * 1 - full speed
 * 2 - low speed
 * 3 - high speed
 * 4 - super speed
 * 5-15 reserved
 */
#define DEV_SPEED_MASK		(0xf << 10)
#define	XDEV_FS			(0x1 << 10)
#define	XDEV_LS			(0x2 << 10)
#define	XDEV_HS			(0x3 << 10)
#define	XDEV_SS			(0x4 << 10)
#define	XDEV_SSP		(0x5 << 10)
#define DEV_UNDEFSPEED(p)	(((p) & DEV_SPEED_MASK) == (0x0<<10))
#define DEV_FULLSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_FS)
#define DEV_LOWSPEED(p)		(((p) & DEV_SPEED_MASK) == XDEV_LS)
#define DEV_HIGHSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_HS)
#define DEV_SUPERSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_SS)
#define DEV_SUPERSPEEDPLUS(p)	(((p) & DEV_SPEED_MASK) == XDEV_SSP)
#define DEV_SUPERSPEED_ANY(p)	(((p) & DEV_SPEED_MASK) >= XDEV_SS)
#define DEV_PORT_SPEED(p)	(((p) >> 10) & 0x0f)

/* Bits 20:23 in the Slot Context are the speed for the device */
#define	SLOT_SPEED_FS		(XDEV_FS << 10)
#define	SLOT_SPEED_LS		(XDEV_LS << 10)
#define	SLOT_SPEED_HS		(XDEV_HS << 10)
#define	SLOT_SPEED_SS		(XDEV_SS << 10)
#define	SLOT_SPEED_SSP		(XDEV_SSP << 10)
/* Port Indicator Control */
#define PORT_LED_OFF	(0 << 14)
#define PORT_LED_AMBER	(1 << 14)
#define PORT_LED_GREEN	(2 << 14)
#define PORT_LED_MASK	(3 << 14)
/* Port Link State Write Strobe - set this when changing link state */
#define PORT_LINK_STROBE	(1 << 16)
/* true: connect status change */
#define PORT_CSC	(1 << 17)
/* true: port enable change */
#define PORT_PEC	(1 << 18)
/* true: warm reset for a USB 3.0 device is done.  A "hot" reset puts the port
 * into an enabled state, and the device into the default state.  A "warm" reset
 * also resets the link, forcing the device through the link training sequence.
 * SW can also look at the Port Reset register to see when warm reset is done.
 */
#define PORT_WRC	(1 << 19)
/* true: over-current change */
#define PORT_OCC	(1 << 20)
/* true: reset change - 1 to 0 transition of PORT_RESET */
#define PORT_RC		(1 << 21)
/* port link status change - set on some port link state transitions:
 *  Transition				Reason
 *  ------------------------------------------------------------------------------
 *  - U3 to Resume			Wakeup signaling from a device
 *  - Resume to Recovery to U0		USB 3.0 device resume
 *  - Resume to U0			USB 2.0 device resume
 *  - U3 to Recovery to U0		Software resume of USB 3.0 device complete
 *  - U3 to U0				Software resume of USB 2.0 device complete
 *  - U2 to U0				L1 resume of USB 2.1 device complete
 *  - U0 to U0 (???)			L1 entry rejection by USB 2.1 device
 *  - U0 to disabled			L1 entry error with USB 2.1 device
 *  - Any state to inactive		Error on USB 3.0 port
 */
#define PORT_PLC	(1 << 22)
/* port configure error change - port failed to configure its link partner */
#define PORT_CEC	(1 << 23)
#define PORT_CHANGE_MASK	(PORT_CSC | PORT_PEC | PORT_WRC | PORT_OCC | \
				 PORT_RC | PORT_PLC | PORT_CEC)


/* Cold Attach Status - xHC can set this bit to report device attached during
 * Sx state. Warm port reset should be perfomed to clear this bit and move port
 * to connected state.
 */
#define PORT_CAS	(1 << 24)
/* wake on connect (enable) */
#define PORT_WKCONN_E	(1 << 25)
/* wake on disconnect (enable) */
#define PORT_WKDISC_E	(1 << 26)
/* wake on over-current (enable) */
#define PORT_WKOC_E	(1 << 27)
/* bits 28:29 reserved */
/* true: device is non-removable - for USB 3.0 roothub emulation */
#define PORT_DEV_REMOVE	(1 << 30)
/* Initiate a warm port reset - complete when PORT_WRC is '1' */
#define PORT_WR		(1 << 31)

/* We mark duplicate entries with -1 */
#define DUPLICATE_ENTRY ((u8)(-1))

/* Port Power Management Status and Control - port_power_base bitmasks */
/* Inactivity timer value for transitions into U1, in microseconds.
 * Timeout can be up to 127us.  0xFF means an infinite timeout.
 */
#define PORT_U1_TIMEOUT(p)	((p) & 0xff)
#define PORT_U1_TIMEOUT_MASK	0xff
/* Inactivity timer value for transitions into U2 */
#define PORT_U2_TIMEOUT(p)	(((p) & 0xff) << 8)
#define PORT_U2_TIMEOUT_MASK	(0xff << 8)
/* Bits 24:31 for port testing */

/* USB2 Protocol PORTSPMSC */
#define	PORT_L1S_MASK		7
#define	PORT_L1S_SUCCESS	1
#define	PORT_RWE		(1 << 3)
#define	PORT_HIRD(p)		(((p) & 0xf) << 4)
#define	PORT_HIRD_MASK		(0xf << 4)
#define	PORT_L1DS_MASK		(0xff << 8)
#define	PORT_L1DS(p)		(((p) & 0xff) << 8)
#define	PORT_HLE		(1 << 16)
#define PORT_TEST_MODE_SHIFT	28

/* USB3 Protocol PORTLI  Port Link Information */
#define PORT_RX_LANES(p)	(((p) >> 16) & 0xf)
#define PORT_TX_LANES(p)	(((p) >> 20) & 0xf)

/* USB2 Protocol PORTHLPMC */
#define PORT_HIRDM(p)((p) & 3)
#define PORT_L1_TIMEOUT(p)(((p) & 0xff) << 2)
#define PORT_BESLD(p)(((p) & 0xf) << 10)

/* use 512 microseconds as USB2 LPM L1 default timeout. */
#define XHCI_L1_TIMEOUT		512

/* Set default HIRD/BESL value to 4 (350/400us) for USB2 L1 LPM resume latency.
 * Safe to use with mixed HIRD and BESL systems (host and device) and is used
 * by other operating systems.
 *
 * XHCI 1.0 errata 8/14/12 Table 13 notes:
 * "Software should choose xHC BESL/BESLD field values that do not violate a
 * device's resume latency requirements,
 * e.g. not program values > '4' if BLC = '1' and a HIRD device is attached,
 * or not program values < '4' if BLC = '0' and a BESL device is attached.
 */
#define XHCI_DEFAULT_BESL	4

/*
 * USB3 specification define a 360ms tPollingLFPSTiemout for USB3 ports
 * to complete link training. usually link trainig completes much faster
 * so check status 10 times with 36ms sleep in places we need to wait for
 * polling to complete.
 */
#define XHCI_PORT_POLLING_LFPS_TIME  36

/**
 * struct doorbell_array
 *
 * Bits  0 -  7: Endpoint target
 * Bits  8 - 15: RsvdZ
 * Bits 16 - 31: Stream ID
 *
 * Section 5.6
 */
struct xhci_doorbell_array {
	uint32_t	doorbell[256];
};

#define DB_VALUE(ep, stream)	((((ep) + 1) & 0xff) | ((stream) << 16))
#define DB_VALUE_HOST		0x00000000

#define PLT_MASK        (0x03 << 6)
#define PLT_SYM         (0x00 << 6)
#define PLT_ASYM_RX     (0x02 << 6)
#define PLT_ASYM_TX     (0x03 << 6)



typedef struct xhci_intr_reg {
	uint32_t	iman;
	uint32_t	imod;
	uint32_t	erst_size;
	uint32_t	rsvd;
	uint64_t	erst_base;
	uint64_t	erst_dequeue;
} xhci_intr_reg_t;

/* iman bitmasks */
/* bit 0 - Interrupt Pending (IP), whether there is an interrupt pending. Write-1-to-clear. */
#define	IMAN_IP			(1 << 0)
/* bit 1 - Interrupt Enable (IE), whether the interrupter is capable of generating an interrupt */
#define	IMAN_IE			(1 << 1)

/* imod bitmasks */
/*
 * bits 15:0 - Interrupt Moderation Interval, the minimum interval between interrupts
 * (in 250ns intervals). The interval between interrupts will be longer if there are no
 * events on the event ring. Default is 4000 (1 ms).
 */
#define IMODI_MASK		(0xffff)
/* bits 31:16 - Interrupt Moderation Counter, used to count down the time to the next interrupt */
#define IMODC_MASK		(0xffff << 16)

/* erst_size bitmasks */
/* bits 15:0 - Event Ring Segment Table Size, number of ERST entries */
#define	ERST_SIZE_MASK		(0xffff)

/* erst_base bitmasks */
/* bits 63:6 - Event Ring Segment Table Base Address Register */
#define ERST_BASE_ADDRESS_MASK	GENMASK_ULL(63, 6)

/* erst_dequeue bitmasks */
/*
 * bits 2:0 - Dequeue ERST Segment Index (DESI), is the segment number (or alias) where the
 * current dequeue pointer lies. This is an optional HW hint.
 */
#define ERST_DESI_MASK		(0x7)
/*
 * bit 3 - Event Handler Busy (EHB), whether the event ring is scheduled to be serviced by
 * a work queue (or delayed service routine)?
 */
#define ERST_EHB		(1 << 3)
/* bits 63:4 - Event Ring Dequeue Pointer */
#define ERST_PTR_MASK		GENMASK_ULL(63, 4)

typedef struct xhci_run_regs {
	uint32_t		    	microframe_index;
	uint32_t    			rsvd[7];
	struct xhci_intr_reg	ir_set[1024];
} xhci_run_regs_t;

typedef struct xhci_trb
{
    uint64_t    parameter;
    uint32_t    status;
    union 
    {
        struct 
        {
            uint32_t cycle_bit               : 1;
            uint32_t eval_next_trb           : 1;
            uint32_t interrupt_on_short_pkt  : 1;
            uint32_t no_snoop                : 1;
            uint32_t chain_bit               : 1;
            uint32_t interrupt_on_completion : 1;
            uint32_t immediate_data          : 1;
            uint32_t rsvd0                   : 2;
            uint32_t block_event_interrupt   : 1;
            uint32_t trb_type                : 6;
            uint32_t rsvd1                   : 16;
        };
        uint32_t control;
    };
    
} xhci_trb_t;

/* flags bitmasks */

/* Address device - disable SetAddress */
#define TRB_BSR		(1<<9)

/* Configure Endpoint - Deconfigure */
#define TRB_DC		(1<<9)

/* Stop Ring - Transfer State Preserve */
#define TRB_TSP		(1<<9)

enum xhci_ep_reset_type {
	EP_HARD_RESET,
	EP_SOFT_RESET,
};

#define TRB_TO_SLOT_ID(p)	(((p) & (0xff<<24)) >> 24)
#define SLOT_ID_FOR_TRB(p)	(((p) & 0xff) << 24)

/* TRB bit mask */
#define	TRB_TYPE_BITMASK	(0xfc00)
#define TRB_TYPE(p)		((p) << 10)
#define TRB_FIELD_TO_TYPE(p)	(((p) & TRB_TYPE_BITMASK) >> 10)
/* TRB type IDs */
/* bulk, interrupt, isoc scatter/gather, and control data stage */
#define TRB_NORMAL		1
/* setup stage for control transfers */
#define TRB_SETUP		2
/* data stage for control transfers */
#define TRB_DATA		3
/* status stage for control transfers */
#define TRB_STATUS		4
/* isoc transfers */
#define TRB_ISOC		5
/* TRB for linking ring segments */
#define TRB_LINK		6
#define TRB_EVENT_DATA		7
/* Transfer Ring No-op (not for the command ring) */
#define TRB_TR_NOOP		8
/* Command TRBs */
/* Enable Slot Command */
#define TRB_ENABLE_SLOT		9
/* Disable Slot Command */
#define TRB_DISABLE_SLOT	10
/* Address Device Command */
#define TRB_ADDR_DEV		11
/* Configure Endpoint Command */
#define TRB_CONFIG_EP		12
/* Evaluate Context Command */
#define TRB_EVAL_CONTEXT	13
/* Reset Endpoint Command */
#define TRB_RESET_EP		14
/* Stop Transfer Ring Command */
#define TRB_STOP_RING		15
/* Set Transfer Ring Dequeue Pointer Command */
#define TRB_SET_DEQ		16
/* Reset Device Command */
#define TRB_RESET_DEV		17
/* Force Event Command (opt) */
#define TRB_FORCE_EVENT		18
/* Negotiate Bandwidth Command (opt) */
#define TRB_NEG_BANDWIDTH	19
/* Set Latency Tolerance Value Command (opt) */
#define TRB_SET_LT		20
/* Get port bandwidth Command */
#define TRB_GET_BW		21
/* Force Header Command - generate a transaction or link management packet */
#define TRB_FORCE_HEADER	22
/* No-op Command - not for transfer rings */
#define TRB_CMD_NOOP		23
/* TRB IDs 24-31 reserved */
/* Event TRBS */
/* Transfer Event */
#define TRB_TRANSFER		32
/* Command Completion Event */
#define TRB_COMPLETION		33
/* Port Status Change Event */
#define TRB_PORT_STATUS		34
/* Bandwidth Request Event (opt) */
#define TRB_BANDWIDTH_EVENT	35
/* Doorbell Event (opt) */
#define TRB_DOORBELL		36
/* Host Controller Event */
#define TRB_HC_EVENT		37
/* Device Notification Event - device sent function wake notification */
#define TRB_DEV_NOTE		38
/* MFINDEX Wrap Event - microframe counter wrapped */
#define TRB_MFINDEX_WRAP	39
/* TRB IDs 40-47 reserved, 48-63 is vendor-defined */
#define TRB_VENDOR_DEFINED_LOW	48
/* Nec vendor-specific command completion event. */
#define	TRB_NEC_CMD_COMP	48
/* Get NEC firmware revision. */
#define	TRB_NEC_GET_FW		49

#define TRB_TYPE_LINK(x)	(((x) & TRB_TYPE_BITMASK) == TRB_TYPE(TRB_LINK))

/* Link TRB specific fields */
#define TRB_TC			(1<<1)

/* Normal TRB fields */
/* transfer_len bitmasks - bits 0:16 */
#define	TRB_LEN(p)		((p) & 0x1ffff)
/* TD Size, packets remaining in this TD, bits 21:17 (5 bits, so max 31) */
#define TRB_TD_SIZE(p)          (min((p), (u32)31) << 17)
#define GET_TD_SIZE(p)		(((p) & 0x3e0000) >> 17)
/* xhci 1.1 uses the TD_SIZE field for TBC if Extended TBC is enabled (ETE) */
#define TRB_TD_SIZE_TBC(p)      (min((p), (u32)31) << 17)
/* Interrupter Target - which MSI-X vector to target the completion event at */
#define TRB_INTR_TARGET(p)	(((p) & 0x3ff) << 22)
#define GET_INTR_TARGET(p)	(((p) >> 22) & 0x3ff)
/* Total burst count field, Rsvdz on xhci 1.1 with Extended TBC enabled (ETE) */
#define TRB_TBC(p)		(((p) & 0x3) << 7)
#define TRB_TLBPC(p)		(((p) & 0xf) << 16)

/* Cycle bit - indicates TRB ownership by HC or HCD */
#define TRB_CYCLE		(1<<0)
/*
 * Force next event data TRB to be evaluated before task switch.
 * Used to pass OS data back after a TD completes.
 */
#define TRB_ENT			(1<<1)
/* Interrupt on short packet */
#define TRB_ISP			(1<<2)
/* Set PCIe no snoop attribute */
#define TRB_NO_SNOOP		(1<<3)
/* Chain multiple TRBs into a TD */
#define TRB_CHAIN		(1<<4)
/* Interrupt on completion */
#define TRB_IOC			(1<<5)
/* The buffer pointer contains immediate data */
#define TRB_IDT			(1<<6)
/* TDs smaller than this might use IDT */
#define TRB_IDT_MAX_SIZE	8

/* Block Event Interrupt */
#define	TRB_BEI			(1<<9)

/* Control transfer TRB specific fields */
#define TRB_DIR_IN		(1<<16)
#define	TRB_TX_TYPE(p)		((p) << 16)
#define	TRB_DATA_OUT		2
#define	TRB_DATA_IN		3

/* Isochronous TRB specific fields */
#define TRB_SIA			(1<<31)
#define TRB_FRAME_ID(p)		(((p) & 0x7ff) << 20)

/* TRB cache size for xHC with TRB cache */
#define TRB_CACHE_SIZE_HS	8
#define TRB_CACHE_SIZE_SS	16


/* Command completion event TRB */
typedef struct xhci_event_cmd {
	/* Pointer to command TRB, or the value passed by the event data trb */
	uint64_t cmd_trb;
    union
    {
        struct {
            uint32_t rsvd0           : 24;
            uint32_t completion_code : 8;
        };
    	uint32_t status;        
    };
    union {
        struct {
            uint32_t cycle_bit   : 1;
            uint32_t rsvd1       : 9;
            uint32_t trb_type    : 6;
            uint32_t vfid        : 8;
            uint32_t slot_id     : 8;
        };
    	uint32_t flags;        
    };
} xhci_event_cmd_t;

typedef struct xhci_transfer_event {
	/* 64-bit buffer address, or immediate data */
	uint64_t	buffer;
    union {
        struct 
        {
            uint32_t transfer_length : 24;
            uint32_t complition_code : 8;          
        };
        uint32_t status; 
    };

	/* This field is interpreted differently based on the type of TRB */
    union {
        struct {
            uint32_t cycle_bit   : 1;
            uint32_t rsvd1       : 1;            
            uint32_t event_data  : 1;
            uint32_t rsvd2       : 7;
            uint32_t trb_type    : 6;
            uint32_t ep_id       : 5;
            uint32_t rsvd3       : 3;
            uint32_t slot_id     : 8;
        };
    	uint32_t flags;        
    };
} xhci_transfer_event_t;

typedef struct xhci_completion_event
{
    list_head_t         event_list;
    xhci_event_cmd_t    event;

} xhci_completion_event_t;

/* Completion Code - only applicable for some types of TRBs */
#define	COMP_CODE_MASK		(0xff << 24)
#define GET_COMP_CODE(p)	(((p) & COMP_CODE_MASK) >> 24)
#define COMP_INVALID				0
#define COMP_SUCCESS				1
#define COMP_DATA_BUFFER_ERROR			2
#define COMP_BABBLE_DETECTED_ERROR		3
#define COMP_USB_TRANSACTION_ERROR		4
#define COMP_TRB_ERROR				5
#define COMP_STALL_ERROR			6
#define COMP_RESOURCE_ERROR			7
#define COMP_BANDWIDTH_ERROR			8
#define COMP_NO_SLOTS_AVAILABLE_ERROR		9
#define COMP_INVALID_STREAM_TYPE_ERROR		10
#define COMP_SLOT_NOT_ENABLED_ERROR		11
#define COMP_ENDPOINT_NOT_ENABLED_ERROR		12
#define COMP_SHORT_PACKET			13
#define COMP_RING_UNDERRUN			14
#define COMP_RING_OVERRUN			15
#define COMP_VF_EVENT_RING_FULL_ERROR		16
#define COMP_PARAMETER_ERROR			17
#define COMP_BANDWIDTH_OVERRUN_ERROR		18
#define COMP_CONTEXT_STATE_ERROR		19
#define COMP_NO_PING_RESPONSE_ERROR		20
#define COMP_EVENT_RING_FULL_ERROR		21
#define COMP_INCOMPATIBLE_DEVICE_ERROR		22
#define COMP_MISSED_SERVICE_ERROR		23
#define COMP_COMMAND_RING_STOPPED		24
#define COMP_COMMAND_ABORTED			25
#define COMP_STOPPED				26
#define COMP_STOPPED_LENGTH_INVALID		27
#define COMP_STOPPED_SHORT_PACKET		28
#define COMP_MAX_EXIT_LATENCY_TOO_LARGE_ERROR	29
#define COMP_ISOCH_BUFFER_OVERRUN		31
#define COMP_EVENT_LOST_ERROR			32
#define COMP_UNDEFINED_ERROR			33
#define COMP_INVALID_STREAM_ID_ERROR		34
#define COMP_SECONDARY_BANDWIDTH_ERROR		35
#define COMP_SPLIT_TRANSACTION_ERROR		36

/* Port Status Change Event TRB fields */
/* Port ID - bits 31:24 */
#define GET_PORT_ID(p)		(((p) & (0xff << 24)) >> 24)

/**
 * struct xhci_slot_ctx
 * @dev_info:	Route string, device speed, hub info, and last valid endpoint
 * @dev_info2:	Max exit latency for device number, root hub port number
 * @tt_info:	tt_info is used to construct split transaction tokens
 * @dev_state:	slot state and device address
 *
 * Slot Context - section 6.2.1.1.  This assumes the HC uses 32-byte context
 * structures.  If the HC uses 64-byte contexts, there is an additional 32 bytes
 * reserved at the end of the slot context for HC internal use.
 */
typedef struct xhci_slot_ctx {
	uint32_t	dev_info;
	uint32_t	dev_info2;
	uint32_t	tt_info;
	uint32_t	dev_state;
	/* offset 0x10 to 0x1f reserved for HC internal use */
	uint32_t	reserved[4];
} xhci_slot_ctx_t;

/* dev_info bitmasks */
/* Route String - 0:19 */
#define ROUTE_STRING_MASK	(0xfffff)
/* Device speed - values defined by PORTSC Device Speed field - 20:23 */
#define DEV_SPEED	(0xf << 20)
#define GET_DEV_SPEED(n) (((n) & DEV_SPEED) >> 20)
/* bit 24 reserved */
/* Is this LS/FS device connected through a HS hub? - bit 25 */
#define DEV_MTT		(0x1 << 25)
/* Set if the device is a hub - bit 26 */
#define DEV_HUB		(0x1 << 26)
/* Index of the last valid endpoint context in this device context - 27:31 */
#define LAST_CTX_MASK	(0x1f << 27)
#define LAST_CTX(p)	((p) << 27)
#define LAST_CTX_TO_EP_NUM(p)	(((p) >> 27) - 1)
#define SLOT_FLAG	(1 << 0)
#define EP0_FLAG	(1 << 1)

/* dev_info2 bitmasks */
/* Max Exit Latency (ms) - worst case time to wake up all links in dev path */
#define MAX_EXIT	(0xffff)
/* Root hub port number that is needed to access the USB device */
#define ROOT_HUB_PORT(p)	(((p) & 0xff) << 16)
#define DEVINFO_TO_ROOT_HUB_PORT(p)	(((p) >> 16) & 0xff)
/* Maximum number of ports under a hub device */
#define XHCI_MAX_PORTS(p)	(((p) & 0xff) << 24)
#define DEVINFO_TO_MAX_PORTS(p)	(((p) & (0xff << 24)) >> 24)

/* tt_info bitmasks */
/*
 * TT Hub Slot ID - for low or full speed devices attached to a high-speed hub
 * The Slot ID of the hub that isolates the high speed signaling from
 * this low or full-speed device.  '0' if attached to root hub port.
 */
#define TT_SLOT		(0xff)
/*
 * The number of the downstream facing port of the high-speed hub
 * '0' if the device is not low or full speed.
 */
#define TT_PORT		(0xff << 8)
#define TT_THINK_TIME(p)	(((p) & 0x3) << 16)
#define GET_TT_THINK_TIME(p)	(((p) & (0x3 << 16)) >> 16)

/* dev_state bitmasks */
/* USB device address - assigned by the HC */
#define DEV_ADDR_MASK	(0xff)
/* bits 8:26 reserved */
/* Slot state */
#define SLOT_STATE	(0x1f << 27)
#define GET_SLOT_STATE(p)	(((p) & (0x1f << 27)) >> 27)

#define SLOT_STATE_DISABLED	0
#define SLOT_STATE_ENABLED	SLOT_STATE_DISABLED
#define SLOT_STATE_DEFAULT	1
#define SLOT_STATE_ADDRESSED	2
#define SLOT_STATE_CONFIGURED	3

/**
 * struct xhci_ep_ctx
 * @ep_info:	endpoint state, streams, mult, and interval information.
 * @ep_info2:	information on endpoint type, max packet size, max burst size,
 * 		error count, and whether the HC will force an event for all
 * 		transactions.
 * @deq:	64-bit ring dequeue pointer address.  If the endpoint only
 * 		defines one stream, this points to the endpoint transfer ring.
 * 		Otherwise, it points to a stream context array, which has a
 * 		ring pointer for each flow.
 * @tx_info:
 * 		Average TRB lengths for the endpoint ring and
 * 		max payload within an Endpoint Service Interval Time (ESIT).
 *
 * Endpoint Context - section 6.2.1.2.  This assumes the HC uses 32-byte context
 * structures.  If the HC uses 64-byte contexts, there is an additional 32 bytes
 * reserved at the end of the endpoint context for HC internal use.
 */
typedef struct xhci_ep_ctx {
	uint32_t	ep_info;
	uint32_t	ep_info2;
	uint64_t	deq;
	uint32_t	tx_info;
	/* offset 0x14 - 0x1f reserved for HC internal use */
	uint32_t	reserved[3];
} xhci_ep_ctx_t;

/* ep_info bitmasks */
/*
 * Endpoint State - bits 0:2
 * 0 - disabled
 * 1 - running
 * 2 - halted due to halt condition - ok to manipulate endpoint ring
 * 3 - stopped
 * 4 - TRB error
 * 5-7 - reserved
 */
#define EP_STATE_MASK		(0x7)
#define EP_STATE_DISABLED	0
#define EP_STATE_RUNNING	1
#define EP_STATE_HALTED		2
#define EP_STATE_STOPPED	3
#define EP_STATE_ERROR		4
#define GET_EP_CTX_STATE(ctx)	(le32_to_cpu((ctx)->ep_info) & EP_STATE_MASK)

/* Mult - Max number of burtst within an interval, in EP companion desc. */
#define EP_MULT(p)		(((p) & 0x3) << 8)
#define CTX_TO_EP_MULT(p)	(((p) >> 8) & 0x3)
/* bits 10:14 are Max Primary Streams */
/* bit 15 is Linear Stream Array */
/* Interval - period between requests to an endpoint - 125u increments. */
#define EP_INTERVAL(p)			(((p) & 0xff) << 16)
#define EP_INTERVAL_TO_UFRAMES(p)	(1 << (((p) >> 16) & 0xff))
#define CTX_TO_EP_INTERVAL(p)		(((p) >> 16) & 0xff)
#define EP_MAXPSTREAMS_MASK		(0x1f << 10)
#define EP_MAXPSTREAMS(p)		(((p) << 10) & EP_MAXPSTREAMS_MASK)
#define CTX_TO_EP_MAXPSTREAMS(p)	(((p) & EP_MAXPSTREAMS_MASK) >> 10)
/* Endpoint is set up with a Linear Stream Array (vs. Secondary Stream Array) */
#define	EP_HAS_LSA		(1 << 15)
/* hosts with LEC=1 use bits 31:24 as ESIT high bits. */
#define CTX_TO_MAX_ESIT_PAYLOAD_HI(p)	(((p) >> 24) & 0xff)

/* ep_info2 bitmasks */
/*
 * Force Event - generate transfer events for all TRBs for this endpoint
 * This will tell the HC to ignore the IOC and ISP flags (for debugging only).
 */
#define	FORCE_EVENT	(0x1)
#define ERROR_COUNT(p)	(((p) & 0x3) << 1)
#define CTX_TO_EP_TYPE(p)	(((p) >> 3) & 0x7)
#define EP_TYPE(p)	((p) << 3)
#define ISOC_OUT_EP	1
#define BULK_OUT_EP	2
#define INT_OUT_EP	3
#define CTRL_EP		4
#define ISOC_IN_EP	5
#define BULK_IN_EP	6
#define INT_IN_EP	7
/* bit 6 reserved */
/* bit 7 is Host Initiate Disable - for disabling stream selection */
#define MAX_BURST(p)	(((p)&0xff) << 8)
#define CTX_TO_MAX_BURST(p)	(((p) >> 8) & 0xff)
#define MAX_PACKET(p)	(((p)&0xffff) << 16)
#define MAX_PACKET_MASK		(0xffff << 16)
#define MAX_PACKET_DECODED(p)	(((p) >> 16) & 0xffff)

/* tx_info bitmasks */
#define EP_AVG_TRB_LENGTH(p)		((p) & 0xffff)
#define EP_MAX_ESIT_PAYLOAD_LO(p)	(((p) & 0xffff) << 16)
#define EP_MAX_ESIT_PAYLOAD_HI(p)	((((p) >> 16) & 0xff) << 24)
#define CTX_TO_MAX_ESIT_PAYLOAD(p)	(((p) >> 16) & 0xffff)

/* deq bitmasks */
#define EP_CTX_CYCLE_MASK		(1 << 0)
#define SCTX_DEQ_MASK			(~0xfL)


/**
 * struct xhci_input_control_context
 * Input control context; see section 6.2.5.
 *
 * @drop_context:	set the bit of the endpoint context you want to disable
 * @add_context:	set the bit of the endpoint context you want to enable
 */
typedef struct xhci_input_control_ctx {
	uint32_t	drop_flags;
	uint32_t	add_flags;
	uint32_t	rsvd2[6];
} xhci_input_control_ctx_t;


typedef struct xhci_erst_entry {
	/* 64-bit event ring segment address */
	uint64_t	seg_addr;
	uint32_t	seg_size;
	/* Set to zero */
	uint32_t	rsvd;
} xhci_erst_entry_t;


typedef struct xhci_ring
{
    size_t              max_trb_count;
    size_t              enqueue_ptr;
    size_t              denqueue_ptr;
    xhci_trb_t          *trbs;
//    uintptr_t           trbs_paddr;
    uint8_t             rcs_bit;
    xhci_erst_entry_t   *seg_table;
//    uintptr_t           seg_table_paddr;
//    volatile xhci_intr_reg_t     *interrupter;
} xhci_ring_t;

struct class_client;

typedef struct pending_urb {
    uint8_t              valid;
    uint8_t              slot_id;
    uint8_t              ep_id;
    uint8_t              dir;
    uint32_t             len;
    uint32_t             offset;
    void                *dma_buf;
    struct class_client *client;
} pending_urb_t;

typedef struct xhci_virt_ep
{
    xhci_ring_t *tr_ring;
    uint16_t    max_packet;
    /* async completion state — set by IRQ handler, polled by transfer submitter */
    volatile int        comp_done;
    volatile int        comp_code;
    volatile uint32_t   residual;
    /* deferred IPC reply (Stage 3 — cross-process bulk/int completion) */
    uint64_t    deferred_sender;
    pid_t       waiter_pid;
    /* Stage 6: direct async URB from class driver */
    pending_urb_t       pending_urb;
} xhci_virt_ep_t;

#define EP_CTX_PER_DEV		31

typedef struct xhci_virt_dev
{
    int     slot_id;
    uint8_t port;
    uint8_t port_speed;
    void    *out_ctx;
    void    *in_ctx;
    xhci_virt_ep_t *eps[EP_CTX_PER_DEV];  // Transfer endpoints 1-31 (endpoint 0 is control)
} xhci_virt_dev_t;

typedef struct xhci_port {
	volatile uint32_t	*addr;
	int			hw_portnum;
	int			portnum;
    int         device_connected;
    int         device_disconnected;
	uint8_t		maj_rev;
	uint8_t		min_rev;
} xhci_port_t;

#define MAX_CLASS_CLIENTS 8

typedef struct class_client {
    uint8_t       slot_id;       // device slot owned by this client (0 = free)
    cap_id_t      notif_cap;
    cap_id_t      data_shm_cap;
    void         *data_shm;
    cap_id_t      result_shm_cap;
    urb_result_t *result_table;
} class_client_t;

typedef struct xhci
{
    volatile struct xhci_cap_regs *cap_regs;
    volatile struct xhci_op_regs  *op_regs;
    volatile struct xhci_run_regs *run_regs;
    volatile struct xhci_doorbell_array *dba;
    volatile uint32_t *head_xcap_ptr;

    /* Cached register copies of read-only HC data */
	uint32_t		hcs_params1;
	uint32_t		hcs_params2;
	uint32_t		hcs_params3;
	uint32_t		hcc_params;
	uint32_t		hcc_params2;

    uint32_t max_ports;
    uint32_t max_slots;
    uint32_t max_scratchpads;
    uint32_t max_erst;

    dma_pool_t  *xmem_pool;

    xhci_virt_dev_t *devs[MAX_HC_SLOTS];

    // uint64_t dcbaap_paddr;
    paddr_t *dcbaap;
    // uint64_t dcbaap_vaddr[MAX_HC_SLOTS];

    paddr_t *scratchpad_array;

    size_t   max_trb_count;
    size_t   enqueue_ptr;
    xhci_trb_t *trbs;
//    uintptr_t trbs_paddr;
    uint8_t  rcs_bit;

    xhci_ring_t *event_ring;

    bool        irq_completed;
    list_head_t competion_events;
    xhci_event_cmd_t *completion_trb;
    uint32_t    hub_events;
#define XHCI_HUB_EVENT_PORT_CHANGE      0x1
#define XHCI_HUB_EVENT_PORT_DISCONNECT  0x2
    uint8_t     active_port;

    xhci_port_t *hw_ports;

    class_client_t class_clients[MAX_CLASS_CLIENTS];

} xhci_t;


static inline void _inc_enqueue_ptr(xhci_ring_t *ring)
{
    if (++ring->enqueue_ptr == ring->max_trb_count - 1) {
        ring->trbs[ring->max_trb_count - 1].control =
            TRB_TYPE(TRB_LINK) | TRB_TC | ring->rcs_bit;
        ring->enqueue_ptr = 0;
        ring->rcs_bit = !ring->rcs_bit;
    }
}

static inline bool _has_unprocessed_events(xhci_ring_t *event_ring)
{
    return (event_ring->trbs[event_ring->denqueue_ptr].cycle_bit == event_ring->rcs_bit);
}

static inline void _ring_doorbell(xhci_t *xhci, uint8_t doorbell, uint8_t target)
{
    xhci->dba->doorbell[doorbell] = (uint32_t)target;
//    putreg32((uint32_t)target, (uint64_t)&xhci->dba->doorbell[doorbell]);
}

static inline void _ring_command_doorbell(xhci_t *xhci)
{
    _ring_doorbell(xhci, 0, 0);
}

static inline void _ring_control_endpoint_doorbell(xhci_t *xhci, uint8_t doorbell)
{
    _ring_doorbell(xhci, doorbell, 1);
}

#include <riscv.h>
extern xhci_t *xhci_dev;

static inline uint32_t _read_portsc_reg(uint8_t port)
{
    return getreg32((uint64_t)&xhci_dev->op_regs->port_status_base + (0x10 * port));
}

static inline void _write_portsc_reg(uint32_t val, uint8_t port)
{
    putreg32(val, (uint64_t)&xhci_dev->op_regs->port_status_base + (0x10 * port));
}

static inline uint8_t _get_port_speed(uint8_t port) {
    uint32_t portsc = _read_portsc_reg(port);
    return (uint8_t)DEV_PORT_SPEED(portsc);
}

static inline uint32_t _get_max_ports()
{
    return xhci_dev->max_ports;
}

int xhci_init(void *base_addr, int irq);
int xhci_start_host();

int xhci_mem_init(xhci_t *xhci);
int xhci_alloc_virt_device(xhci_t *xhci, int slot_id, uint8_t port,
                            uint8_t parent_slot, uint32_t route_string, uint8_t speed,
                            uint8_t tt_slot, uint8_t tt_port);
xhci_ring_t *xhci_alloc_transfer_ring(xhci_t *xhci, size_t trb_count);
xhci_event_cmd_t *xhci_send_command(xhci_t *xhci, xhci_trb_t *trb, uint32_t timeout_ms);
uint8_t xhci_enable_device_slot();
int xhci_address_device(uint8_t slot_id, bool bsr);
int xhci_evaluate_context(uint8_t slot_id) ;
int xhci_ep0_control_transfer(xhci_t *xhci, uint8_t slot_id, usb_control_request_t *req, 
                             void *data, size_t data_len, bool data_in);
int xhci_control_transfer(xhci_t *xhci, uint8_t slot_id, usb_control_request_t *req, 
                         void *data, size_t data_len);
int xhci_get_descriptor(xhci_t *xhci, uint8_t slot_id, uint8_t desc_type, 
                       uint8_t desc_index, uint16_t lang_id, void *buffer, 
                       size_t buffer_size, size_t *actual_length);
int xhci_set_configuration(xhci_t *xhci, uint8_t slot_id, uint8_t config_value);
int xhci_set_address(xhci_t *xhci, uint8_t slot_id, uint8_t device_address);
int xhci_enumerate_device(uint8_t slot_id);
const char* _usb_speed_to_string(uint8_t speed);
//int _handle_new_device(uint8_t port_id);
int xhci_configure_endpoint(uint8_t slot_id) ;
int xhci_config_eps(xhci_t *xhci, uint8_t slot_id, uint8_t ep_nums, usb_ep_desc_t *ep_desc);
void _dump_device_context(xhci_virt_dev_t *dev);
int xhci_disable_device_slot(uint8_t slot_id);
int xhci_stop_ep(uint8_t slot_id, uint8_t ep_id);
int xhci_reset_ep(uint8_t slot_id, uint8_t ep_id);
void xhci_free_virt_device(xhci_t *xhci, uint8_t slot_id);
int xhci_submit_bulk_transfer(xhci_t *xhci, uint8_t slot_id, uint8_t ep_id,
                               void *buf, uint32_t len, bool data_in,
                               uint32_t *out_residual);
int xhci_arm_bulk_transfer(xhci_t *xhci, uint8_t slot_id, uint8_t ep_id,
                            void *buf, uint32_t len, bool data_in);
#endif /* __XHCI_H__ */