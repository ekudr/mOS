// Forward declaration for mutual recursion
struct inode;

typedef enum {
    INODE_TYPE_REGULAR_FILE,
    INODE_TYPE_DIRECTORY,
    INODE_TYPE_SHARED_MEMORY
} InodeType;

typedef struct inode {
    // Standard inode metadata
    int id;                        // Unique identifier for the inode
    InodeType type;                // File, directory, or shared memory block
    int permissions;               // Permissions (e.g., read, write)
    size_t size;                   // Size in bytes
    unsigned int link_count;       // Number of hard links pointing to this inode
    char name[256];               // Name of the file/directory

    // Pointers for the list-linked tree structure
    list_head_t parent;          // Pointer to the parent directory
    list_head_t siblings;    // Pointer to the next entry in the same directory
    
    // Data storage union
    union {
        // For files and shared memory
        struct {
            // For regular files, this points to a data buffer in memory
            void* data_buffer;
            // For shared memory, this holds the kernel object handle
            int kernel_shm_handle;
        } file_data;

        // For directories ??? 
        struct {
            list_head_t children; // Pointer to the first entry in this directory
        } dir_data;
    };
} inode;
