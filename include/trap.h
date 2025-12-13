#ifndef __TRAP_H__
#define __TRAP_H__

#include <stdint.h>
#include <stddef.h>

void usertrapret(void);


void sbi_set_timer(uint64_t stime_value);

// ask the PLIC what interrupt we should serve.
int plic_claim(void);

// tell the PLIC we've served this IRQ.
void plic_complete(uint32_t irq);

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr(void);

void heartbeat(void);

void delivery_signal(task_t * t, signal_t sig, signal_payload_t payload);
uint64_t usersigret(void);

#endif /* _TRAP_H */