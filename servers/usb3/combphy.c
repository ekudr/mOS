#include <mosstd.h>
#include <riscv.h>

#include <libsys/timer.h>

#include "k1x.h"
#include "phy.h"

#define SPACEMIT_COMBPHY_WAIT_TIMEOUT 1000
#define SPACEMIT_COMBPHY_MODE_SEL BIT(3)

// Registers for USB3 PHY
#define SPACEMIT_COMBPHY_USB_REG1 0x68
#define SPACEMIT_COMBPHY_USB_REG1_VAL 0x0
#define SPACEMIT_COMBPHY_USB_REG2 (0x12 << 2)
#define SPACEMIT_COMBPHY_USB_REG2_VAL 0x603a2276
#define SPACEMIT_COMBPHY_USB_REG3 (0x02 << 2)
#define SPACEMIT_COMBPHY_USB_REG3_VAL 0x97c
#define SPACEMIT_COMBPHY_USB_REG4 (0x06 << 2)
#define SPACEMIT_COMBPHY_USB_REG4_VAL 0x0
#define SPACEMIT_COMBPHY_USB_PLL_REG 0x8
#define SPACEMIT_COMBPHY_USB_PLL_MASK 0x1
#define SPACEMIT_COMBPHY_USB_PLL_VAL 0x1
#define SPACEMIT_COMBPHY_USB_TERM_SHORT 0x3000


static inline void spacemit_reg_updatel(uint64_t reg, u32 offset, u32 mask,
					u32 val)
{
	u32 tmp;
	tmp = getreg32(reg + offset);
	tmp = (tmp & ~(mask)) | val;
	putreg32(tmp, reg + offset);
}

static int combphy_wait_ready(usb_phy_t *phy, uint32_t offset, uint32_t mask, uint32_t val)
{
	int timeout = SPACEMIT_COMBPHY_WAIT_TIMEOUT;
	while (((getreg32((uint64_t)phy->base + offset) & mask) != val) && --timeout)
		;
	if (!timeout) {
		return -ETIMEOUT;
	}
	debug("phy init timeout remain: %d\n", timeout);
	return 0;
}

static int combphy_set_mode(usb_phy_t *phy)
{
    uint32_t val;
    uint64_t reg_addr;

    reg_addr = (uint64_t)apmu_base + PMUA_USB_PHY_CTRL0;
    val = getreg32(reg_addr);
    val |= SPACEMIT_COMBPHY_MODE_SEL;
    putreg32(val, reg_addr);

    return SUCCESS;
}

static int combphy_init_usb(usb_phy_t *phy)
{
	int ret;
	void *base = phy->base;
	debug("[COMBPHY] USB3 PHY init\n");

	putreg32(SPACEMIT_COMBPHY_USB_REG1_VAL, (uint64_t)base + SPACEMIT_COMBPHY_USB_REG1);
	putreg32(SPACEMIT_COMBPHY_USB_REG2_VAL, (uint64_t)base + SPACEMIT_COMBPHY_USB_REG2);
	putreg32(SPACEMIT_COMBPHY_USB_REG3_VAL, (uint64_t)base + SPACEMIT_COMBPHY_USB_REG3);
	putreg32(SPACEMIT_COMBPHY_USB_REG4_VAL, (uint64_t)base + SPACEMIT_COMBPHY_USB_REG4);

	ret = combphy_wait_ready(phy, SPACEMIT_COMBPHY_USB_PLL_REG,
					  SPACEMIT_COMBPHY_USB_PLL_MASK,
					  SPACEMIT_COMBPHY_USB_PLL_VAL);

//	if (priv->suspend_term_quirk) {
//		spacemit_reg_updatel((uint64_t)base, 0x18, 0, SPACEMIT_COMBPHY_USB_TERM_SHORT);
//	}

	if (ret < 0)
		debug("USB3 PHY init timeout!\n");

	return ret;
}

int combphy_init(usb_phy_t *phy)
{
    int ret;

    ret = combphy_set_mode(phy);
    if (ret < 0) {
        debug("[COMBPHY] failed to set mode for PHY type, ret = %d\n", ret);
        return ret;
    }

    reset_deassert(phy->rst);
	udelay(50);

    ret = combphy_init_usb(phy);
    if (ret < 0) {
        debug("[COMBPHY] failed to init PHY as usb type, ret = %d\n", ret);
        return ret;
    }

    return SUCCESS;
}