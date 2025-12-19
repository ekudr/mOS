#ifndef __EHCI_H__
#define __EHCI_H__

#include <stdint.h>

#include "k1x.h"
#include "phy.h"

/* Section 2.2.3 - N_PORTS */
#define MAX_HC_PORTS		15

/*
 * Register Space.
 */
struct ehci_hccr {
	uint32_t cr_capbase;
#define HC_LENGTH(p)		(((p) >> 0) & 0x00ff)
#define HC_VERSION(p)		(((p) >> 16) & 0xffff)
	uint32_t cr_hcsparams;
#define HCS_PPC(p)		((p) & (1 << 4))
#define HCS_INDICATOR(p)	((p) & (1 << 16)) /* Port indicators */
#define HCS_N_PORTS(p)		(((p) >> 0) & 0xf)
	uint32_t cr_hccparams;
	uint8_t cr_hcsp_portrt[8];
} __attribute__ ((packed, aligned(4)));

struct ehci_hcor {
	uint32_t or_usbcmd;
#define CMD_FRAMELIST_SIZE             0xC     // valid values are:
#define CMD_FRAMELIST_1024             0x0
#define CMD_FRAMELIST_512              0x4
#define CMD_FRAMELIST_256              0x8
#define CMD_PARK	(1 << 11)		/* enable "park" */
#define CMD_PARK_CNT(c)	(((c) >> 8) & 3)	/* how many transfers to park */
#define CMD_LRESET	(1 << 7)		/* partial reset */
#define CMD_IAAD	(1 << 6)		/* "doorbell" interrupt */
#define CMD_ASE		(1 << 5)		/* async schedule enable */
#define CMD_PSE		(1 << 4)		/* periodic schedule enable */
#define CMD_RESET	(1 << 1)		/* reset HC not bus */
#define CMD_RUN		(1 << 0)		/* start/stop HC */
	uint32_t or_usbsts;
#define STS_ASS		(1 << 15)
#define	STS_PSS		(1 << 14)
#define STS_HALT	(1 << 12)
#define STS_IAA		(1 << 5)
	uint32_t or_usbintr;
#define INTR_UE         (1 << 0)                /* USB interrupt enable */
#define INTR_UEE        (1 << 1)                /* USB error interrupt enable */
#define INTR_PCE        (1 << 2)                /* Port change detect enable */
#define INTR_SEE        (1 << 4)                /* system error enable */
#define INTR_AAE        (1 << 5)                /* Interrupt on async adavance enable */
	uint32_t or_frindex;
	uint32_t or_ctrldssegment;
	uint32_t or_periodiclistbase;
	uint32_t or_asynclistaddr;
	uint32_t _reserved_0_;
	uint32_t or_burstsize;
	uint32_t or_txfilltuning;
#define TXFIFO_THRESH_MASK		(0x3f << 16)
#define TXFIFO_THRESH(p)		((p & 0x3f) << 16)
	uint32_t _reserved_1_[6];
	uint32_t or_configflag;
#define FLAG_CF		(1 << 0)	/* true:  we'll support "high speed" */
	uint32_t or_portsc[MAX_HC_PORTS];
#define PORTSC_PSPD(x)		(((x) >> 26) & 0x3)
#define PORTSC_PSPD_FS			0x0
#define PORTSC_PSPD_LS			0x1
#define PORTSC_PSPD_HS			0x2
#define PORTSC_FSL_PFSC		BIT(24) /* PFSC bit to disable HS chirping */

	uint32_t or_systune;
} __attribute__ ((packed, aligned(4)));

typedef struct 
{
    void *base;
    clk_t *clk;  // host clock
    clk_t *rst;  // Host reset
    usb_phy_t   *phy;
    uint32_t    irq;

    struct ehci_hccr *hccr;
    struct ehci_hcor *hcor;
} usb2_dev_t;


static inline uint32_t ehci_readl(void *reg)
{
    return (uint32_t) getreg32((uint64_t)reg);
}

static inline void ehci_writel(void *reg, uint32_t v)
{
    putreg32(v, (uint64_t)reg);
}

static void ehci_checkPortLineStatus(usb2_dev_t *d, uint8_t j);

static void ehci_detectDevice(usb2_dev_t *d, uint8_t j);

#endif /* __EHCI_H__ */