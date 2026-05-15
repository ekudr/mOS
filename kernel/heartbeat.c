#include <common.h>
#include <riscv.h>

extern uint64_t ticks;

struct heartbeat_led {
    int status;
    uint64_t timer;
} hb_led;

#if defined(__SPACEMIT_K1__)
static int pinctrl_set(uint32_t pin, uint32_t sel) {
    uint64_t reg = PINCTRL_BASE + (pin*4);
//    DEBUG("[PIN_CTRL] sel 0x%X => 0x%X\n", sel, reg);
    putreg32(sel, PA2DA(reg));
//    DEBUG("[PIN_CTRL] 0x%X = 0x%X\n", reg, getreg32(reg));
    return 0;
}
#endif

void heartbeat_init(void)
{
    hb_led.timer = ticks + 200;
    hb_led.status = 0;

#if defined(__JH7110__)
    uint32_t shift = ((3 & 0x3) << 3);
    uint32_t mask = 3 << shift;

    // set RGPIO3 to OUT  
    putreg32(getreg32(PA2DA(AON_PINCTRL_BASE)) & !mask, PA2DA(AON_PINCTRL_BASE));
#endif

#if defined(__SPACEMIT_K1__)
    // connect led to GPIO_96
    pinctrl_set(HEARTBEAT_PIN, HEARTBEAT_SEL);
    // set GPIO_96 to OUT  
    putreg32(1,(PA2DA(GPIO_BASE)+0x100+0x54));
//    putreg32(1,(GPIO_BASE+0x100+0x18));
#endif
}


static void led_on(void) 
{
#if defined(__JH7110__)
    uint32_t shift = ((3 & 0x3) << 3);
    uint32_t mask = 3 << shift;
    // set RGPIO3 to 1  
    putreg32((getreg32(PA2DA(AON_PINCTRL_BASE)+4) & ~mask) | (1 << shift), PA2DA(AON_PINCTRL_BASE)+4);
#endif

#if defined(__SPACEMIT_K1__)
    putreg32(1,(PA2DA(GPIO_BASE)+0x100+0x18));
#endif

    hb_led.status = 1;
}

static void led_off(void) 
{
#if defined(__JH7110__)
    uint32_t shift = ((3 & 0x3) << 3);
    uint32_t mask = 3 << shift;
    // set RGPIO3 to 0  
    putreg32((getreg32(PA2DA(AON_PINCTRL_BASE)+4) & ~mask) & ~(1 << shift), PA2DA(AON_PINCTRL_BASE)+4);
#endif

#if defined(__SPACEMIT_K1__)
    putreg32(1,(PA2DA(GPIO_BASE)+0x100+0x24));
#endif

    hb_led.status = 0;
}

static void led_switch(void) 
{
    if (hb_led.status)
        led_off();
    else
        led_on();
}

void heartbeat(void)
{
    if(ticks >= hb_led.timer){
        led_switch();
        hb_led.timer = ticks + 100;
    }
}