#ifndef __LIBBC_VFS_H__
#define __LIBBC_VFS_H__

enum vfs_msg_types {
    VFS_OPEN =1,
    VFS_READ,
    VFS_WRITE,
    VFS_CREATE,
};


typedef enum {
    VFS_FILE,
    VFS_DIRECTORY,
    VFS_DEVICE,
    VFS_SHARED_MEMORY
} inode_type_t;

struct vfs_msg
{
    int          type;
    uint64_t     pid;
    int          ret;
    inode_type_t inode_type;
    char         path[256];
    int          cap_id;
};


int vfs_open(const char *path);
int vfs_create(const char *path, inode_type_t type, int cap_id);

#endif /* __LIBBC_VFS_H__ */