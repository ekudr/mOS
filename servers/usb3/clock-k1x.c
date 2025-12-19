#include <common.h>
#include <riscv.h>
#include <libsys/memory.h>
#include <memory.h>

#include "k1x.h"

void *apmu_base;

// Clocks
static CLOCK_GATE(usb_axi_clk, &apmu_base, APMU_USB_CLK_RST_CTRL, USB_AXI_CLK);
static CLOCK_GATE(usb_p1_clk, &apmu_base, APMU_USB_CLK_RST_CTRL, USBP1_AXI_CLK);
static CLOCK_GATE(usb30_clk, &apmu_base, APMU_USB_CLK_RST_CTRL, USB3_0_BUS_CLK_EN);


// Resets
static CLOCK_GATE(usb_axi_rst, &apmu_base, APMU_USB_CLK_RST_CTRL, USB_AXI_RST);
static CLOCK_GATE(usb_p1_rst, &apmu_base, APMU_USB_CLK_RST_CTRL, USBP1_AXI_RST);
static CLOCK_GATE(usb30_rst, &apmu_base, APMU_USB_CLK_RST_CTRL, USB3_0_AHB_RSTN | USB3_0_VCC_RESETN | USB3_0_PHY_RESETN);

static clk_t *k1x_clk_table[] = {
    [CLK_USB_P1]    = &usb_p1_clk,
    [CLK_USB_AXI]   = &usb_axi_clk,
    [CLK_USB30]     = &usb30_clk,

};

static clk_t *k1x_rst_table[] = {
    
    [RESET_USB_AXI]   = &usb_axi_rst,
    [RESET_USBP1_AXI] = &usb_p1_rst,
    [RESET_USB3_0]    = &usb30_rst,
};

void clock_enable(clk_t *clk)
{
    uint64_t base = *(uint64_t *)clk->base;
//    debug("[CLK] put 0x%X to 0x%p\n", getreg32(base + clk->reg) | clk->mask, base + clk->reg);
    putreg32(getreg32(base + clk->reg) | clk->mask, base + clk->reg);
}

int ccu_init()
{
    uint64_t pa = PGROUNDDOWN(APMU_BASE);
    uint64_t off = APMU_BASE & (PAGE_SIZE - 1);
    apmu_base = mmap(NULL, 0x1000, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)pa);
    if (!apmu_base) return -ENOMEM;
    apmu_base += off;
}

clk_t *get_clk_by_id(int id)
{
    return k1x_clk_table[id];
}

clk_t *get_reset_by_id(int id)
{
    return k1x_rst_table[id];
}

void reset_deassert(clk_t *rst)
{
    uint64_t base = *(uint64_t *)rst->base;
//    debug("[RST] put 0x%X to 0x%p\n", getreg32(base + rst->reg) | rst->mask, base + rst->reg);
    putreg32(getreg32(base + rst->reg) | rst->mask, base + rst->reg); 
//    debug("[RST] got 0x%X from 0x%p\n", getreg32(base + rst->reg), base + rst->reg);
}

void reset_assert(clk_t *rst)
{
    uint64_t base = *(uint64_t *)rst->base;
//    debug("[RST] put 0x%X to 0x%p\n", getreg32(base + rst->reg) | rst->mask, base + rst->reg);
    putreg32(getreg32(base + rst->reg) & ~rst->mask, base + rst->reg);   
}