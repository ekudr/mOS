#ifndef __DWC3_H__
#define __DWC3_H__

#include <stdint.h>

#include "k1x.h"
#include "phy.h"

#define DWC3_MSG_MAX	500

/* Global constants */
#define DWC3_PULL_UP_TIMEOUT	500	/* ms */
#define DWC3_BOUNCE_SIZE	1024	/* size of a superspeed bulk */
#define DWC3_EP0_SETUP_SIZE	512
#define DWC3_ENDPOINTS_NUM	32
#define DWC3_XHCI_RESOURCES_NUM	2
#define DWC3_ISOC_MAX_RETRIES	5

#define DWC3_SCRATCHBUF_SIZE	4096	/* each buffer is assumed to be 4KiB */
#define DWC3_EVENT_BUFFERS_SIZE	4096
#define DWC3_EVENT_TYPE_MASK	0xfe

#define DWC3_EVENT_TYPE_DEV	0
#define DWC3_EVENT_TYPE_CARKIT	3
#define DWC3_EVENT_TYPE_I2C	4

#define DWC3_DEVICE_EVENT_DISCONNECT		0
#define DWC3_DEVICE_EVENT_RESET			1
#define DWC3_DEVICE_EVENT_CONNECT_DONE		2
#define DWC3_DEVICE_EVENT_LINK_STATUS_CHANGE	3
#define DWC3_DEVICE_EVENT_WAKEUP		4
#define DWC3_DEVICE_EVENT_HIBER_REQ		5
#define DWC3_DEVICE_EVENT_SUSPEND		6
#define DWC3_DEVICE_EVENT_SOF			7
#define DWC3_DEVICE_EVENT_ERRATIC_ERROR		9
#define DWC3_DEVICE_EVENT_CMD_CMPL		10
#define DWC3_DEVICE_EVENT_OVERFLOW		11

/* Controller's role while using the OTG block */
#define DWC3_OTG_ROLE_IDLE	0
#define DWC3_OTG_ROLE_HOST	1
#define DWC3_OTG_ROLE_DEVICE	2

#define DWC3_GEVNTCOUNT_MASK	0xfffc
#define DWC3_GEVNTCOUNT_EHB	BIT(31)
#define DWC3_GSNPSID_MASK	0xffff0000
#define DWC3_GSNPSREV_MASK	0xffff
#define DWC3_GSNPS_ID(p)	(((p) & DWC3_GSNPSID_MASK) >> 16)

/* DWC3 registers memory space boundries */
#define DWC3_XHCI_REGS_START		0x0
#define DWC3_XHCI_REGS_END		0x7fff
#define DWC3_GLOBALS_REGS_START		0xc100
#define DWC3_GLOBALS_REGS_END		0xc6ff
#define DWC3_DEVICE_REGS_START		0xc700
#define DWC3_DEVICE_REGS_END		0xcbff
#define DWC3_OTG_REGS_START		0xcc00
#define DWC3_OTG_REGS_END		0xccff

/* Global Registers */
#define DWC3_GSBUSCFG0		0xc100
#define DWC3_GSBUSCFG1		0xc104
#define DWC3_GTXTHRCFG		0xc108
#define DWC3_GRXTHRCFG		0xc10c
#define DWC3_GCTL		0xc110
#define DWC3_GEVTEN		0xc114
#define DWC3_GSTS		0xc118
#define DWC3_GUCTL1		0xc11c
#define DWC3_GSNPSID		0xc120
#define DWC3_GGPIO		0xc124
#define DWC3_GUID		0xc128
#define DWC3_GUCTL		0xc12c
#define DWC3_GBUSERRADDR0	0xc130
#define DWC3_GBUSERRADDR1	0xc134
#define DWC3_GPRTBIMAP0		0xc138
#define DWC3_GPRTBIMAP1		0xc13c
#define DWC3_GHWPARAMS0		0xc140
#define DWC3_GHWPARAMS1		0xc144
#define DWC3_GHWPARAMS2		0xc148
#define DWC3_GHWPARAMS3		0xc14c
#define DWC3_GHWPARAMS4		0xc150
#define DWC3_GHWPARAMS5		0xc154
#define DWC3_GHWPARAMS6		0xc158
#define DWC3_GHWPARAMS7		0xc15c
#define DWC3_GDBGFIFOSPACE	0xc160
#define DWC3_GDBGLTSSM		0xc164
#define DWC3_GDBGBMU		0xc16c
#define DWC3_GDBGLSPMUX		0xc170
#define DWC3_GDBGLSP		0xc174
#define DWC3_GDBGEPINFO0	0xc178
#define DWC3_GDBGEPINFO1	0xc17c
#define DWC3_GPRTBIMAP_HS0	0xc180
#define DWC3_GPRTBIMAP_HS1	0xc184
#define DWC3_GPRTBIMAP_FS0	0xc188
#define DWC3_GPRTBIMAP_FS1	0xc18c
#define DWC3_GUCTL2		0xc19c

#define DWC3_VER_NUMBER		0xc1a0
#define DWC3_VER_TYPE		0xc1a4

#define DWC3_GUSB2PHYCFG(n)	(0xc200 + ((n) * 0x04))
#define DWC3_GUSB2I2CCTL(n)	(0xc240 + ((n) * 0x04))

#define DWC3_GUSB2PHYACC(n)	(0xc280 + ((n) * 0x04))

#define DWC3_GUSB3PIPECTL(n)	(0xc2c0 + ((n) * 0x04))

#define DWC3_GTXFIFOSIZ(n)	(0xc300 + ((n) * 0x04))
#define DWC3_GRXFIFOSIZ(n)	(0xc380 + ((n) * 0x04))

#define DWC3_GEVNTADRLO(n)	(0xc400 + ((n) * 0x10))
#define DWC3_GEVNTADRHI(n)	(0xc404 + ((n) * 0x10))
#define DWC3_GEVNTSIZ(n)	(0xc408 + ((n) * 0x10))
#define DWC3_GEVNTCOUNT(n)	(0xc40c + ((n) * 0x10))

#define DWC3_GHWPARAMS8		0xc600
#define DWC3_GUCTL3		0xc60c
#define DWC3_GFLADJ		0xc630
#define DWC3_GHWPARAMS9		0xc6e0

/* Device Registers */
#define DWC3_DCFG		0xc700
#define DWC3_DCTL		0xc704
#define DWC3_DEVTEN		0xc708
#define DWC3_DSTS		0xc70c
#define DWC3_DGCMDPAR		0xc710
#define DWC3_DGCMD		0xc714
#define DWC3_DALEPENA		0xc720
#define DWC3_DCFG1		0xc740 /* DWC_usb32 only */

#define DWC3_DEP_BASE(n)	(0xc800 + ((n) * 0x10))
#define DWC3_DEPCMDPAR2		0x00
#define DWC3_DEPCMDPAR1		0x04
#define DWC3_DEPCMDPAR0		0x08
#define DWC3_DEPCMD		0x0c

#define DWC3_DEV_IMOD(n)	(0xca00 + ((n) * 0x4))

/* OTG Registers */
#define DWC3_OCFG		0xcc00
#define DWC3_OCTL		0xcc04
#define DWC3_OEVT		0xcc08
#define DWC3_OEVTEN		0xcc0C
#define DWC3_OSTS		0xcc10

/* Bit fields */

/* Global SoC Bus Configuration INCRx Register 0 */
#define DWC3_GSBUSCFG0_INCR256BRSTENA	(1 << 7) /* INCR256 burst */
#define DWC3_GSBUSCFG0_INCR128BRSTENA	(1 << 6) /* INCR128 burst */
#define DWC3_GSBUSCFG0_INCR64BRSTENA	(1 << 5) /* INCR64 burst */
#define DWC3_GSBUSCFG0_INCR32BRSTENA	(1 << 4) /* INCR32 burst */
#define DWC3_GSBUSCFG0_INCR16BRSTENA	(1 << 3) /* INCR16 burst */
#define DWC3_GSBUSCFG0_INCR8BRSTENA	(1 << 2) /* INCR8 burst */
#define DWC3_GSBUSCFG0_INCR4BRSTENA	(1 << 1) /* INCR4 burst */
#define DWC3_GSBUSCFG0_INCRBRSTENA	(1 << 0) /* undefined length enable */
#define DWC3_GSBUSCFG0_INCRBRST_MASK	0xff

/* Global Debug LSP MUX Select */
#define DWC3_GDBGLSPMUX_ENDBC		BIT(15)	/* Host only */
#define DWC3_GDBGLSPMUX_HOSTSELECT(n)	((n) & 0x3fff)
#define DWC3_GDBGLSPMUX_DEVSELECT(n)	(((n) & 0xf) << 4)
#define DWC3_GDBGLSPMUX_EPSELECT(n)	((n) & 0xf)

/* Global Debug Queue/FIFO Space Available Register */
#define DWC3_GDBGFIFOSPACE_NUM(n)	((n) & 0x1f)
#define DWC3_GDBGFIFOSPACE_TYPE(n)	(((n) << 5) & 0x1e0)
#define DWC3_GDBGFIFOSPACE_SPACE_AVAILABLE(n) (((n) >> 16) & 0xffff)

#define DWC3_TXFIFO		0
#define DWC3_RXFIFO		1
#define DWC3_TXREQQ		2
#define DWC3_RXREQQ		3
#define DWC3_RXINFOQ		4
#define DWC3_PSTATQ		5
#define DWC3_DESCFETCHQ		6
#define DWC3_EVENTQ		7
#define DWC3_AUXEVENTQ		8

/* Global RX Threshold Configuration Register */
#define DWC3_GRXTHRCFG_MAXRXBURSTSIZE(n) (((n) & 0x1f) << 19)
#define DWC3_GRXTHRCFG_RXPKTCNT(n) (((n) & 0xf) << 24)
#define DWC3_GRXTHRCFG_PKTCNTSEL BIT(29)

/* Global RX Threshold Configuration Register for DWC_usb31 only */
#define DWC31_GRXTHRCFG_MAXRXBURSTSIZE(n)	(((n) & 0x1f) << 16)
#define DWC31_GRXTHRCFG_RXPKTCNT(n)		(((n) & 0x1f) << 21)
#define DWC31_GRXTHRCFG_PKTCNTSEL		BIT(26)
#define DWC31_RXTHRNUMPKTSEL_HS_PRD		BIT(15)
#define DWC31_RXTHRNUMPKT_HS_PRD(n)		(((n) & 0x3) << 13)
#define DWC31_RXTHRNUMPKTSEL_PRD		BIT(10)
#define DWC31_RXTHRNUMPKT_PRD(n)		(((n) & 0x1f) << 5)
#define DWC31_MAXRXBURSTSIZE_PRD(n)		((n) & 0x1f)

/* Global TX Threshold Configuration Register for DWC_usb31 only */
#define DWC31_GTXTHRCFG_MAXTXBURSTSIZE(n)	(((n) & 0x1f) << 16)
#define DWC31_GTXTHRCFG_TXPKTCNT(n)		(((n) & 0x1f) << 21)
#define DWC31_GTXTHRCFG_PKTCNTSEL		BIT(26)
#define DWC31_TXTHRNUMPKTSEL_HS_PRD		BIT(15)
#define DWC31_TXTHRNUMPKT_HS_PRD(n)		(((n) & 0x3) << 13)
#define DWC31_TXTHRNUMPKTSEL_PRD		BIT(10)
#define DWC31_TXTHRNUMPKT_PRD(n)		(((n) & 0x1f) << 5)
#define DWC31_MAXTXBURSTSIZE_PRD(n)		((n) & 0x1f)

/* Global Configuration Register */
#define DWC3_GCTL_PWRDNSCALE(n)	((n) << 19)
#define DWC3_GCTL_PWRDNSCALE_MASK	GENMASK(31, 19)
#define DWC3_GCTL_U2RSTECN	BIT(16)
#define DWC3_GCTL_RAMCLKSEL(x)	(((x) & DWC3_GCTL_CLK_MASK) << 6)
#define DWC3_GCTL_CLK_BUS	(0)
#define DWC3_GCTL_CLK_PIPE	(1)
#define DWC3_GCTL_CLK_PIPEHALF	(2)
#define DWC3_GCTL_CLK_MASK	(3)

#define DWC3_GCTL_PRTCAP(n)	(((n) & (3 << 12)) >> 12)
#define DWC3_GCTL_PRTCAPDIR(n)	((n) << 12)
#define DWC3_GCTL_PRTCAP_HOST	1
#define DWC3_GCTL_PRTCAP_DEVICE	2
#define DWC3_GCTL_PRTCAP_OTG	3

#define DWC3_GCTL_CORESOFTRESET		BIT(11)
#define DWC3_GCTL_SOFITPSYNC		BIT(10)
#define DWC3_GCTL_SCALEDOWN(n)		((n) << 4)
#define DWC3_GCTL_SCALEDOWN_MASK	DWC3_GCTL_SCALEDOWN(3)
#define DWC3_GCTL_DISSCRAMBLE		BIT(3)
#define DWC3_GCTL_U2EXIT_LFPS		BIT(2)
#define DWC3_GCTL_GBLHIBERNATIONEN	BIT(1)
#define DWC3_GCTL_DSBLCLKGTNG		BIT(0)

/* Global User Control Register */
#define DWC3_GUCTL_HSTINAUTORETRY	BIT(14)

/* Global User Control 1 Register */
#define DWC3_GUCTL1_DEV_DECOUPLE_L1L2_EVT	BIT(31)
#define DWC3_GUCTL1_TX_IPGAP_LINECHECK_DIS	BIT(28)
#define DWC3_GUCTL1_DEV_FORCE_20_CLK_FOR_30_CLK	BIT(26)
#define DWC3_GUCTL1_DEV_L1_EXIT_BY_HW		BIT(24)
#define DWC3_GUCTL1_PARKMODE_DISABLE_SS		BIT(17)
#define DWC3_GUCTL1_RESUME_OPMODE_HS_HOST	BIT(10)

/* Global Status Register */
#define DWC3_GSTS_OTG_IP	BIT(10)
#define DWC3_GSTS_BC_IP		BIT(9)
#define DWC3_GSTS_ADP_IP	BIT(8)
#define DWC3_GSTS_HOST_IP	BIT(7)
#define DWC3_GSTS_DEVICE_IP	BIT(6)
#define DWC3_GSTS_CSR_TIMEOUT	BIT(5)
#define DWC3_GSTS_BUS_ERR_ADDR_VLD	BIT(4)
#define DWC3_GSTS_CURMOD(n)	((n) & 0x3)
#define DWC3_GSTS_CURMOD_DEVICE	0
#define DWC3_GSTS_CURMOD_HOST	1

/* Global USB2 PHY Configuration Register */
#define DWC3_GUSB2PHYCFG_PHYSOFTRST	BIT(31)
#define DWC3_GUSB2PHYCFG_U2_FREECLK_EXISTS	BIT(30)
#define DWC3_GUSB2PHYCFG_SUSPHY		BIT(6)
#define DWC3_GUSB2PHYCFG_ULPI_UTMI	BIT(4)
#define DWC3_GUSB2PHYCFG_ENBLSLPM	BIT(8)
#define DWC3_GUSB2PHYCFG_PHYIF(n)	(n << 3)
#define DWC3_GUSB2PHYCFG_PHYIF_MASK	DWC3_GUSB2PHYCFG_PHYIF(1)
#define DWC3_GUSB2PHYCFG_USBTRDTIM(n)	(n << 10)
#define DWC3_GUSB2PHYCFG_USBTRDTIM_MASK	DWC3_GUSB2PHYCFG_USBTRDTIM(0xf)
#define USBTRDTIM_UTMI_8_BIT		9
#define USBTRDTIM_UTMI_16_BIT		5
#define UTMI_PHYIF_16_BIT		1
#define UTMI_PHYIF_8_BIT		0

/* Global USB2 PHY Vendor Control Register */
#define DWC3_GUSB2PHYACC_NEWREGREQ	BIT(25)
#define DWC3_GUSB2PHYACC_DONE		BIT(24)
#define DWC3_GUSB2PHYACC_BUSY		BIT(23)
#define DWC3_GUSB2PHYACC_WRITE		BIT(22)
#define DWC3_GUSB2PHYACC_ADDR(n)	(n << 16)
#define DWC3_GUSB2PHYACC_EXTEND_ADDR(n)	(n << 8)
#define DWC3_GUSB2PHYACC_DATA(n)	(n & 0xff)

/* Global USB3 PIPE Control Register */
#define DWC3_GUSB3PIPECTL_PHYSOFTRST	BIT(31)
#define DWC3_GUSB3PIPECTL_U2SSINP3OK	BIT(29)
#define DWC3_GUSB3PIPECTL_DISRXDETINP3	BIT(28)
#define DWC3_GUSB3PIPECTL_UX_EXIT_PX	BIT(27)
#define DWC3_GUSB3PIPECTL_REQP1P2P3	BIT(24)
#define DWC3_GUSB3PIPECTL_DEP1P2P3(n)	((n) << 19)
#define DWC3_GUSB3PIPECTL_DEP1P2P3_MASK	DWC3_GUSB3PIPECTL_DEP1P2P3(7)
#define DWC3_GUSB3PIPECTL_DEP1P2P3_EN	DWC3_GUSB3PIPECTL_DEP1P2P3(1)
#define DWC3_GUSB3PIPECTL_DEPOCHANGE	BIT(18)
#define DWC3_GUSB3PIPECTL_SUSPHY	BIT(17)
#define DWC3_GUSB3PIPECTL_LFPSFILT	BIT(9)
#define DWC3_GUSB3PIPECTL_RX_DETOPOLL	BIT(8)
#define DWC3_GUSB3PIPECTL_TX_DEEPH_MASK	DWC3_GUSB3PIPECTL_TX_DEEPH(3)
#define DWC3_GUSB3PIPECTL_TX_DEEPH(n)	((n) << 1)

/* Global TX Fifo Size Register */
#define DWC31_GTXFIFOSIZ_TXFRAMNUM	BIT(15)		/* DWC_usb31 only */
#define DWC31_GTXFIFOSIZ_TXFDEP(n)	((n) & 0x7fff)	/* DWC_usb31 only */
#define DWC3_GTXFIFOSIZ_TXFDEP(n)	((n) & 0xffff)
#define DWC3_GTXFIFOSIZ_TXFSTADDR(n)	((n) & 0xffff0000)

/* Global RX Fifo Size Register */
#define DWC31_GRXFIFOSIZ_RXFDEP(n)	((n) & 0x7fff)	/* DWC_usb31 only */
#define DWC3_GRXFIFOSIZ_RXFDEP(n)	((n) & 0xffff)

/* Global Event Size Registers */
#define DWC3_GEVNTSIZ_INTMASK		BIT(31)
#define DWC3_GEVNTSIZ_SIZE(n)		((n) & 0xffff)

/* Global HWPARAMS0 Register */
#define DWC3_GHWPARAMS0_MODE(n)		((n) & 0x3)
#define DWC3_GHWPARAMS0_MODE_GADGET	0
#define DWC3_GHWPARAMS0_MODE_HOST	1
#define DWC3_GHWPARAMS0_MODE_DRD	2
#define DWC3_GHWPARAMS0_MBUS_TYPE(n)	(((n) >> 3) & 0x7)
#define DWC3_GHWPARAMS0_SBUS_TYPE(n)	(((n) >> 6) & 0x3)
#define DWC3_GHWPARAMS0_MDWIDTH(n)	(((n) >> 8) & 0xff)
#define DWC3_GHWPARAMS0_SDWIDTH(n)	(((n) >> 16) & 0xff)
#define DWC3_GHWPARAMS0_AWIDTH(n)	(((n) >> 24) & 0xff)

/* Global HWPARAMS1 Register */
#define DWC3_GHWPARAMS1_EN_PWROPT(n)	(((n) & (3 << 24)) >> 24)
#define DWC3_GHWPARAMS1_EN_PWROPT_NO	0
#define DWC3_GHWPARAMS1_EN_PWROPT_CLK	1
#define DWC3_GHWPARAMS1_EN_PWROPT_HIB	2
#define DWC3_GHWPARAMS1_PWROPT(n)	((n) << 24)
#define DWC3_GHWPARAMS1_PWROPT_MASK	DWC3_GHWPARAMS1_PWROPT(3)
#define DWC3_GHWPARAMS1_ENDBC		BIT(31)

/* Global HWPARAMS3 Register */
#define DWC3_GHWPARAMS3_SSPHY_IFC(n)		((n) & 3)
#define DWC3_GHWPARAMS3_SSPHY_IFC_DIS		0
#define DWC3_GHWPARAMS3_SSPHY_IFC_GEN1		1
#define DWC3_GHWPARAMS3_SSPHY_IFC_GEN2		2 /* DWC_usb31 only */
#define DWC3_GHWPARAMS3_HSPHY_IFC(n)		(((n) & (3 << 2)) >> 2)
#define DWC3_GHWPARAMS3_HSPHY_IFC_DIS		0
#define DWC3_GHWPARAMS3_HSPHY_IFC_UTMI		1
#define DWC3_GHWPARAMS3_HSPHY_IFC_ULPI		2
#define DWC3_GHWPARAMS3_HSPHY_IFC_UTMI_ULPI	3
#define DWC3_GHWPARAMS3_FSPHY_IFC(n)		(((n) & (3 << 4)) >> 4)
#define DWC3_GHWPARAMS3_FSPHY_IFC_DIS		0
#define DWC3_GHWPARAMS3_FSPHY_IFC_ENA		1

/* Global HWPARAMS4 Register */
#define DWC3_GHWPARAMS4_HIBER_SCRATCHBUFS(n)	(((n) & (0x0f << 13)) >> 13)
#define DWC3_MAX_HIBER_SCRATCHBUFS		15

/* Global HWPARAMS6 Register */
#define DWC3_GHWPARAMS6_BCSUPPORT		BIT(14)
#define DWC3_GHWPARAMS6_OTG3SUPPORT		BIT(13)
#define DWC3_GHWPARAMS6_ADPSUPPORT		BIT(12)
#define DWC3_GHWPARAMS6_HNPSUPPORT		BIT(11)
#define DWC3_GHWPARAMS6_SRPSUPPORT		BIT(10)
#define DWC3_GHWPARAMS6_EN_FPGA			BIT(7)

/* DWC_usb32 only */
#define DWC3_GHWPARAMS6_MDWIDTH(n)		((n) & (0x3 << 8))

/* Global HWPARAMS7 Register */
#define DWC3_GHWPARAMS7_RAM1_DEPTH(n)	((n) & 0xffff)
#define DWC3_GHWPARAMS7_RAM2_DEPTH(n)	(((n) >> 16) & 0xffff)

/* Global HWPARAMS9 Register */
#define DWC3_GHWPARAMS9_DEV_TXF_FLUSH_BYPASS	BIT(0)
#define DWC3_GHWPARAMS9_DEV_MST			BIT(1)

/* Global Frame Length Adjustment Register */
#define DWC3_GFLADJ_30MHZ_SDBND_SEL		BIT(7)
#define DWC3_GFLADJ_30MHZ_MASK			0x3f
#define DWC3_GFLADJ_REFCLK_FLADJ_MASK		GENMASK(21, 8)
#define DWC3_GFLADJ_REFCLK_LPM_SEL		BIT(23)
#define DWC3_GFLADJ_240MHZDECR			GENMASK(30, 24)
#define DWC3_GFLADJ_240MHZDECR_PLS1		BIT(31)

/* Global User Control Register*/
#define DWC3_GUCTL_REFCLKPER_MASK		0xffc00000
#define DWC3_GUCTL_REFCLKPER_SEL		22

/* Global User Control Register 2 */
#define DWC3_GUCTL2_RST_ACTBITLATER		BIT(14)

/* Global User Control Register 3 */
#define DWC3_GUCTL3_SPLITDISABLE		BIT(14)

/* Device Configuration Register */
#define DWC3_DCFG_NUMLANES(n)	(((n) & 0x3) << 30) /* DWC_usb32 only */

#define DWC3_DCFG_DEVADDR(addr)	((addr) << 3)
#define DWC3_DCFG_DEVADDR_MASK	DWC3_DCFG_DEVADDR(0x7f)

#define DWC3_DCFG_SPEED_MASK	(7 << 0)
#define DWC3_DCFG_SUPERSPEED_PLUS (5 << 0)  /* DWC_usb31 only */
#define DWC3_DCFG_SUPERSPEED	(4 << 0)
#define DWC3_DCFG_HIGHSPEED	(0 << 0)
#define DWC3_DCFG_FULLSPEED	BIT(0)

#define DWC3_DCFG_NUMP_SHIFT	17
#define DWC3_DCFG_NUMP(n)	(((n) >> DWC3_DCFG_NUMP_SHIFT) & 0x1f)
#define DWC3_DCFG_NUMP_MASK	(0x1f << DWC3_DCFG_NUMP_SHIFT)
#define DWC3_DCFG_LPM_CAP	BIT(22)
#define DWC3_DCFG_IGNSTRMPP	BIT(23)

/* Device Control Register */
#define DWC3_DCTL_RUN_STOP	BIT(31)
#define DWC3_DCTL_CSFTRST	BIT(30)
#define DWC3_DCTL_LSFTRST	BIT(29)

#define DWC3_DCTL_HIRD_THRES_MASK	(0x1f << 24)
#define DWC3_DCTL_HIRD_THRES(n)	((n) << 24)

#define DWC3_DCTL_APPL1RES	BIT(23)

/* These apply for core versions 1.87a and earlier */
#define DWC3_DCTL_TRGTULST_MASK		(0x0f << 17)
#define DWC3_DCTL_TRGTULST(n)		((n) << 17)
#define DWC3_DCTL_TRGTULST_U2		(DWC3_DCTL_TRGTULST(2))
#define DWC3_DCTL_TRGTULST_U3		(DWC3_DCTL_TRGTULST(3))
#define DWC3_DCTL_TRGTULST_SS_DIS	(DWC3_DCTL_TRGTULST(4))
#define DWC3_DCTL_TRGTULST_RX_DET	(DWC3_DCTL_TRGTULST(5))
#define DWC3_DCTL_TRGTULST_SS_INACT	(DWC3_DCTL_TRGTULST(6))

/* These apply for core versions 1.94a and later */
#define DWC3_DCTL_NYET_THRES(n)		(((n) & 0xf) << 20)

#define DWC3_DCTL_KEEP_CONNECT		BIT(19)
#define DWC3_DCTL_L1_HIBER_EN		BIT(18)
#define DWC3_DCTL_CRS			BIT(17)
#define DWC3_DCTL_CSS			BIT(16)

#define DWC3_DCTL_INITU2ENA		BIT(12)
#define DWC3_DCTL_ACCEPTU2ENA		BIT(11)
#define DWC3_DCTL_INITU1ENA		BIT(10)
#define DWC3_DCTL_ACCEPTU1ENA		BIT(9)
#define DWC3_DCTL_TSTCTRL_MASK		(0xf << 1)

#define DWC3_DCTL_ULSTCHNGREQ_MASK	(0x0f << 5)
#define DWC3_DCTL_ULSTCHNGREQ(n) (((n) << 5) & DWC3_DCTL_ULSTCHNGREQ_MASK)

#define DWC3_DCTL_ULSTCHNG_NO_ACTION	(DWC3_DCTL_ULSTCHNGREQ(0))
#define DWC3_DCTL_ULSTCHNG_SS_DISABLED	(DWC3_DCTL_ULSTCHNGREQ(4))
#define DWC3_DCTL_ULSTCHNG_RX_DETECT	(DWC3_DCTL_ULSTCHNGREQ(5))
#define DWC3_DCTL_ULSTCHNG_SS_INACTIVE	(DWC3_DCTL_ULSTCHNGREQ(6))
#define DWC3_DCTL_ULSTCHNG_RECOVERY	(DWC3_DCTL_ULSTCHNGREQ(8))
#define DWC3_DCTL_ULSTCHNG_COMPLIANCE	(DWC3_DCTL_ULSTCHNGREQ(10))
#define DWC3_DCTL_ULSTCHNG_LOOPBACK	(DWC3_DCTL_ULSTCHNGREQ(11))

/* Device Event Enable Register */
#define DWC3_DEVTEN_VNDRDEVTSTRCVEDEN	BIT(12)
#define DWC3_DEVTEN_EVNTOVERFLOWEN	BIT(11)
#define DWC3_DEVTEN_CMDCMPLTEN		BIT(10)
#define DWC3_DEVTEN_ERRTICERREN		BIT(9)
#define DWC3_DEVTEN_SOFEN		BIT(7)
#define DWC3_DEVTEN_U3L2L1SUSPEN	BIT(6)
#define DWC3_DEVTEN_HIBERNATIONREQEVTEN	BIT(5)
#define DWC3_DEVTEN_WKUPEVTEN		BIT(4)
#define DWC3_DEVTEN_ULSTCNGEN		BIT(3)
#define DWC3_DEVTEN_CONNECTDONEEN	BIT(2)
#define DWC3_DEVTEN_USBRSTEN		BIT(1)
#define DWC3_DEVTEN_DISCONNEVTEN	BIT(0)

#define DWC3_DSTS_CONNLANES(n)		(((n) >> 30) & 0x3) /* DWC_usb32 only */

/* Device Status Register */
#define DWC3_DSTS_DCNRD			BIT(29)

/* This applies for core versions 1.87a and earlier */
#define DWC3_DSTS_PWRUPREQ		BIT(24)

/* These apply for core versions 1.94a and later */
#define DWC3_DSTS_RSS			BIT(25)
#define DWC3_DSTS_SSS			BIT(24)

#define DWC3_DSTS_COREIDLE		BIT(23)
#define DWC3_DSTS_DEVCTRLHLT		BIT(22)

#define DWC3_DSTS_USBLNKST_MASK		(0x0f << 18)
#define DWC3_DSTS_USBLNKST(n)		(((n) & DWC3_DSTS_USBLNKST_MASK) >> 18)

#define DWC3_DSTS_RXFIFOEMPTY		BIT(17)

#define DWC3_DSTS_SOFFN_MASK		(0x3fff << 3)
#define DWC3_DSTS_SOFFN(n)		(((n) & DWC3_DSTS_SOFFN_MASK) >> 3)

#define DWC3_DSTS_CONNECTSPD		(7 << 0)

#define DWC3_DSTS_SUPERSPEED_PLUS	(5 << 0) /* DWC_usb31 only */
#define DWC3_DSTS_SUPERSPEED		(4 << 0)
#define DWC3_DSTS_HIGHSPEED		(0 << 0)
#define DWC3_DSTS_FULLSPEED		BIT(0)

/* Device Generic Command Register */
#define DWC3_DGCMD_SET_LMP		0x01
#define DWC3_DGCMD_SET_PERIODIC_PAR	0x02
#define DWC3_DGCMD_XMIT_FUNCTION	0x03

/* These apply for core versions 1.94a and later */
#define DWC3_DGCMD_SET_SCRATCHPAD_ADDR_LO	0x04
#define DWC3_DGCMD_SET_SCRATCHPAD_ADDR_HI	0x05

#define DWC3_DGCMD_SELECTED_FIFO_FLUSH	0x09
#define DWC3_DGCMD_ALL_FIFO_FLUSH	0x0a
#define DWC3_DGCMD_SET_ENDPOINT_NRDY	0x0c
#define DWC3_DGCMD_SET_ENDPOINT_PRIME	0x0d
#define DWC3_DGCMD_RUN_SOC_BUS_LOOPBACK	0x10

#define DWC3_DGCMD_STATUS(n)		(((n) >> 12) & 0x0F)
#define DWC3_DGCMD_CMDACT		BIT(10)
#define DWC3_DGCMD_CMDIOC		BIT(8)

/* Device Generic Command Parameter Register */
#define DWC3_DGCMDPAR_FORCE_LINKPM_ACCEPT	BIT(0)
#define DWC3_DGCMDPAR_FIFO_NUM(n)		((n) << 0)
#define DWC3_DGCMDPAR_RX_FIFO			(0 << 5)
#define DWC3_DGCMDPAR_TX_FIFO			BIT(5)
#define DWC3_DGCMDPAR_LOOPBACK_DIS		(0 << 0)
#define DWC3_DGCMDPAR_LOOPBACK_ENA		BIT(0)

/* Device Endpoint Command Register */
#define DWC3_DEPCMD_PARAM_SHIFT		16
#define DWC3_DEPCMD_PARAM(x)		((x) << DWC3_DEPCMD_PARAM_SHIFT)
#define DWC3_DEPCMD_GET_RSC_IDX(x)	(((x) >> DWC3_DEPCMD_PARAM_SHIFT) & 0x7f)
#define DWC3_DEPCMD_STATUS(x)		(((x) >> 12) & 0x0F)
#define DWC3_DEPCMD_HIPRI_FORCERM	BIT(11)
#define DWC3_DEPCMD_CLEARPENDIN		BIT(11)
#define DWC3_DEPCMD_CMDACT		BIT(10)
#define DWC3_DEPCMD_CMDIOC		BIT(8)

#define DWC3_DEPCMD_DEPSTARTCFG		(0x09 << 0)
#define DWC3_DEPCMD_ENDTRANSFER		(0x08 << 0)
#define DWC3_DEPCMD_UPDATETRANSFER	(0x07 << 0)
#define DWC3_DEPCMD_STARTTRANSFER	(0x06 << 0)
#define DWC3_DEPCMD_CLEARSTALL		(0x05 << 0)
#define DWC3_DEPCMD_SETSTALL		(0x04 << 0)
/* This applies for core versions 1.90a and earlier */
#define DWC3_DEPCMD_GETSEQNUMBER	(0x03 << 0)
/* This applies for core versions 1.94a and later */
#define DWC3_DEPCMD_GETEPSTATE		(0x03 << 0)
#define DWC3_DEPCMD_SETTRANSFRESOURCE	(0x02 << 0)
#define DWC3_DEPCMD_SETEPCONFIG		(0x01 << 0)

#define DWC3_DEPCMD_CMD(x)		((x) & 0xf)

/* The EP number goes 0..31 so ep0 is always out and ep1 is always in */
#define DWC3_DALEPENA_EP(n)		BIT(n)

/* DWC_usb32 DCFG1 config */
#define DWC3_DCFG1_DIS_MST_ENH		BIT(1)

#define DWC3_DEPCMD_TYPE_CONTROL	0
#define DWC3_DEPCMD_TYPE_ISOC		1
#define DWC3_DEPCMD_TYPE_BULK		2
#define DWC3_DEPCMD_TYPE_INTR		3

#define DWC3_DEV_IMOD_COUNT_SHIFT	16
#define DWC3_DEV_IMOD_COUNT_MASK	(0xffff << 16)
#define DWC3_DEV_IMOD_INTERVAL_SHIFT	0
#define DWC3_DEV_IMOD_INTERVAL_MASK	(0xffff << 0)

/* OTG Configuration Register */
#define DWC3_OCFG_DISPWRCUTTOFF		BIT(5)
#define DWC3_OCFG_HIBDISMASK		BIT(4)
#define DWC3_OCFG_SFTRSTMASK		BIT(3)
#define DWC3_OCFG_OTGVERSION		BIT(2)
#define DWC3_OCFG_HNPCAP		BIT(1)
#define DWC3_OCFG_SRPCAP		BIT(0)

/* OTG CTL Register */
#define DWC3_OCTL_OTG3GOERR		BIT(7)
#define DWC3_OCTL_PERIMODE		BIT(6)
#define DWC3_OCTL_PRTPWRCTL		BIT(5)
#define DWC3_OCTL_HNPREQ		BIT(4)
#define DWC3_OCTL_SESREQ		BIT(3)
#define DWC3_OCTL_TERMSELIDPULSE	BIT(2)
#define DWC3_OCTL_DEVSETHNPEN		BIT(1)
#define DWC3_OCTL_HSTSETHNPEN		BIT(0)

/* OTG Event Register */
#define DWC3_OEVT_DEVICEMODE		BIT(31)
#define DWC3_OEVT_XHCIRUNSTPSET		BIT(27)
#define DWC3_OEVT_DEVRUNSTPSET		BIT(26)
#define DWC3_OEVT_HIBENTRY		BIT(25)
#define DWC3_OEVT_CONIDSTSCHNG		BIT(24)
#define DWC3_OEVT_HRRCONFNOTIF		BIT(23)
#define DWC3_OEVT_HRRINITNOTIF		BIT(22)
#define DWC3_OEVT_ADEVIDLE		BIT(21)
#define DWC3_OEVT_ADEVBHOSTEND		BIT(20)
#define DWC3_OEVT_ADEVHOST		BIT(19)
#define DWC3_OEVT_ADEVHNPCHNG		BIT(18)
#define DWC3_OEVT_ADEVSRPDET		BIT(17)
#define DWC3_OEVT_ADEVSESSENDDET	BIT(16)
#define DWC3_OEVT_BDEVBHOSTEND		BIT(11)
#define DWC3_OEVT_BDEVHNPCHNG		BIT(10)
#define DWC3_OEVT_BDEVSESSVLDDET	BIT(9)
#define DWC3_OEVT_BDEVVBUSCHNG		BIT(8)
#define DWC3_OEVT_BSESSVLD		BIT(3)
#define DWC3_OEVT_HSTNEGSTS		BIT(2)
#define DWC3_OEVT_SESREQSTS		BIT(1)
#define DWC3_OEVT_ERROR			BIT(0)

/* OTG Event Enable Register */
#define DWC3_OEVTEN_XHCIRUNSTPSETEN	BIT(27)
#define DWC3_OEVTEN_DEVRUNSTPSETEN	BIT(26)
#define DWC3_OEVTEN_HIBENTRYEN		BIT(25)
#define DWC3_OEVTEN_CONIDSTSCHNGEN	BIT(24)
#define DWC3_OEVTEN_HRRCONFNOTIFEN	BIT(23)
#define DWC3_OEVTEN_HRRINITNOTIFEN	BIT(22)
#define DWC3_OEVTEN_ADEVIDLEEN		BIT(21)
#define DWC3_OEVTEN_ADEVBHOSTENDEN	BIT(20)
#define DWC3_OEVTEN_ADEVHOSTEN		BIT(19)
#define DWC3_OEVTEN_ADEVHNPCHNGEN	BIT(18)
#define DWC3_OEVTEN_ADEVSRPDETEN	BIT(17)
#define DWC3_OEVTEN_ADEVSESSENDDETEN	BIT(16)
#define DWC3_OEVTEN_BDEVBHOSTENDEN	BIT(11)
#define DWC3_OEVTEN_BDEVHNPCHNGEN	BIT(10)
#define DWC3_OEVTEN_BDEVSESSVLDDETEN	BIT(9)
#define DWC3_OEVTEN_BDEVVBUSCHNGEN	BIT(8)

/* OTG Status Register */
#define DWC3_OSTS_DEVRUNSTP		BIT(13)
#define DWC3_OSTS_XHCIRUNSTP		BIT(12)
#define DWC3_OSTS_PERIPHERALSTATE	BIT(4)
#define DWC3_OSTS_XHCIPRTPOWER		BIT(3)
#define DWC3_OSTS_BSESVLD		BIT(2)
#define DWC3_OSTS_VBUSVLD		BIT(1)
#define DWC3_OSTS_CONIDSTS		BIT(0)


#define DWC3_IP_IS(_ip)							\
	(dwc->ip == _ip##_IP)

#define DWC3_VER_IS(_ip, _ver)						\
	(DWC3_IP_IS(_ip) && dwc->revision == _ip##_REVISION_##_ver)

#define DWC3_VER_IS_PRIOR(_ip, _ver)					\
	(DWC3_IP_IS(_ip) && dwc->revision < _ip##_REVISION_##_ver)

#define DWC3_VER_IS_WITHIN(_ip, _from, _to)				\
	(DWC3_IP_IS(_ip) &&						\
	 dwc->revision >= _ip##_REVISION_##_from &&			\
	 (!(_ip##_REVISION_##_to) ||					\
	  dwc->revision <= _ip##_REVISION_##_to))

#define DWC3_VER_TYPE_IS_WITHIN(_ip, _ver, _from, _to)			\
	(DWC3_VER_IS(_ip, _ver) &&					\
	 dwc->version_type >= _ip##_VERSIONTYPE_##_from &&		\
	 (!(_ip##_VERSIONTYPE_##_to) ||					\
	  dwc->version_type <= _ip##_VERSIONTYPE_##_to))

enum usb_dr_mode {
	USB_DR_MODE_UNKNOWN,
	USB_DR_MODE_HOST,
	USB_DR_MODE_PERIPHERAL,
	USB_DR_MODE_OTG,
};

/* USB 2.0 defines three speeds, here's how Linux identifies them */

enum usb_device_speed {
	USB_SPEED_UNKNOWN = 0,			/* enumerating */
	USB_SPEED_LOW, USB_SPEED_FULL,		/* usb 1.1 */
	USB_SPEED_HIGH,				/* usb 2.0 */
	USB_SPEED_WIRELESS,			/* wireless (usb 2.5) */
	USB_SPEED_SUPER,			/* usb 3.0 */
    USB_SPEED_SUPER_PLUS,			/* usb 3.1 */
};

struct dwc3_hwparams {
	u32	hwparams0;
	u32	hwparams1;
	u32	hwparams2;
	u32	hwparams3;
	u32	hwparams4;
	u32	hwparams5;
	u32	hwparams6;
	u32	hwparams7;
	u32	hwparams8;
	u32	hwparams9;
};

typedef struct 
{
    void *base;
    clk_t *clk;  // host clock
    rst_t *rst;  // Host reset
    void *regs;     // Global registers

	usb_phy_t		*usb2_phy;
	usb_phy_t		*usb3_phy;
    uint32_t    irq;

    struct dwc3_hwparams hwparams;
    enum usb_dr_mode	dr_mode;
    uint32_t			current_dr_role;
    
    uint32_t			fladj;
    uint32_t			maximum_speed;

    uint32_t			ip;

#define DWC3_IP			0x5533
#define DWC31_IP		0x3331
#define DWC32_IP		0x3332

	uint32_t			revision;

#define DWC3_REVISION_ANY	0x0
#define DWC3_REVISION_173A	0x5533173a
#define DWC3_REVISION_175A	0x5533175a
#define DWC3_REVISION_180A	0x5533180a
#define DWC3_REVISION_183A	0x5533183a
#define DWC3_REVISION_185A	0x5533185a
#define DWC3_REVISION_187A	0x5533187a
#define DWC3_REVISION_188A	0x5533188a
#define DWC3_REVISION_190A	0x5533190a
#define DWC3_REVISION_194A	0x5533194a
#define DWC3_REVISION_200A	0x5533200a
#define DWC3_REVISION_202A	0x5533202a
#define DWC3_REVISION_210A	0x5533210a
#define DWC3_REVISION_220A	0x5533220a
#define DWC3_REVISION_230A	0x5533230a
#define DWC3_REVISION_240A	0x5533240a
#define DWC3_REVISION_250A	0x5533250a
#define DWC3_REVISION_260A	0x5533260a
#define DWC3_REVISION_270A	0x5533270a
#define DWC3_REVISION_280A	0x5533280a
#define DWC3_REVISION_290A	0x5533290a
#define DWC3_REVISION_300A	0x5533300a
#define DWC3_REVISION_310A	0x5533310a
#define DWC3_REVISION_330A	0x5533330a

#define DWC31_REVISION_ANY	0x0
#define DWC31_REVISION_110A	0x3131302a
#define DWC31_REVISION_120A	0x3132302a
#define DWC31_REVISION_160A	0x3136302a
#define DWC31_REVISION_170A	0x3137302a
#define DWC31_REVISION_180A	0x3138302a
#define DWC31_REVISION_190A	0x3139302a

#define DWC32_REVISION_ANY	0x0
#define DWC32_REVISION_100A	0x3130302a

	uint32_t			version_type;

#define DWC31_VERSIONTYPE_ANY		0x0
#define DWC31_VERSIONTYPE_EA01		0x65613031
#define DWC31_VERSIONTYPE_EA02		0x65613032
#define DWC31_VERSIONTYPE_EA03		0x65613033
#define DWC31_VERSIONTYPE_EA04		0x65613034
#define DWC31_VERSIONTYPE_EA05		0x65613035
#define DWC31_VERSIONTYPE_EA06		0x65613036

	unsigned		connected:1;
	unsigned		softconnect:1;
	unsigned		delayed_status:1;
	unsigned		ep0_bounced:1;
	unsigned		ep0_expect_in:1;
	unsigned		has_hibernation:1;
	unsigned		sysdev_is_parent:1;
	unsigned		has_lpm_erratum:1;
	unsigned		is_utmi_l1_suspend:1;
	unsigned		is_fpga:1;
	unsigned		pending_events:1;
	unsigned		do_fifo_resize:1;
	unsigned		pullups_connected:1;
	unsigned		setup_packet_pending:1;
	unsigned		three_stage_setup:1;
	unsigned		dis_start_transfer_quirk:1;
	unsigned		usb3_lpm_capable:1;
	unsigned		usb2_lpm_disable:1;
	unsigned		usb2_gadget_lpm_disable:1;

	unsigned		disable_scramble_quirk:1;
	unsigned		u2exit_lfps_quirk:1;
	unsigned		u2ss_inp3_quirk:1;
	unsigned		req_p1p2p3_quirk:1;
	unsigned                del_p1p2p3_quirk:1;
	unsigned		del_phy_power_chg_quirk:1;
	unsigned		lfps_filter_quirk:1;
	unsigned		rx_detect_poll_quirk:1;
	unsigned		dis_u3_susphy_quirk:1;
	unsigned		dis_u2_susphy_quirk:1;
	unsigned		dis_enblslpm_quirk:1;
	unsigned		dis_u1_entry_quirk:1;
	unsigned		dis_u2_entry_quirk:1;
	unsigned		dis_rxdet_inp3_quirk:1;
	unsigned		dis_u2_freeclk_exists_quirk:1;
	unsigned		dis_del_phy_power_chg_quirk:1;
	unsigned		dis_tx_ipgap_linecheck_quirk:1;
	unsigned		resume_hs_terminations:1;
	unsigned		parkmode_disable_ss_quirk:1;
	unsigned		gfladj_refclk_lpm_sel:1;

	unsigned		tx_de_emphasis_quirk:1;
	unsigned		tx_de_emphasis:2;

	unsigned		dis_metastability_quirk:1;

	unsigned		dis_split_quirk:1;
	unsigned		async_callbacks:1;

    uint16_t		imod_interval;

} dwc3_dev_t;

static inline u32 dwc3_readl(void *base, uint32_t offset)
{
	uint32_t value;
//    debug("[USB2] read at 0x%p 0x%p+0x%p\n", (uint64_t)base + offset, base, offset);
	value = getreg32((uint64_t)base + offset);

	return value;
}

static inline void dwc3_writel(void *base, uint32_t offset, uint32_t value)
{
	putreg32(value, (uint64_t)base + offset);
}

int hub_enable();

#endif /* __DWC3_H__ */