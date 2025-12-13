#ifndef __WM_H__
#define __WM_H__

#include<wm_msg.h>

int wm_create_window(uint32_t width, uint32_t height, uint32_t flags, uint32_t *wid, int *shm_cap);

#endif /* __WM_H__ */