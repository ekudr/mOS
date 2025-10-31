


typedef struct inode {
    // Standard inode metadata
    uint64_t id;                        // Unique identifier for the inode

    inode_type_t type;                // File, directory, or shared memory block
    int permissions;               // Permissions (e.g., read, write)
    size_t size;                   // Size in bytes
    unsigned int link_count;       // Number of hard links pointing to this inode
    char name[256];               // Name of the file/directory

    // Pointers for the list-linked tree structure
    list_head_t parent;          // List in the parent directory
    list_head_t children; // List of the entries in this directory
//    list_head_t siblings;    // Pointer to the next entry in the same directory
    
    // Data storage union
    union {
        // For files and shared memory
        struct {
            // For regular files, this points to a data buffer in memory
            void* data_buffer;
            // For shared memory, this holds the kernel object handle
            int kernel_shm_handle;
        } file_data;

        struct {
            int cap_id;
        } dev_data;
        // For directories 
        struct {
            int hz;
        } dir_data;
    };
} inode_t;