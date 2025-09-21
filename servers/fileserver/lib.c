// client_lib.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration for simulated kernel IPC
int client_kernel_send_ipc(const void* message, size_t msg_size);
int client_kernel_receive_ipc(void* buffer, size_t buf_size);

// Simulated standard library replacements
int shm_open(const char* path, size_t size) {
    Message msg;
    msg.type = OPEN_FILE;
    strcpy(msg.path, path);
    msg.size = size;

    client_kernel_send_ipc(&msg, sizeof(msg));
    
    int fd;
    client_kernel_receive_ipc(&fd, sizeof(fd));
    return fd;
}

void* mmap(int fd) {
    Message msg;
    msg.type = MMAP_SHM;
    msg.fd = fd;

    client_kernel_send_ipc(&msg, sizeof(msg));

    void* mapped_addr;
    client_kernel_receive_ipc(&mapped_addr, sizeof(mapped_addr));
    return mapped_addr;
}


// Example application (compiled with client_lib.c)
void client_app_main() {
    // Client 1 creates a shared memory block
    printf("Client 1: Creating shared memory /shm/block1\n");
    int shm_fd = shm_open("/shm/block1", 4096);
    if (shm_fd < 0) {
        printf("Client 1: Failed to create shared memory.\n");
        return;
    }

    // Client 1 maps the shared memory
    char* shm_ptr = (char*)mmap(shm_fd);
    if (shm_ptr == (void*)-1) {
        printf("Client 1: Failed to map shared memory.\n");
        return;
    }

    // Client 1 writes data to the shared memory
    strcpy(shm_ptr, "Hello from Client 1!");
    printf("Client 1: Wrote '%s' to shared memory.\n", shm_ptr);

    // Now, imagine Client 2 runs concurrently and maps the same block...
}
