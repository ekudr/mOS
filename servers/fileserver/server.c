// file_server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Assuming microkernel_api.h is available
#include "microkernel_api.h"

#define MAX_FILES 64
#define SHM_DIR_NAME "/shm"

// File system data structures
typedef enum { REGULAR_FILE, SHARED_MEMORY } InodeType;

typedef struct {
    char name[256];
    InodeType type;
    int inode_id;
    mem_obj_handle_t shm_handle; // Kernel handle for shared memory
    void* shm_vaddr;             // Server's own mapping
} Inode;

Inode filesystem[MAX_FILES];
int next_inode_id = 0;

// IPC message format
typedef enum { OPEN_FILE, MMAP_SHM } RequestType;

typedef struct {
    RequestType type;
    char path[256];
    size_t size; // For creating new SHM
    int fd;      // For mmap request
} Message;

// A simple simulated IPC system for this example
void handle_ipc_message(Message* msg, pid_t client_pid) {
    if (msg->type == OPEN_FILE) {
        // Find or create inode
        int fd = -1;
        for (int i = 0; i < next_inode_id; i++) {
            if (strcmp(filesystem[i].name, msg->path) == 0) {
                fd = i;
                break;
            }
        }

        if (fd == -1 && strncmp(msg->path, SHM_DIR_NAME, strlen(SHM_DIR_NAME)) == 0) {
            // It's a request to create a new shared memory object
            if (next_inode_id < MAX_FILES) {
                Inode* new_inode = &filesystem[next_inode_id];
                strcpy(new_inode->name, msg->path);
                new_inode->type = SHARED_MEMORY;
                new_inode->inode_id = next_inode_id;
                new_inode->shm_handle = kernel_mem_alloc(msg->size);
                
                // Server maps the memory for its own use
                new_inode->shm_vaddr = kernel_mem_map(new_inode->shm_handle, getpid());

                fd = next_inode_id;
                next_inode_id++;
                printf("Server: Created new shared memory '%s'\n", new_inode->name);
            }
        }
        
        // Respond to client with file descriptor (fd)
        kernel_send_ipc(client_pid, &fd, sizeof(fd));

    } else if (msg->type == MMAP_SHM) {
        Inode* inode = &filesystem[msg->fd];
        if (inode->type == SHARED_MEMORY) {
            // Get the kernel memory object handle
            mem_obj_handle_t shm_handle = inode->shm_handle;
            
            // Request the kernel to map it for the client
            void* mapped_addr = kernel_mem_map(shm_handle, client_pid);
            
            // Respond to client with the virtual address
            kernel_send_ipc(client_pid, &mapped_addr, sizeof(mapped_addr));
            printf("Server: Mapped shared memory for client %d\n", client_pid);
        }
    }
}

void file_server_main() {
    printf("File server started. Listening for requests...\n");
    Message msg;
    pid_t client_pid;

    while (1) {
        // In a real system, this would block and fill the message struct.
        // For this example, we'll simulate a message coming from a client.
        // kernel_receive_ipc(&msg, sizeof(msg), &client_pid);
    }
}
