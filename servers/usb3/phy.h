#ifndef __PHY_H__
#define __PHY_H__

#include "k1x.h"

#define PHY_28LP	0x2800
#define PHY_40LP	0x4000
#define PHY_55LP	0x5500

#define MV_PHY_FLAG_PLL_LOCK_BYPASS	(1 << 0)

#define USB2_PHY_REG01			0x4
#define USB2_PHY_REG01_PLL_IS_READY	(0x1 << 0)
#define USB2_PHY_REG04			0x10
#define USB2_PHY_REG04_EN_HSTSOF	(0x1 << 0)
#define USB2_PHY_REG04_AUTO_CLEAR_DIS	(0x1 << 2)
#define USB2_PHY_REG08			0x20
#define USB2_PHY_REG08_DISCON_DET	(0x1 << 9)
#define USB2_PHY_REG0D			0x34
#define USB2_PHY_REG26			0x98
#define USB2_PHY_REG22			0x88
#define USB2_CFG_FORCE_CDRCLK		(0x1 << 6)
#define USB2_PHY_REG06			0x18
#define USB2_CFG_HS_SRC_SEL		(0x1 << 0)

#define USB2D_CTRL_RESET_TIME_MS	50


typedef struct usb_phy
{
    void *base;
    clk_t *clk;

} usb_phy_t;


int usb_phy_init(usb_phy_t *phy);

#endif /* __PHY_H__ */