#ifndef __TTY_H__
#define __TTY_H__

#define TTY_PUT_STRING  1

struct tty_message {
    int type;
    char message[100];
};

#endif /* __TTY_H__ */