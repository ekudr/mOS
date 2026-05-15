#include <mosstd.h>
#include <riscv.h>
//#include <libsys/memory.h>
//#include <memory.h>

#include "k1x.h"

void *apmu_base;

// Clocks
static CLOCK_GATE(usb_axi_clk, &apmu_base, APMU_USB_CLK_RST_CTRL, USB_AXI_CLK);
static CLOCK_GATE(usb_p1_clk, &apmu_base, APMU_USB_CLK_RST_CTRL, USBP1_AXI_CLK);
static CLOCK_GATE(usb30_clk, &apmu_base, APMU_USB_CLK_RST_CTRL, USB3_0_BUS_CLK_EN);
static CLOCK_GATE(pcie0_clk, &apmu_base, APMU_PCIE_CLK_RES_CTRL_0, 0x7);


// Resets
static RESET_GATE(usb_axi_rst, &apmu_base, APMU_USB_CLK_RST_CTRL, USB_AXI_RST, USB_AXI_RST, 0);
static RESET_GATE(usb_p1_rst, &apmu_base, APMU_USB_CLK_RST_CTRL, USBP1_AXI_RST, USBP1_AXI_RST, 0);
static RESET_GATE(usb30_rst, &apmu_base, APMU_USB_CLK_RST_CTRL, BIT(9) | BIT(10) | BIT(11), BIT(9) | BIT(10) | BIT(11), 0);
static RESET_GATE(pcie0_rst, &apmu_base, APMU_PCIE_CLK_RES_CTRL_0, BIT(3)|BIT(4)|BIT(5)|BIT(8), BIT(3)|BIT(4)|BIT(5), BIT(8));
static RESET_GATE(pcie1_rst, &apmu_base, APMU_PCIE_CLK_RES_CTRL_1, BIT(3)|BIT(4)|BIT(5)|BIT(8), BIT(3)|BIT(4)|BIT(5), BIT(8));
static RESET_GATE(pcie2_rst, &apmu_base, APMU_PCIE_CLK_RES_CTRL_2, 0x138, 0x38, 0x100);
	

static clk_t *k1x_clk_table[] = {
    [CLK_USB_P1]    = &usb_p1_clk,
    [CLK_USB_AXI]   = &usb_axi_clk,
    [CLK_USB30]     = &usb30_clk,
    [CLK_PCIE0]    = &pcie0_clk,

};

static rst_t *k1x_rst_table[] = {
    
    [RESET_USB_AXI]   = &usb_axi_rst,
    [RESET_USBP1_AXI] = &usb_p1_rst,
    [RESET_USB3_0]    = &usb30_rst,
    [RESET_PCIE0]     = &pcie0_rst,
    [RESET_PCIE1]	  = &pcie1_rst,
    [RESET_PCIE2]	  = &pcie2_rst,
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

rst_t *get_reset_by_id(int id)
{
    return k1x_rst_table[id];
}

void reset_deassert(rst_t *rst)
{
    uint32_t val;
    uint64_t base = *(uint64_t *)rst->base;

    val = getreg32((uint64_t)base + rst->reg);
    val &= ~rst->mask;
    val |= rst->deassert_val;
    putreg32(val, (uint64_t)base + rst->reg); 
}

void reset_assert(rst_t *rst)
{
    uint32_t val;
    uint64_t base = *(uint64_t *)rst->base;

    val = getreg32((uint64_t)base + rst->reg);
    val &= ~rst->mask;
    val |= rst->assert_val;
    putreg32(val, (uint64_t)base + rst->reg); 
}

void dump_reg(size_t reg)
{
    debug("[REG DUMP] 0x%lX = 0x%X\n", reg, getreg32((uint64_t)apmu_base + reg));
}