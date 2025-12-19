#ifndef __K1X_H__
#define __K1X_H__

#include <stdint.h>
#include "k1x-clock.h"
#include "k1x-reset.h"

#define BIT(nr)			(1UL << (nr))
#define CLOCK_GATE(_struct, _base, _reg, _mask) \
        clk_t _struct = {   \
            .base   = _base,  \
            .reg    = _reg,    \
            .mask   = _mask,  \
        };
        

#define APMU_BASE 0xD4282800ULL

// APMU registers

#define APMU_USB_CLK_RST_CTRL   0x5c  


#define PMU_SD_ROT_WAKE_CLR    0x7c  

// APMU_USB_CLK_RST_CTRL
#define USB_AXI_RST         BIT(0)
#define USB_AXI_CLK         BIT(1)
#define USBP1_AXI_RST       BIT(4)
#define USBP1_AXI_CLK       BIT(5)
#define USB3_0_BUS_CLK_EN   BIT(8) 
#define USB3_0_AHB_RSTN     BIT(9)
#define USB3_0_VCC_RESETN   BIT(10)
#define USB3_0_PHY_RESETN   BIT(11)

// PMU_SD_ROT_WAKE_CLR
#define PMU_SD_ROT_WAKE_CLR_VBUS_DRV    BIT(21)
#define USB_VBUS_WK_CLR BIT(18)


extern void *apmu_base;


typedef struct {
    void     *base; 
    uint32_t reg;
    uint32_t mask;
} clk_t;

int ccu_init(void);
void clock_enable(clk_t *clk);
clk_t *get_clk_by_id(int id);

clk_t *get_reset_by_id(int id);
void reset_deassert(clk_t *rst);
void reset_assert(clk_t *rst);

#endif /* __K1X_H__ */