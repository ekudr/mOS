#ifndef __CAP_TYPES_H__
#define __CAP_TYPES_H__

#define CRIGHT_SND  (1U << 0)
#define CRIGHT_RCV  (1U << 1)
#define CRIGHT_MAP  (1U << 2)
#define CRIGHT_IRQ  (1U << 3)
#define CRIGHT_GRANT  (1U << 4)

#define CAP_SELF 0
#define CAP_NS   1

typedef enum {
    CAP_NONE = 0,
    CAP_ENDPOINT,
    CAP_REPLAY,
    CAP_FASTCALL,
    CAP_SHMEMORY,
    CAP_IRQ,
} cap_type_t;



#endif /* __CAP_TYPES_H__ */