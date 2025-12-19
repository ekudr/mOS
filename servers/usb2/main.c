#include <common.h>

int ehci_init();

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}

int main()
{
    debug("USB3 SPACEMIT K1 driver ver 0.0.0\n");

    int ret = ehci_init();
    if (ret < 0) panic("EHCI init");
}