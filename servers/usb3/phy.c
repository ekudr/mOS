#include <mosstd.h>
#include <riscv.h>
//#include <libsys/memory.h>
#include <libsys/timer.h>

#include "k1x.h"
#include "phy.h"

int usb_phy_init(usb_phy_t *phy)
{
    uint32_t loops, temp;
    uint64_t base = (uint64_t)phy->base;

    clock_enable(phy->clk);


    // make sure the usb controller is not under reset process before any configuration
	udelay(50);
	putreg32(0xbec4, base + USB2_PHY_REG26); //24M ref clk
	udelay(150);

    loops = USB2D_CTRL_RESET_TIME_MS * 1000;

	//wait for usb2 phy PLL ready
	do {
		temp = getreg32(base + USB2_PHY_REG01);
		if (temp & USB2_PHY_REG01_PLL_IS_READY)
			break;
		udelay(50);
	} while(--loops);

	if (loops == 0)
		debug("[USB2] Wait PHY_REG01[PLLREADY] timeout\n");

	//release usb2 phy internal reset and enable clock gating
	putreg32(0x60ef, base + USB2_PHY_REG01);
	putreg32(0x1c, base + USB2_PHY_REG0D);

	//select HS parallel data path
	temp = getreg32(base + USB2_PHY_REG06);
	// temp |= USB2_CFG_HS_SRC_SEL;
	temp &= ~(USB2_CFG_HS_SRC_SEL);
	putreg32(temp, base + USB2_PHY_REG06);

	/* auto clear host disc*/
	temp = getreg32(base + USB2_PHY_REG04);
	temp |= USB2_PHY_REG04_AUTO_CLEAR_DIS;
	putreg32(temp, base + USB2_PHY_REG04);

	return SUCCESS;
}