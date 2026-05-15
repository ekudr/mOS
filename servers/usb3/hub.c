#include <libsys/common.h>
#include <string.h>
#include <libsys/timer.h>
#include <libsys/riscv.h>
#include <libsys/memory.h>

#define GPIO_BASE   0xD4019000UL
#define PINCTRL_BASE 0xD401E000UL

/* pin offset */
#define PINID(x)	((x) + 1)

#define DVL1        PINID(120)
#define GPIO_123    PINID(142)
#define GPIO_124    PINID(143)

/* pin mux */
#define MUX_MODE0       0
#define MUX_MODE1       1
#define MUX_MODE2       2
#define MUX_MODE3       3
#define MUX_MODE4       4
#define MUX_MODE5       5
#define MUX_MODE6       6
#define MUX_MODE7       7

/* strong pull resistor */
#define SPU_EN          (1 << 3)

/* edge detect */
#define EDGE_NONE       (1 << 6)
#define EDGE_RISE       (1 << 4)
#define EDGE_FALL       (1 << 5)
#define EDGE_BOTH       (3 << 4)

/* slew rate output control */
#define SLE_EN          (1 << 7)

/* schmitter trigger input threshhold */
#define ST00            (0 << 8)
#define ST01            (1 << 8)
#define ST02            (2 << 8)
#define ST03            (3 << 8)

/* driver strength*/
#define PAD_1V8_DS0     (0 << 11)
#define PAD_1V8_DS1     (1 << 11)
#define PAD_1V8_DS2     (2 << 11)
#define PAD_1V8_DS3     (3 << 11)

/*
 * notice: !!!
 * ds2 ---> bit10, ds1 ----> bit12, ds0 ----> bit11
*/
#define PAD_3V_DS0      (0 << 10)     /* bit[12:10] 000 */
#define PAD_3V_DS1      (2 << 10)     /* bit[12:10] 010 */
#define PAD_3V_DS2      (4 << 10)     /* bit[12:10] 100 */
#define PAD_3V_DS3      (6 << 10)     /* bit[12:10] 110 */
#define PAD_3V_DS4      (1 << 10)     /* bit[12:10] 001 */
#define PAD_3V_DS5      (3 << 10)     /* bit[12:10] 011 */
#define PAD_3V_DS6      (5 << 10)     /* bit[12:10] 101 */
#define PAD_3V_DS7      (7 << 10)     /* bit[12:10] 111 */

/* pull up/down */
#define PULL_DIS        (0 << 13)     /* bit[15:13] 000 */
#define PULL_UP         (6 << 13)     /* bit[15:13] 110 */
#define PULL_DOWN       (5 << 13)     /* bit[15:13] 101 */

#define K1X_PADCONF(pinid, conf, mux)	((pinid) * 4) (conf) (mux)

#define GPIO97_SEL      (MUX_MODE1 | EDGE_NONE | PULL_DOWN | PAD_1V8_DS0)
#define GPIO123_SEL     (MUX_MODE0 | EDGE_NONE | PULL_DOWN | PAD_1V8_DS0)
#define GPIO124_SEL     (MUX_MODE0 | EDGE_NONE | PULL_UP   | PAD_1V8_DS2)

void *pinctrl_base;
void *gpio_base;

static int pinctrl_set(uint32_t pin, uint32_t sel) 
{
    uint64_t reg = (uint64_t)pinctrl_base + (pin*4);
//    DEBUG("[PIN_CTRL] sel 0x%X => 0x%X\n", sel, reg);
    putreg32(sel, reg);
//    DEBUG("[PIN_CTRL] 0x%X = 0x%X\n", reg, getreg32(reg));
    return 0;
}




int hub_enable()
{
    pinctrl_base = mmap(NULL, 0x1000, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)PINCTRL_BASE);
    if (!pinctrl_base) return -EIO;

    gpio_base = mmap(NULL, 0x1000, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)GPIO_BASE);
    if (!gpio_base) return -EIO;    

    // connect GPIO_97 to pin DVL1
    pinctrl_set(DVL1, GPIO97_SEL);

    // connect GPIO_123 to pin GPIO_123
    pinctrl_set(GPIO_123, GPIO123_SEL);

    // connect GPIO_124 to pin GPIO_124
    pinctrl_set(GPIO_124, GPIO124_SEL);

    // HUB enable
    // set GPIO_123 to 1
    putreg32((1<<27),(uint64_t)gpio_base+0x100+0x18);
    // set GPIO_123 to OUT  
    putreg32((1<<27),(uint64_t)gpio_base+0x100+0x54);

    udelay(10000); // wait 10ms
    // HUB reset
    // set GPIO_124 to 1
    putreg32((1<<28),(uint64_t)gpio_base+0x100+0x18);
    // set GPIO_124 to OUT  
    putreg32((1<<28),(uint64_t)gpio_base+0x100+0x54);
    
    udelay(10000); // wait 10ms
    // HUB VBUS ON
    // set GPIO_97 to 1
    putreg32(2,(uint64_t)gpio_base+0x100+0x18);
    // set GPIO_97 to OUT  
    putreg32(2,(uint64_t)gpio_base+0x100+0x54);    

    return SUCCESS;
}