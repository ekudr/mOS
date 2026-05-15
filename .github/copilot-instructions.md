# mOS Codebase Instructions for AI Coding Agents

## Project Overview
mOS is a RISC-V microkernel hybrid OS supporting VisionFive2, BPI-F3, and QEMU. The architecture uses capability-based security with inter-process communication (IPC) for user-space services.

## Build System & Setup
- **Build Tool**: Meson + Ninja (not Make)
- **Setup command**: `meson setup build -Dboard=<bpi-f3|vf2|qemu>`
- **Build command**: `ninja -C build`
- **Board-specific compilation**: Flags in [meson.build](meson.build) set `-march` and `-mcpu` per board
- **Key flags**: `-nostdlib -static -ffreestanding -O3` (embedded kernel compilation)

## Architecture & Major Components

### Kernel Core ([kernel/](kernel/))
- **Initialization**: [kernel/init.c](kernel/init.c) - boots all HARTs (hardware threads), initializes VM, IPC, PLIC
- **Trap handling**: [kernel/trap.c](kernel/trap.c) + [kernel/trampoline.S](kernel/trampoline.S) - manages syscalls, exceptions, interrupts
- **Scheduling**: [kernel/sched.c](kernel/sched.c) - per-CPU task scheduling with kernel locks
- **Syscall dispatch**: [kernel/syscall.c](kernel/syscall.c) - routes system calls (numbers in [include/syscall.h](include/syscall.h))

### Memory Management ([kernel/mm/](kernel/mm/))
- **Virtual Memory**: [kernel/mm/vm.c](kernel/mm/vm.c) - kernel page table setup
- **User VM**: [kernel/mm/uvm.c](kernel/mm/uvm.c) - per-task address spaces
- **Shared Memory**: [kernel/mm/shmem.c](kernel/mm/shmem.c) - IPC shared buffers
- **Allocation**: SLUB allocator via [libsys/malloc.c](libsys/malloc.c)

### IPC & Capability System
- **IPC Core**: [kernel/ipc.c](kernel/ipc.c) - message passing with inline registers + out-of-line buffers
- **Message format**: `msginfo_word_t` encodes label (32-bit), length (16-bit), extra caps (8-bit), flags (8-bit)
- **Capability model**: [include/cap.h](include/cap.h) - each task has capability node (max 64 caps), grants/transfers via syscalls
- **Message registers**: a0-a4 used for inline message words ([kernel/ipc.c](kernel/ipc.c))

### User-Space Services ([servers/](servers/))
- **Service Architecture**: Each server runs as isolated task with its own capability domain
- **Key Services**: 
  - `vfs` - filesystem
  - `nameserver` - service discovery
  - `devman` - device management  
  - `console`/`tty` - terminal I/O
  - `mmc_drv`, `usb-core`, `usb2`, `usb3` - device drivers
- **Common Library**: [libsys/](libsys/) - syscall wrappers, malloc, printf

## Critical Patterns

### Syscall Convention
1. Syscall number in `a7` register
2. Parameters in `a0-a6`
3. Trap to kernel via `ecall`
4. Kernel invokes handler from [kernel/syscall.c](kernel/syscall.c)
5. Return value in `a0`

### IPC Message Flow
1. Sender calls `SYS_send` (blocking) or `SYS_nb_send` (non-blocking)
2. Kernel copies inline registers + out-of-line message buffer to receiver
3. Capability objects transferred via `extra_caps` count
4. Receiver wakes when message arrives
5. Example: [servers/nameserver/main.c](servers/nameserver/main.c) - waits for requests via `sys_recv()`

### Capability Grants
- `SYS_cap_grant`: Give cap to another task (both retain access)
- `SYS_cap_transfer`: Move cap ownership to another task
- Cap format: 32-bit ID with embedded root/node indices ([include/cap.h](include/cap.h))

### Multi-Core Synchronization
- **Per-CPU structure**: `struct cpu *current_cpu` in TP register (RISC-V calling convention)
- **Spinlocks**: [kernel/spinlock.c](kernel/spinlock.c) - disable interrupts + atomic swap
- **Key lock**: `tickslock` protects timer tick counter

### Trapframe & Context Switching
- **Trapframe** ([include/sched.h](include/sched.h)): kernel state saved on user-space task switch
  - `kernel_satp`: kernel page table
  - `kernel_sp`: kernel stack top
  - `epc`: user program counter to resume
- **Context** ([include/sched.h](include/sched.h)): callee-saved regs for kernel-to-kernel switches

## Board-Specific Code
- Board headers in [board/{vf2,bpi-f3,qemu}/include/](board/)
- CPU/HART mappings: `HARTID2CPU()`, `CPU2HARTID()` macros
- Per-board compilation flags set in [meson.build](meson.build) (lines 36-47)
- QEMU limitation: MEMIO not allowed in user space

## Common Development Tasks

### Adding a Syscall
1. Add `#define SYS_myname N` in [include/syscall.h](include/syscall.h)
2. Add handler function in [kernel/syscall.c](kernel/syscall.c)
3. Route in syscall dispatch switch
4. Add wrapper in [libsys/syscall.c](libsys/syscall.c) for user-space

### Adding a Server Service
1. Create source in [servers/myserver/](servers/)
2. Add executable target to [servers/meson.build](servers/meson.build)
3. Link with `libsys_a` and `libc_a`
4. Use libsys IPC wrappers for message send/recv
5. Register with nameserver for discoverability

### Debugging Memory Issues
- Use `-D__DEBUG__` in meson buildtype (adds debug macro)
- Call `kprint()` for kernel-space output (wraps printf)
- Call `printf()` in user-space servers
- Check [kernel/mm/kmem.c](kernel/mm/kmem.c) for SLUB allocator state

## Include Dependencies
- **Common includes**: [include/common.h](include/common.h) - defines panic, debug, BIT, NELEM, etc.
- **Architecture**: [include/riscv.h](include/riscv.h) - CSR read/write macros
- **Board data**: `#include <board.h>` auto-selects per Meson config

## File Organization Rules
- Kernel code: `kernel/` and `include/`
- User libraries: `libsys/`, `libc/`, `lib/`
- Services: `servers/{servicename}/`
- Assembly: `.S` files (preprocessed), `switch.S` for context switch, `trampoline.S` for trap entry
