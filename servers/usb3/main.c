#include <common.h>
#include <sched.h>

int usb3_init();

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}

int main()
{
    for (size_t i = 0; i < 1000; i++)
    {
        sched_yield();
    }
    
    debug("USB3 SPACEMIT K1 driver ver 0.0.0\n");

    int ret = usb3_init();
    if (ret < 0) panic("EHCI init");
}