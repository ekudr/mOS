// microkernel_api.h

// Handle for a kernel-managed memory object.
typedef int mem_obj_handle_t;

// Allocates a new physical memory object of a given size.
// Returns a handle on success, or an error code.
mem_obj_handle_t kernel_mem_alloc(size_t size);

// Maps a memory object into the address space of a target process.
// Returns the mapped virtual address on success, or an error code.
void* kernel_mem_map(mem_obj_handle_t mem_handle, pid_t target_pid);

// Unmaps a memory object from the address space of a target process.
int kernel_mem_unmap(void* address, size_t size);

// Frees the underlying physical memory object.
int kernel_mem_free(mem_obj_handle_t mem_handle);

// Inter-process communication (IPC) function.
int kernel_send_ipc(pid_t target_pid, const void* message, size_t msg_size);
int kernel_receive_ipc(void* buffer, size_t buf_size);
