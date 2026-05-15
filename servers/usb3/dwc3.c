#include <mosstd.h>
#include <riscv.h>
//#include <libsys/memory.h>
#include <string.h>
#include <libsys/cap.h>
#include <signals.h>
#include <libsys/timer.h>

#include "dwc3.h"

#include "xhci.h"

#define DWC3_BASE 0xC0A00000ULL
#define COMBPHY_BASE 0xC0B10000ULL

dwc3_dev_t k1x;
usb_phy_t  usb2_phy;
usb_phy_t  usb3_phy;

static void dwc3_cache_hwparams(dwc3_dev_t *dwc)
{
	struct dwc3_hwparams	*parms = &dwc->hwparams;

	parms->hwparams0 = dwc3_readl(dwc->base, DWC3_GHWPARAMS0);
	parms->hwparams1 = dwc3_readl(dwc->base, DWC3_GHWPARAMS1);
	parms->hwparams2 = dwc3_readl(dwc->base, DWC3_GHWPARAMS2);
	parms->hwparams3 = dwc3_readl(dwc->base, DWC3_GHWPARAMS3);
	parms->hwparams4 = dwc3_readl(dwc->base, DWC3_GHWPARAMS4);
	parms->hwparams5 = dwc3_readl(dwc->base, DWC3_GHWPARAMS5);
	parms->hwparams6 = dwc3_readl(dwc->base, DWC3_GHWPARAMS6);
	parms->hwparams7 = dwc3_readl(dwc->base, DWC3_GHWPARAMS7);
	parms->hwparams8 = dwc3_readl(dwc->base, DWC3_GHWPARAMS8);

	if (DWC3_IP_IS(DWC32))
		parms->hwparams9 = dwc3_readl(dwc->base, DWC3_GHWPARAMS9);
}

static int dwc3_get_dr_mode(dwc3_dev_t *dwc)
{
	enum usb_dr_mode mode;
	// struct device *dev = dwc->dev;
	unsigned int hw_mode;

	if (dwc->dr_mode == USB_DR_MODE_UNKNOWN)
	 	dwc->dr_mode = USB_DR_MODE_OTG;

	// mode = dwc->dr_mode;
	hw_mode = DWC3_GHWPARAMS0_MODE(dwc->hwparams.hwparams0);
debug("[USB3] HW MODE 0x%X\n", hw_mode);
	switch (hw_mode) {
	case DWC3_GHWPARAMS0_MODE_GADGET:
		// if (IS_ENABLED(CONFIG_USB_DWC3_HOST)) {
		// 	dev_err(dev,
		// 		"Controller does not support host mode.\n");
		// 	return -EINVAL;
		// }
		mode = USB_DR_MODE_PERIPHERAL;
		break;
	case DWC3_GHWPARAMS0_MODE_HOST:
		// if (IS_ENABLED(CONFIG_USB_DWC3_GADGET)) {
		// 	dev_err(dev,
		// 		"Controller does not support device mode.\n");
		// 	return -EINVAL;
		// }
		mode = USB_DR_MODE_HOST;
		break;
	default:
		// if (IS_ENABLED(CONFIG_USB_DWC3_HOST))
		// 	mode = USB_DR_MODE_HOST;
		// else if (IS_ENABLED(CONFIG_USB_DWC3_GADGET))
		// 	mode = USB_DR_MODE_PERIPHERAL;

		/*
		 * DWC_usb31 and DWC_usb3 v3.30a and higher do not support OTG
		 * mode. If the controller supports DRD but the dr_mode is not
		 * specified or set to OTG, then set the mode to peripheral.
		 */
		// if (mode == USB_DR_MODE_OTG && !dwc->edev &&
		//     (!IS_ENABLED(CONFIG_USB_ROLE_SWITCH) ||
		//      !device_property_read_bool(dwc->dev, "usb-role-switch")) &&
		//     !DWC3_VER_IS_PRIOR(DWC3, 330A))
		// 	mode = USB_DR_MODE_PERIPHERAL;
        mode = USB_DR_MODE_HOST;
	}

	if (mode != dwc->dr_mode) {
		debug("[USB2] Configuration mismatch. dr_mode forced to %s\n",
			 mode == USB_DR_MODE_HOST ? "host" : "gadget");

		dwc->dr_mode = mode;
	}

	return 0;
}

void dwc3_enable_susphy(dwc3_dev_t *dwc, bool enable)
{
	uint32_t reg;
	int i;

	for (i = 0; i < 1/*dwc->num_usb3_ports*/; i++) {
		reg = dwc3_readl(dwc->base, DWC3_GUSB3PIPECTL(i));
		if (enable && !dwc->dis_u3_susphy_quirk)
			reg |= DWC3_GUSB3PIPECTL_SUSPHY;
		else 
			reg &= ~DWC3_GUSB3PIPECTL_SUSPHY;

		dwc3_writel(dwc->base, DWC3_GUSB3PIPECTL(i), reg);
	}

	for (i = 0; i < 1/*dwc->num_usb2_ports*/; i++) {
		reg = dwc3_readl(dwc->base, DWC3_GUSB2PHYCFG(i));
		if (enable && !dwc->dis_u2_susphy_quirk)
			reg |= DWC3_GUSB2PHYCFG_SUSPHY;
		else
			reg &= ~DWC3_GUSB2PHYCFG_SUSPHY;

		dwc3_writel(dwc->base, DWC3_GUSB2PHYCFG(i), reg);
	}
}

void dwc3_set_prtcap(dwc3_dev_t *dwc, uint32_t mode)
{
	uint32_t reg;
	unsigned int hw_mode;

	reg = dwc3_readl(dwc->base, DWC3_GCTL);

	 /*
	  * For DRD controllers, GUSB3PIPECTL.SUSPENDENABLE and
	  * GUSB2PHYCFG.SUSPHY should be cleared during mode switching,
	  * and they can be set after core initialization.
	  */
	hw_mode = DWC3_GHWPARAMS0_MODE(dwc->hwparams.hwparams0);
	if (hw_mode == DWC3_GHWPARAMS0_MODE_DRD) {
		if (DWC3_GCTL_PRTCAP(reg) != mode)
			dwc3_enable_susphy(dwc, false);
	}	

	reg &= ~(DWC3_GCTL_PRTCAPDIR(DWC3_GCTL_PRTCAP_OTG));
	reg |= DWC3_GCTL_PRTCAPDIR(mode);
	dwc3_writel(dwc->base, DWC3_GCTL, reg);

	dwc->current_dr_role = mode;
}


static void dwc3_frame_length_adjustment(dwc3_dev_t *dwc)
{
	uint32_t reg;
	uint32_t dft;

	if (DWC3_VER_IS_PRIOR(DWC3, 250A))
		return;

	if (dwc->fladj == 0)
		return;

	reg = dwc3_readl(dwc->base, DWC3_GFLADJ);
	dft = reg & DWC3_GFLADJ_30MHZ_MASK;
	if (dft != dwc->fladj) {
		reg &= ~DWC3_GFLADJ_30MHZ_MASK;
		reg |= DWC3_GFLADJ_30MHZ_SDBND_SEL | dwc->fladj;
		dwc3_writel(dwc->base, DWC3_GFLADJ, reg);
	}
}

// static void dwc3_ref_clk_period(struct dwc3 *dwc)
// {
// 	unsigned long period;
// 	unsigned long fladj;
// 	unsigned long decr;
// 	unsigned long rate;
// 	u32 reg;

// 	if (dwc->ref_clk) {
// 		rate = clk_get_rate(dwc->ref_clk);
// 		if (!rate)
// 			return;
// 		period = NSEC_PER_SEC / rate;
// 	} else if (dwc->ref_clk_per) {
// 		period = dwc->ref_clk_per;
// 		rate = NSEC_PER_SEC / period;
// 	} else {
// 		return;
// 	}

// 	reg = dwc3_readl(dwc->regs, DWC3_GUCTL);
// 	reg &= ~DWC3_GUCTL_REFCLKPER_MASK;
// 	reg |=  FIELD_PREP(DWC3_GUCTL_REFCLKPER_MASK, period);
// 	dwc3_writel(dwc->regs, DWC3_GUCTL, reg);

// 	if (DWC3_VER_IS_PRIOR(DWC3, 250A))
// 		return;

// 	/*
// 	 * The calculation below is
// 	 *
// 	 * 125000 * (NSEC_PER_SEC / (rate * period) - 1)
// 	 *
// 	 * but rearranged for fixed-point arithmetic. The division must be
// 	 * 64-bit because 125000 * NSEC_PER_SEC doesn't fit in 32 bits (and
// 	 * neither does rate * period).
// 	 *
// 	 * Note that rate * period ~= NSEC_PER_SECOND, minus the number of
// 	 * nanoseconds of error caused by the truncation which happened during
// 	 * the division when calculating rate or period (whichever one was
// 	 * derived from the other). We first calculate the relative error, then
// 	 * scale it to units of 8 ppm.
// 	 */
// 	fladj = div64_u64(125000ULL * NSEC_PER_SEC, (u64)rate * period);
// 	fladj -= 125000;

// 	/*
// 	 * The documented 240MHz constant is scaled by 2 to get PLS1 as well.
// 	 */
// 	decr = 480000000 / rate;

// 	reg = dwc3_readl(dwc->regs, DWC3_GFLADJ);
// 	reg &= ~DWC3_GFLADJ_REFCLK_FLADJ_MASK
// 	    &  ~DWC3_GFLADJ_240MHZDECR
// 	    &  ~DWC3_GFLADJ_240MHZDECR_PLS1;
// 	reg |= FIELD_PREP(DWC3_GFLADJ_REFCLK_FLADJ_MASK, fladj)
// 	    |  FIELD_PREP(DWC3_GFLADJ_240MHZDECR, decr >> 1)
// 	    |  FIELD_PREP(DWC3_GFLADJ_240MHZDECR_PLS1, decr & 1);

// 	if (dwc->gfladj_refclk_lpm_sel)
// 		reg |=  DWC3_GFLADJ_REFCLK_LPM_SEL;

// 	dwc3_writel(dwc->regs, DWC3_GFLADJ, reg);
// }

static bool dwc3_core_is_valid(dwc3_dev_t *dwc)
{
	uint32_t reg;

	reg = dwc3_readl(dwc->base, DWC3_GSNPSID);

	dwc->ip = DWC3_GSNPS_ID(reg);
    debug("[USB2] ip revision 0x%X\n", reg);

	/* This should read as U3 followed by revision number */
	if (DWC3_IP_IS(DWC3)) {
		dwc->revision = reg;
	} else if (DWC3_IP_IS(DWC31) || DWC3_IP_IS(DWC32)) {
		dwc->revision = dwc3_readl(dwc->base, DWC3_VER_NUMBER);
		dwc->version_type = dwc3_readl(dwc->base, DWC3_VER_TYPE);
	} else {
	  	return false;
	}

	return true;
}

int dwc3_core_soft_reset(dwc3_dev_t *dwc)
{
	u32		reg;
	int		retries = 1000;

	/*
	 * We're resetting only the device side because, if we're in host mode,
	 * XHCI driver will reset the host block. If dwc3 was configured for
	 * host-only mode, then we can return early.
	 */
	 if (dwc->current_dr_role == DWC3_GCTL_PRTCAP_HOST)
	 	return 0;

	reg = dwc3_readl(dwc->base, DWC3_DCTL);
	reg |= DWC3_DCTL_CSFTRST;
	reg &= ~DWC3_DCTL_RUN_STOP;

    /**
 * dwc3_gadget_dctl_write_safe - write to DCTL safe from link state change
 * @dwc: pointer to our context structure
 * @value: value to write to DCTL
 *
 * Use this function when doing read-modify-write to DCTL. It will not
 * send link state change request.
 */
    reg &= ~DWC3_DCTL_ULSTCHNGREQ_MASK;
	dwc3_writel(dwc->base, DWC3_DCTL, reg);

	/*
	 * For DWC_usb31 controller 1.90a and later, the DCTL.CSFRST bit
	 * is cleared only after all the clocks are synchronized. This can
	 * take a little more than 50ms. Set the polling rate at 20ms
	 * for 10 times instead.
	 */
	if (DWC3_VER_IS_WITHIN(DWC31, 190A, ANY) || DWC3_IP_IS(DWC32))
		retries = 10;	

	do {
		reg = dwc3_readl(dwc->base, DWC3_DCTL);
		if (!(reg & DWC3_DCTL_CSFTRST))
			goto done;

		if (DWC3_VER_IS_WITHIN(DWC31, 190A, ANY) || DWC3_IP_IS(DWC32))
			udelay(20000);
		else
		udelay(1);
	} while (--retries);

	debug("[USB3] DWC3 controller soft reset failed.\n");
	return -ETIMEOUT;

done:
	/*
	 * For DWC_usb31 controller 1.80a and prior, once DCTL.CSFRST bit
	 * is cleared, we must wait at least 50ms before accessing the PHY
	 * domain (synchronization delay).
	 */
	if (DWC3_VER_IS_WITHIN(DWC31, ANY, 180A))
		udelay(50000);
	return 0;
}

static void dwc3_core_setup_global_control(dwc3_dev_t *dwc)
{
	uint32_t hwparams4 = dwc->hwparams.hwparams4;
	unsigned int power_opt;
	unsigned int hw_mode;
	uint32_t reg;

	reg = dwc3_readl(dwc->base, DWC3_GCTL);
	reg &= ~DWC3_GCTL_SCALEDOWN_MASK;

	hw_mode = DWC3_GHWPARAMS0_MODE(dwc->hwparams.hwparams0);
	power_opt = DWC3_GHWPARAMS1_EN_PWROPT(dwc->hwparams.hwparams1);

	switch (power_opt) {
	case DWC3_GHWPARAMS1_EN_PWROPT_CLK:
		/**
		 * WORKAROUND: DWC3 revisions between 2.10a and 2.50a have an
		 * issue which would cause xHCI compliance tests to fail.
		 *
		 * Because of that we cannot enable clock gating on such
		 * configurations.
		 *
		 * Refers to:
		 *
		 * STAR#9000588375: Clock Gating, SOF Issues when ref_clk-Based
		 * SOF/ITP Mode Used
		 */
		if ((dwc->dr_mode == USB_DR_MODE_HOST ||
				dwc->dr_mode == USB_DR_MODE_OTG) &&
				DWC3_VER_IS_WITHIN(DWC3, 210A, 250A))
			reg |= DWC3_GCTL_DSBLCLKGTNG | DWC3_GCTL_SOFITPSYNC;
		else
			reg &= ~DWC3_GCTL_DSBLCLKGTNG;
		break;
	case DWC3_GHWPARAMS1_EN_PWROPT_HIB:
		/* enable hibernation here */
//        dwc->nr_scratch = DWC3_GHWPARAMS4_HIBER_SCRATCHBUFS(hwparams4);

		/*
		 * REVISIT Enabling this bit so that host-mode hibernation
		 * will work. Device-mode hibernation is not yet implemented.
		 */
		reg |= DWC3_GCTL_GBLHIBERNATIONEN;
		break;
	default:
		/* nothing */
		break;
	}


	/* check if current dwc3 is on simulation board */
	if (dwc->hwparams.hwparams6 & DWC3_GHWPARAMS6_EN_FPGA) {
		debug("[USB3] Running with FPGA optimizations\n");
//		dwc->is_fpga = true;
	}

	if (dwc->disable_scramble_quirk && dwc->is_fpga)
		reg |= DWC3_GCTL_DISSCRAMBLE;
	else
		reg &= ~DWC3_GCTL_DISSCRAMBLE;

	if (dwc->u2exit_lfps_quirk)
		reg |= DWC3_GCTL_U2EXIT_LFPS;

	/*
	 * WORKAROUND: DWC3 revisions <1.90a have a bug
	 * where the device can fail to connect at SuperSpeed
	 * and falls back to high-speed mode which causes
	 * the device to enter a Connect/Disconnect loop
	 */
	if (DWC3_VER_IS_PRIOR(DWC3, 190A))
		reg |= DWC3_GCTL_U2RSTECN;

	dwc3_writel(dwc->base, DWC3_GCTL, reg);
}

static int dwc3_phy_setup(dwc3_dev_t *dwc)
{
	uint32_t hw_mode;
	uint32_t reg;

	// soft reset the PHYs
	reg = dwc3_readl(dwc->base, DWC3_GUSB3PIPECTL(0));
	reg |= DWC3_GUSB3PIPECTL_PHYSOFTRST;
	dwc3_writel(dwc->base, DWC3_GUSB3PIPECTL(0), reg);
	udelay(100);
	reg &= ~DWC3_GUSB3PIPECTL_PHYSOFTRST;
	dwc3_writel(dwc->base, DWC3_GUSB3PIPECTL(0), reg);

	while (dwc3_readl(dwc->base, DWC3_GUSB3PIPECTL(0)) &
	       DWC3_GUSB3PIPECTL_PHYSOFTRST)
	{
		udelay(10);
	}
	
	hw_mode = DWC3_GHWPARAMS0_MODE(dwc->hwparams.hwparams0);

	reg = dwc3_readl(dwc->base, DWC3_GUSB3PIPECTL(0));

	/*
	 * Make sure UX_EXIT_PX is cleared as that causes issues with some
	 * PHYs. Also, this bit is not supposed to be used in normal operation.
	 */
	reg &= ~DWC3_GUSB3PIPECTL_UX_EXIT_PX;

	/*
	 * For DRD controllers, GUSB3PIPECTL.SUSPENDENABLE must be cleared after
	 * power-on reset, and it can be set after core initialization, which is
	 * after device soft-reset during initialization.
	 */

	if (hw_mode == DWC3_GHWPARAMS0_MODE_DRD)
		reg &= ~DWC3_GUSB3PIPECTL_SUSPHY;
    
	if (dwc->u2ss_inp3_quirk)
		reg |= DWC3_GUSB3PIPECTL_U2SSINP3OK;

	if (dwc->dis_rxdet_inp3_quirk)
		reg |= DWC3_GUSB3PIPECTL_DISRXDETINP3;

	if (dwc->req_p1p2p3_quirk)
		reg |= DWC3_GUSB3PIPECTL_REQP1P2P3;

	if (dwc->del_p1p2p3_quirk)
		reg |= DWC3_GUSB3PIPECTL_DEP1P2P3_EN;

	if (dwc->del_phy_power_chg_quirk)
		reg |= DWC3_GUSB3PIPECTL_DEPOCHANGE;

	if (dwc->lfps_filter_quirk)
		reg |= DWC3_GUSB3PIPECTL_LFPSFILT;

	if (dwc->rx_detect_poll_quirk)
		reg |= DWC3_GUSB3PIPECTL_RX_DETOPOLL;

	if (dwc->tx_de_emphasis_quirk)
		reg |= DWC3_GUSB3PIPECTL_TX_DEEPH(dwc->tx_de_emphasis);

	if (dwc->dis_del_phy_power_chg_quirk)
		reg &= ~DWC3_GUSB3PIPECTL_DEPOCHANGE;  

    dwc3_writel(dwc->base, DWC3_GUSB3PIPECTL(0), reg);

 	reg = dwc3_readl(dwc->base, DWC3_GUSB2PHYCFG(0));

    /* Select the HS PHY interface */
	// switch (DWC3_GHWPARAMS3_HSPHY_IFC(dwc->hwparams.hwparams3)) {
	// case DWC3_GHWPARAMS3_HSPHY_IFC_UTMI_ULPI:
    // debug("[USB2] HS PHY UTMI ULPI\n");
	// 	if (dwc->hsphy_interface &&
	// 			!strncmp(dwc->hsphy_interface, "utmi", 4)) {
	//
// 			break;
// 		} else if (dwc->hsphy_interface &&
// 				!strncmp(dwc->hsphy_interface, "ulpi", 4)) {
// 			reg |= DWC3_GUSB2PHYCFG_ULPI_UTMI;
// 			dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);
// 		} else {
// 			/* Relying on default value. */
// 			if (!(reg & DWC3_GUSB2PHYCFG_ULPI_UTMI))
// 				break;
// 		}
// //		fallthrough;
// 	case DWC3_GHWPARAMS3_HSPHY_IFC_ULPI:
// 	default:
// 		break;
// 	}
// 	switch (dwc->hsphy_mode) {
// 	case USBPHY_INTERFACE_MODE_UTMI:
		// reg &= ~(DWC3_GUSB2PHYCFG_PHYIF_MASK |
		//        DWC3_GUSB2PHYCFG_USBTRDTIM_MASK);
		// reg |= DWC3_GUSB2PHYCFG_PHYIF(UTMI_PHYIF_8_BIT) |
		//        DWC3_GUSB2PHYCFG_USBTRDTIM(USBTRDTIM_UTMI_8_BIT);
		// break;
	// case USBPHY_INTERFACE_MODE_UTMIW:
	// 	reg &= ~(DWC3_GUSB2PHYCFG_PHYIF_MASK |
	// 	       DWC3_GUSB2PHYCFG_USBTRDTIM_MASK);
	// 	reg |= DWC3_GUSB2PHYCFG_PHYIF(UTMI_PHYIF_16_BIT) |
	// 	       DWC3_GUSB2PHYCFG_USBTRDTIM(USBTRDTIM_UTMI_16_BIT);
	// 	break;
	// default:
	// 	break;
	// }

	if (hw_mode == DWC3_GHWPARAMS0_MODE_DRD)
		reg &= ~DWC3_GUSB2PHYCFG_SUSPHY;

	if (dwc->dis_enblslpm_quirk)
		reg &= ~DWC3_GUSB2PHYCFG_ENBLSLPM;
	else
		reg |= DWC3_GUSB2PHYCFG_ENBLSLPM;

	if (dwc->dis_u2_freeclk_exists_quirk || dwc->gfladj_refclk_lpm_sel)
		reg &= ~DWC3_GUSB2PHYCFG_U2_FREECLK_EXISTS;
    
    dwc3_writel(dwc->base, DWC3_GUSB2PHYCFG(0), reg);

    return SUCCESS;
}

/* set global incr burst type configuration registers */
// static void dwc3_set_incr_burst_type(dwc3_dev_t *dwc)
// {
// 	struct device *dev = dwc->dev;
// 	/* incrx_mode : for INCR burst type. */
// 	bool incrx_mode;
// 	/* incrx_size : for size of INCRX burst. */
// 	u32 incrx_size;
// 	u32 *vals;
// 	u32 cfg;
// 	int ntype;
// 	int ret;
// 	int i;

// 	cfg = dwc3_readl(dwc->base, DWC3_GSBUSCFG0);

// 	/*
// 	 * Handle property "snps,incr-burst-type-adjustment".
// 	 * Get the number of value from this property:
// 	 * result <= 0, means this property is not supported.
// 	 * result = 1, means INCRx burst mode supported.
// 	 * result > 1, means undefined length burst mode supported.
// 	 */
// 	ntype = device_property_count_u32(dev, "snps,incr-burst-type-adjustment");
// 	if (ntype <= 0)
// 		return;

// 	vals = kcalloc(ntype, sizeof(u32), GFP_KERNEL);
// 	if (!vals)
// 		return;

// 	/* Get INCR burst type, and parse it */
// 	ret = device_property_read_u32_array(dev,
// 			"snps,incr-burst-type-adjustment", vals, ntype);
// 	if (ret) {
// 		kfree(vals);
// 		dev_err(dev, "Error to get property\n");
// 		return;
// 	}

// 	incrx_size = *vals;

// 	if (ntype > 1) {
// 		/* INCRX (undefined length) burst mode */
// 		incrx_mode = INCRX_UNDEF_LENGTH_BURST_MODE;
// 		for (i = 1; i < ntype; i++) {
// 			if (vals[i] > incrx_size)
// 				incrx_size = vals[i];
// 		}
// 	} else {
// 		/* INCRX burst mode */
// 		incrx_mode = INCRX_BURST_MODE;
// 	}

// 	kfree(vals);

// 	/* Enable Undefined Length INCR Burst and Enable INCRx Burst */
// 	cfg &= ~DWC3_GSBUSCFG0_INCRBRST_MASK;
// 	if (incrx_mode)
// 		cfg |= DWC3_GSBUSCFG0_INCRBRSTENA;
// 	switch (incrx_size) {
// 	case 256:
// 		cfg |= DWC3_GSBUSCFG0_INCR256BRSTENA;
// 		break;
// 	case 128:
// 		cfg |= DWC3_GSBUSCFG0_INCR128BRSTENA;
// 		break;
// 	case 64:
// 		cfg |= DWC3_GSBUSCFG0_INCR64BRSTENA;
// 		break;
// 	case 32:
// 		cfg |= DWC3_GSBUSCFG0_INCR32BRSTENA;
// 		break;
// 	case 16:
// 		cfg |= DWC3_GSBUSCFG0_INCR16BRSTENA;
// 		break;
// 	case 8:
// 		cfg |= DWC3_GSBUSCFG0_INCR8BRSTENA;
// 		break;
// 	case 4:
// 		cfg |= DWC3_GSBUSCFG0_INCR4BRSTENA;
// 		break;
// 	case 1:
// 		break;
// 	default:
// 		dev_err(dev, "Invalid property\n");
// 		break;
// 	}

// 	dwc3_writel(dwc->regs, DWC3_GSBUSCFG0, cfg);
// }

static int dwc3_core_init(dwc3_dev_t *dwc)
{
    uint32_t	hw_mode;
	uint32_t	reg;
	int			ret;

	hw_mode = DWC3_GHWPARAMS0_MODE(dwc->hwparams.hwparams0);

    ret = dwc3_phy_setup(dwc);
	if (ret)
		goto err0;

	// if (!dwc->phys_ready) {
	// 	ret = dwc3_core_get_phy(dwc);
	// 	if (ret)
	// 		goto err0a;
	// 	dwc->phys_ready = true;
	// }

	usb_phy_init(dwc->usb2_phy);
	combphy_init(dwc->usb3_phy);

	ret = dwc3_core_soft_reset(dwc);
	if (ret)
		goto err1;

	if (hw_mode == DWC3_GHWPARAMS0_MODE_DRD &&
	    !DWC3_VER_IS_WITHIN(DWC3, ANY, 194A)) {
		if (!dwc->dis_u3_susphy_quirk) {
			reg = dwc3_readl(dwc->base, DWC3_GUSB3PIPECTL(0));
			reg |= DWC3_GUSB3PIPECTL_SUSPHY;
			dwc3_writel(dwc->base, DWC3_GUSB3PIPECTL(0), reg);
		}

		if (!dwc->dis_u2_susphy_quirk) {
			reg = dwc3_readl(dwc->base, DWC3_GUSB2PHYCFG(0));
			reg |= DWC3_GUSB2PHYCFG_SUSPHY;
			dwc3_writel(dwc->base, DWC3_GUSB2PHYCFG(0), reg);
		}
	}

    dwc3_core_setup_global_control(dwc);

    // setup scratch buffer
    // ---------------

    	/* Adjust Frame Length */
	dwc3_frame_length_adjustment(dwc);

	/* Adjust Reference Clock Period */
//	dwc3_ref_clk_period(dwc);

//	dwc3_set_incr_burst_type(dwc);

    // Event buffer setup
    // ----------

    /*
	 * ENDXFER polling is available on version 3.10a and later of
	 * the DWC_usb3 controller. It is NOT available in the
	 * DWC_usb31 controller.
	 */
	if (DWC3_VER_IS_WITHIN(DWC3, 310A, ANY)) {
		reg = dwc3_readl(dwc->base, DWC3_GUCTL2);
		reg |= DWC3_GUCTL2_RST_ACTBITLATER;
		dwc3_writel(dwc->base, DWC3_GUCTL2, reg);
	}


    if (!DWC3_VER_IS_PRIOR(DWC3, 250A)) {
		reg = dwc3_readl(dwc->base, DWC3_GUCTL1);

		/*
		 * Enable hardware control of sending remote wakeup
		 * in HS when the device is in the L1 state.
		 */
		if (!DWC3_VER_IS_PRIOR(DWC3, 290A))
			reg |= DWC3_GUCTL1_DEV_L1_EXIT_BY_HW;

		/*
		 * Decouple USB 2.0 L1 & L2 events which will allow for
		 * gadget driver to only receive U3/L2 suspend & wakeup
		 * events and prevent the more frequent L1 LPM transitions
		 * from interrupting the driver.
		 */
		if (!DWC3_VER_IS_PRIOR(DWC3, 300A))
			reg |= DWC3_GUCTL1_DEV_DECOUPLE_L1L2_EVT;

		if (dwc->dis_tx_ipgap_linecheck_quirk)
			reg |= DWC3_GUCTL1_TX_IPGAP_LINECHECK_DIS;

		if (dwc->parkmode_disable_ss_quirk)
			reg |= DWC3_GUCTL1_PARKMODE_DISABLE_SS;
			

		if (DWC3_VER_IS_WITHIN(DWC3, 290A, ANY)) {
			if (dwc->maximum_speed == USB_SPEED_FULL ||
			    dwc->maximum_speed == USB_SPEED_HIGH)
				reg |= DWC3_GUCTL1_DEV_FORCE_20_CLK_FOR_30_CLK;
			else
				reg &= ~DWC3_GUCTL1_DEV_FORCE_20_CLK_FOR_30_CLK;
			}
		dwc3_writel(dwc->base, DWC3_GUCTL1, reg);
	}

	if (dwc->dr_mode == USB_DR_MODE_HOST ||
	    dwc->dr_mode == USB_DR_MODE_OTG) {
		reg = dwc3_readl(dwc->base, DWC3_GUCTL);

		/*
		 * Enable Auto retry Feature to make the controller operating in
		 * Host mode on seeing transaction errors(CRC errors or internal
		 * overrun scenerios) on IN transfers to reply to the device
		 * with a non-terminating retry ACK (i.e, an ACK transcation
		 * packet with Retry=1 & Nump != 0)
		 */
		reg |= DWC3_GUCTL_HSTINAUTORETRY;

		dwc3_writel(dwc->base, DWC3_GUCTL, reg);
	}
	
    	/*
	 * Must config both number of packets and max burst settings to enable
	 * RX and/or TX threshold.
	 */
	if (!DWC3_IP_IS(DWC3) && dwc->dr_mode == USB_DR_MODE_HOST) {
		u8 rx_thr_num = 0; // dwc->rx_thr_num_pkt_prd;
		u8 rx_maxburst = 0; //dwc->rx_max_burst_prd;
		u8 tx_thr_num = 0; //dwc->tx_thr_num_pkt_prd;
		u8 tx_maxburst = 0; //dwc->tx_max_burst_prd;

		if (rx_thr_num && rx_maxburst) {
			reg = dwc3_readl(dwc->base, DWC3_GRXTHRCFG);
			reg |= DWC31_RXTHRNUMPKTSEL_PRD;

			reg &= ~DWC31_RXTHRNUMPKT_PRD(~0);
			reg |= DWC31_RXTHRNUMPKT_PRD(rx_thr_num);

			reg &= ~DWC31_MAXRXBURSTSIZE_PRD(~0);
			reg |= DWC31_MAXRXBURSTSIZE_PRD(rx_maxburst);

			dwc3_writel(dwc->base, DWC3_GRXTHRCFG, reg);
		}

		if (tx_thr_num && tx_maxburst) {
			reg = dwc3_readl(dwc->base, DWC3_GTXTHRCFG);
			reg |= DWC31_TXTHRNUMPKTSEL_PRD;

			reg &= ~DWC31_TXTHRNUMPKT_PRD(~0);
			reg |= DWC31_TXTHRNUMPKT_PRD(tx_thr_num);

			reg &= ~DWC31_MAXTXBURSTSIZE_PRD(~0);
			reg |= DWC31_MAXTXBURSTSIZE_PRD(tx_maxburst);

			dwc3_writel(dwc->base, DWC3_GTXTHRCFG, reg);
		}
	}

    return SUCCESS;
err1:
    // usb_phy_shutdown(dwc->usb2_phy);
    // usb_phy_shutdown(dwc->usb3_phy);

err0:
    return ret;
}

/* check whether the core supports IMOD */
bool dwc3_has_imod(dwc3_dev_t *dwc)
{
	return DWC3_VER_IS_WITHIN(DWC3, 300A, ANY) ||
		DWC3_VER_IS_WITHIN(DWC31, 120A, ANY) ||
		DWC3_IP_IS(DWC32);
}

static void dwc3_check_params(dwc3_dev_t *dwc)
{
//	struct device *dev = dwc->dev;
	unsigned int hwparam_gen =
		DWC3_GHWPARAMS3_SSPHY_IFC(dwc->hwparams.hwparams3);

	/* Check for proper value of imod_interval */
	if (dwc->imod_interval && !dwc3_has_imod(dwc)) {
		debug("[USB3] Interrupt moderation not supported\n");
		dwc->imod_interval = 0;
	}

	/*
	 * Workaround for STAR 9000961433 which affects only version
	 * 3.00a of the DWC_usb3 core. This prevents the controller
	 * interrupt from being masked while handling events. IMOD
	 * allows us to work around this issue. Enable it for the
	 * affected version.
	 */
	if (!dwc->imod_interval &&
	    DWC3_VER_IS(DWC3, 300A))
		dwc->imod_interval = 1;

	/* Check the maximum_speed parameter */
	switch (dwc->maximum_speed) {
	case USB_SPEED_FULL:
	case USB_SPEED_HIGH:
		break;
	case USB_SPEED_SUPER:
		if (hwparam_gen == DWC3_GHWPARAMS3_SSPHY_IFC_DIS)
			debug("{USB3] UDC doesn't support Gen 1\n");
		break;
	case USB_SPEED_SUPER_PLUS:
		if ((DWC3_IP_IS(DWC32) &&
		     hwparam_gen == DWC3_GHWPARAMS3_SSPHY_IFC_DIS) ||
		    (!DWC3_IP_IS(DWC32) &&
		     hwparam_gen != DWC3_GHWPARAMS3_SSPHY_IFC_GEN2))
			debug("[USB3] UDC doesn't support SSP\n");
		break;
	default:
		debug("[USB2] invalid maximum_speed parameter %d\n",
			dwc->maximum_speed);
//		fallthrough;
	case USB_SPEED_UNKNOWN:
		switch (hwparam_gen) {
		case DWC3_GHWPARAMS3_SSPHY_IFC_GEN2:
			dwc->maximum_speed = USB_SPEED_SUPER_PLUS;
			break;
		case DWC3_GHWPARAMS3_SSPHY_IFC_GEN1:
			if (DWC3_IP_IS(DWC32))
				dwc->maximum_speed = USB_SPEED_SUPER_PLUS;
			else
				dwc->maximum_speed = USB_SPEED_SUPER;
			break;
		case DWC3_GHWPARAMS3_SSPHY_IFC_DIS:
			dwc->maximum_speed = USB_SPEED_HIGH;
			break;
		default:
			dwc->maximum_speed = USB_SPEED_SUPER;
			break;
		}
		break;
	}

	/*
	 * Currently the controller does not have visibility into the HW
	 * parameter to determine the maximum number of lanes the HW supports.
	 * If the number of lanes is not specified in the device property, then
	 * set the default to support dual-lane for DWC_usb32 and single-lane
	 * for DWC_usb31 for super-speed-plus.
	 */
	// if (dwc->maximum_speed == USB_SPEED_SUPER_PLUS) {
	// 	switch (dwc->max_ssp_rate) {
	// 	case USB_SSP_GEN_2x1:
	// 		if (hwparam_gen == DWC3_GHWPARAMS3_SSPHY_IFC_GEN1)
	// 			dev_warn(dev, "UDC only supports Gen 1\n");
	// 		break;
	// 	case USB_SSP_GEN_1x2:
	// 	case USB_SSP_GEN_2x2:
	// 		if (DWC3_IP_IS(DWC31))
	// 			dev_warn(dev, "UDC only supports single lane\n");
	// 		break;
	// 	case USB_SSP_GEN_UNKNOWN:
	// 	default:
	// 		switch (hwparam_gen) {
	// 		case DWC3_GHWPARAMS3_SSPHY_IFC_GEN2:
	// 			if (DWC3_IP_IS(DWC32))
	// 				dwc->max_ssp_rate = USB_SSP_GEN_2x2;
	// 			else
	// 				dwc->max_ssp_rate = USB_SSP_GEN_2x1;
	// 			break;
	// 		case DWC3_GHWPARAMS3_SSPHY_IFC_GEN1:
	// 			if (DWC3_IP_IS(DWC32))
	// 				dwc->max_ssp_rate = USB_SSP_GEN_1x2;
	// 			break;
	// 		}
	// 		break;
	// 	}
	// }
}


static int dwc3_core_init_mode(dwc3_dev_t *dwc)
{
//	struct device *dev = dwc->dev;
	int ret;

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_set_prtcap(dwc, DWC3_GCTL_PRTCAP_DEVICE);

		// if (dwc->usb2_phy)
		// 	otg_set_vbus(dwc->usb2_phy->otg, false);
		// phy_set_mode(dwc->usb2_generic_phy, PHY_MODE_USB_DEVICE);
		// phy_set_mode(dwc->usb3_generic_phy, PHY_MODE_USB_DEVICE);
        debug("[USB3] Init gadget mode\n");
		// ret = dwc3_gadget_init(dwc);
		// if (ret)
		// 	return dev_err_probe(dev, ret, "failed to initialize gadget\n");
		break;
	case USB_DR_MODE_HOST:
		dwc3_set_prtcap(dwc, DWC3_GCTL_PRTCAP_HOST);

		// if (dwc->usb2_phy)
		// 	otg_set_vbus(dwc->usb2_phy->otg, true);
		// phy_set_mode(dwc->usb2_generic_phy, PHY_MODE_USB_HOST);
		// phy_set_mode(dwc->usb3_generic_phy, PHY_MODE_USB_HOST);
        debug("[USB3] Init host mode\n");
			ret = xhci_init(k1x.base, k1x.irq);
			if (ret < 0) return ret;
				break;
	case USB_DR_MODE_OTG:
    debug("[USB3] Init dual-role mode\n");
		// INIT_WORK(&dwc->drd_work, __dwc3_set_mode);
		// ret = dwc3_drd_init(dwc);
		// if (ret)
		// 	return dev_err_probe(dev, ret, "failed to initialize dual-role\n");
		break;
	default:
		debug("[USB#] Unsupported mode of operation %d\n", dwc->dr_mode);
		return -EINVAL;
	}

	return 0;
}

static void dwc3_regs_dump(dwc3_dev_t *dwc)
{
	debug("[XHCI] DWC3_GSBUSCFG0 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GSBUSCFG0));
	debug("[XHCI] DWC3_GSBUSCFG1 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GSBUSCFG1));
	debug("[XHCI] DWC3_GTXTHRCFG = 0x%X\n", dwc3_readl(dwc->base, DWC3_GTXTHRCFG));
	debug("[XHCI] DWC3_GRXTHRCFG = 0x%X\n", dwc3_readl(dwc->base, DWC3_GRXTHRCFG));
	debug("[XHCI] DWC3_GCTL = 0x%X\n", dwc3_readl(dwc->base, DWC3_GCTL));
	debug("[XHCI] DWC3_GEVTEN = 0x%X\n", dwc3_readl(dwc->base, DWC3_GEVTEN));
	debug("[XHCI] DWC3_GSTS = 0x%X\n", dwc3_readl(dwc->base, DWC3_GSTS));
	debug("[XHCI] DWC3_GUCTL1 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GUCTL1));
	debug("[XHCI] DWC3_GSNPSID = 0x%X\n", dwc3_readl(dwc->base, DWC3_GSNPSID));
	debug("[XHCI] DWC3_GGPIO = 0x%X\n", dwc3_readl(dwc->base, DWC3_GGPIO));
	debug("[XHCI] DWC3_GUID = 0x%X\n", dwc3_readl(dwc->base, DWC3_GUID));
	debug("[XHCI] DWC3_GUCTL = 0x%X\n", dwc3_readl(dwc->base, DWC3_GUCTL));
	debug("[XHCI] DWC3_GBUSERRADDR0 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GBUSERRADDR0));
	debug("[XHCI] DWC3_GBUSERRADDR1 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GBUSERRADDR1));
	debug("[XHCI] DWC3_GPRTBIMAP0 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GPRTBIMAP0));
	debug("[XHCI] DWC3_GPRTBIMAP1 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GPRTBIMAP1));
	debug("[XHCI] DWC3_GHWPARAMS0 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GHWPARAMS0));
	debug("[XHCI] DWC3_GHWPARAMS1 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GHWPARAMS1));
	debug("[XHCI] DWC3_GHWPARAMS2 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GHWPARAMS2));
	debug("[XHCI] DWC3_GHWPARAMS3 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GHWPARAMS3));
	debug("[XHCI] DWC3_GHWPARAMS4 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GHWPARAMS4));
	debug("[XHCI] DWC3_GHWPARAMS5 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GHWPARAMS5));
	debug("[XHCI] DWC3_GHWPARAMS6 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GHWPARAMS6));
	debug("[XHCI] DWC3_GHWPARAMS7 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GHWPARAMS7));
	debug("[XHCI] DWC3_GDBGFIFOSPACE = 0x%X\n", dwc3_readl(dwc->base, DWC3_GDBGFIFOSPACE));
	debug("[XHCI] DWC3_GDBGLTSSM = 0x%X\n", dwc3_readl(dwc->base, DWC3_GDBGLTSSM));
	debug("[XHCI] DWC3_GDBGBMU = 0x%X\n", dwc3_readl(dwc->base, DWC3_GDBGBMU));
	debug("[XHCI] DWC3_GDBGLSPMUX = 0x%X\n", dwc3_readl(dwc->base, DWC3_GDBGLSPMUX));
	debug("[XHCI] DWC3_GDBGLSP = 0x%X\n", dwc3_readl(dwc->base, DWC3_GDBGLSP));
	debug("[XHCI] DWC3_GDBGEPINFO0 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GDBGEPINFO0));
	debug("[XHCI] DWC3_GDBGEPINFO1 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GDBGEPINFO1));
	debug("[XHCI] DWC3_GPRTBIMAP_HS0 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GPRTBIMAP_HS0));
	debug("[XHCI] DWC3_GPRTBIMAP_HS1 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GPRTBIMAP_HS1));
	debug("[XHCI] DWC3_GPRTBIMAP_FS0 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GPRTBIMAP_FS0));
	debug("[XHCI] DWC3_GPRTBIMAP_FS1 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GPRTBIMAP_FS1));
	debug("[XHCI] DWC3_GUCTL2 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GUCTL2));
	debug("[XHCI] DWC3_VER_NUMBER = 0x%X\n", dwc3_readl(dwc->base, DWC3_VER_NUMBER));
	debug("[XHCI] DWC3_VER_TYPE = 0x%X\n", dwc3_readl(dwc->base, DWC3_VER_TYPE));
	debug("[XHCI] DWC3_GUSB2PHYCFG(0) = 0x%X\n", dwc3_readl(dwc->base, DWC3_GUSB2PHYCFG(0)));
	debug("[XHCI] DWC3_GUSB2I2CCTL(0) = 0x%X\n", dwc3_readl(dwc->base, DWC3_GUSB2I2CCTL(0)));
	debug("[XHCI] DWC3_GUSB2PHYACC(0) = 0x%X\n", dwc3_readl(dwc->base, DWC3_GUSB2PHYACC(0)));
	debug("[XHCI] DWC3_GUSB3PIPECTL(0) = 0x%X\n", dwc3_readl(dwc->base, DWC3_GUSB3PIPECTL(0)));
	debug("[XHCI] DWC3_GHWPARAMS8 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GHWPARAMS8));
	debug("[XHCI] DWC3_GUCTL3 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GUCTL3));
	debug("[XHCI] DWC3_GFLADJ = 0x%X\n", dwc3_readl(dwc->base, DWC3_GFLADJ));
	debug("[XHCI] DWC3_GHWPARAMS9 = 0x%X\n", dwc3_readl(dwc->base, DWC3_GHWPARAMS9));

	debug("[XHCI] DWC3_DCFG = 0x%X\n", dwc3_readl(dwc->base, DWC3_DCFG));
	debug("[XHCI] DWC3_DCTL = 0x%X\n", dwc3_readl(dwc->base, DWC3_DCTL));
	debug("[XHCI] DWC3_DEVTEN = 0x%X\n", dwc3_readl(dwc->base, DWC3_DEVTEN));
	debug("[XHCI] DWC3_DSTS = 0x%X\n", dwc3_readl(dwc->base, DWC3_DSTS));
	debug("[XHCI] DWC3_DGCMDPAR = 0x%X\n", dwc3_readl(dwc->base, DWC3_DGCMDPAR));
	debug("[XHCI] DWC3_DGCMD = 0x%X\n", dwc3_readl(dwc->base, DWC3_DGCMD));
	debug("[XHCI] DWC3_DALEPENA = 0x%X\n", dwc3_readl(dwc->base, DWC3_DALEPENA));
	if (DWC3_IP_IS(DWC32))
		debug("[XHCI] DWC3_DCFG1 = 0x%X\n", dwc3_readl(dwc->base, DWC3_DCFG1));
}



int usb3_init()
{
    memset(&k1x, 0, sizeof(k1x));
    memset(&usb2_phy, 0, sizeof(usb2_phy));
    memset(&usb3_phy, 0, sizeof(usb3_phy));

    k1x.base = mmap(NULL, 0x100000, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)DWC3_BASE);
    if (!k1x.base) return -EIO;

    k1x.irq = 125; // 149 additional usb3_vbus_id_wakeup

	k1x.dr_mode = USB_DR_MODE_HOST;
//		phy_type = "utmi";
//	k1x.hsphy_interface = "utmi";
	k1x.dis_enblslpm_quirk = 1;
	k1x.dis_u2_susphy_quirk = 1;
	k1x.dis_u3_susphy_quirk = 1;
	k1x.dis_del_phy_power_chg_quirk = 1;
	k1x.dis_tx_ipgap_linecheck_quirk = 1;
	k1x.parkmode_disable_ss_quirk = 1;
	k1x.dis_rxdet_inp3_quirk = 1;
//	k1x.snps_xhci_trb_ent_quirk = 1;

    ccu_init();

    
    k1x.clk = get_clk_by_id(CLK_USB30);
    if (!k1x.clk) return -EINVAL;

    k1x.rst = get_reset_by_id(RESET_USB3_0);
    if (!k1x.rst) return -EINVAL;    

    usb2_phy.base = k1x.base + 0x30000;

    usb2_phy.clk = get_clk_by_id(CLK_USB30);
    if (!usb2_phy.clk) return -EINVAL;

    k1x.usb2_phy = &usb2_phy;

	usb3_phy.base = mmap(NULL, 0x1000, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)COMBPHY_BASE);
	usb3_phy.rst = get_reset_by_id(RESET_PCIE0);
	if (!usb3_phy.rst) return -EINVAL;
	k1x.usb3_phy = &usb3_phy;


    clock_enable(k1x.clk);    
    udelay(50);  
    reset_deassert(k1x.rst);

    udelay(50);	
    k1x.regs = k1x.base;

    if (!dwc3_core_is_valid(&k1x))
        return -ENOSUPPORT;

    dwc3_cache_hwparams(&k1x);

    // DRD mode
    dwc3_get_dr_mode(&k1x);

    int ret = dwc3_core_init(&k1x);
    if (ret < 0) return ret;

    dwc3_check_params(&k1x);

    dwc3_core_init_mode(&k1x);

//	dwc3_regs_dump(&k1x);


//	dwc3_regs_dump(&k1x);

	return SUCCESS;
}