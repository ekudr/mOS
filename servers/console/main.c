#include <common.h>
#include <mosstd.h>
#include <string.h> 
#include <libsys/ipc.h>
#include <libsys/cap.h>
#include <vfs.h>
#include <devman.h>
#include <tty.h>
#include <wm.h>
#include <ipc.h>

#include "text_render.h"

/* import font that's in the object file*/
extern char _binary_font_psf_start[];
extern char _binary_font_psf_end[];

fb_console_t con;

int cap;

void panic(const char *str)
{
    debug("\x1b[31m[PANIC]\x1b[0m %s\n", str);
    for(;;);    
}

void init_server(void)
{
    cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    debug("Console driver created cap 0x%lX\n", cap);

 
    int ret = devman_register("con0", cap);    
    if (ret < 0)
        panic("PANIC CONSOLE");


    // int vfs = vfs_open("/dev/con0");
    // debug("[CONSOLE] open vfs returned %d\n", vfs);
    // if (vfs < 0) {
        
    int  vfs = vfs_create("/dev/con0", VFS_DEVICE, cap);
    if (vfs < 0)
        panic("CONSOLE VFS REGISTER");
    // }
    // debug("[CONSOLE] create vfs returned %d\n", vfs);
}

int console_init()
{
    font_t font;

    int shm;
    uint32_t wid;

    int st = wm_create_window(640, 480, 0, &wid, &shm);
    if (st < 0)
        panic("PANIC CONSOLE");
        
    void *addr = ipc_shm_attach(shm, NULL, 0);
    if (!addr) panic("shm attach");

    // debug("Create Window id %d shm 0x%lX\n", wid, addr);
    // panic("STOP");
//    void *fb_base = mmap(NULL, FB_SIZE, MAP_MEMIO | MAP_READ | MAP_WRITE, (void *)FB_BASE);
//    if (!fb_base) panic("fb map");


    int ret = font_load(_binary_font_psf_start, &font);
    if (ret < 0) panic("font load");

    ret = fb_console_init(&con, addr, 640, 480, 32, 2560, 
                        0xFFDBA400, 0x9F080600, font);
    if (ret < 0) panic("fb console init");

    fb_puts(&con, "mOS version 0.0.2\n");
//    fb_puts(&con, "/>\n");
    return SUCCESS;
}

int main()
{
    debug("Framebuffer console v 0.0.1\n");
    console_init();
    init_server();

    
    while (1)
    {
        uint64_t info, sender;
        struct tty_message *msg;
        msg = (struct tty_message *)get_ipc_buffer()->msg;

        info = ipc_nb_recv(cap, &sender);
        int ret = (int)(label_from_msginfo_word(info));
        if(ret < 0) {
            debug("[CONS] received info error %d\n", ret);
            continue;    
        }   
        if (msg->type == TTY_PUT_STRING) fb_puts(&con ,msg->message);

    }
    return 0;
}