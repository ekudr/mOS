#include <mosstd.h>
#include <cap.h>
#include <libsys/ipc.h>
#include <libsys/list.h>
#include <string.h>

#include <vfs.h>

#include "inode.h"



int vfs_cap;

// Global root directory
struct inode* root;
uint64_t next_node_id = 1;

static void init_server()
{
    // Create Endpoint for VFS
    vfs_cap = create_capability(CAP_ENDPOINT, CRIGHT_RCV | CRIGHT_SND | CRIGHT_GRANT);
    if (vfs_cap < 0) {
        debug("PANIC VFS");
        for (;;);
    }     
    int ret = ns_register_cap(vfs_cap, "vfs", CRIGHT_SND | CRIGHT_GRANT);
    if (ret < 0) {
        debug("PANIC VFS");
        for (;;);
    }   
}

static inline uint64_t alloc_node_id()
{
    return __atomic_fetch_add(&next_node_id, 1, __ATOMIC_ACQ_REL);
}

static inode_t *inode_create(const char*name, inode_type_t type, int cap_id)
{
    if (!name) return NULL;
    
    inode_t *node = malloc(sizeof(inode_t));
       
    memset(node, 0, sizeof(*node));
    node->id = alloc_node_id();    
    strncpy(node->name, name, sizeof(node->name)-1); 
    node->type = type;
    list_init(&node->parent);
    list_init(&node->children);
    
    if (type == VFS_DEVICE) {
        if (!cap_id)
        {
            free(node);
            return NULL;
        }
        
        node->dev_data.cap_id = cap_id;
    }

    return node;
}

static kerrno_t inode_add(inode_t *parent, inode_t *child)
{
    
    if (!parent || !child) return -EINVAL;

    list_add(&parent->children, &child->parent);
    return SUCCESS;
}

static inode_t *vfs_lookup_path(inode_t *root, const char *path)
{
    if (!root || !path || path[0] != '/') return NULL;
    
    inode_t *cur = root;
    char component[32];
    int i = 1; // Skip initial '/'

    while (path[i])
    {
        int j = 0;
        while (path[i] && path[i] != '/' && j < sizeof(component)-1)
            component[j++] = path[i++];
        component[j] = '\0';
        if (path[i] == '/') i++;
        
        // search current children
        inode_t *next = NULL, *pos;
//        debug("[VFS] search %s\n", component);
        list_for_each_entry(pos, &cur->children, parent) {
            if (strcmp(pos->name, component) == 0) {
//                debug("[VFS] search node name %s\n", pos->name);
                next = pos;
                break;
            }                
        }

        if (!next) return NULL;
        cur = next;
    }
    return cur;
}

static inode_t *vfs_create_node(inode_t *root, const char *parent_path, const char *name,
                        inode_type_t type, int cap_id)
{
//    debug("[VFS] parent path %s\n", parent_path);
    inode_t *parent = vfs_lookup_path(root, parent_path);
//    debug("Found parent node 0x%lX\n", parent);
    if (!parent || parent->type != VFS_DIRECTORY) return NULL;

    inode_t *node = inode_create(name, type, cap_id);
    
    if (inode_add(parent, node) < 0) {
        free(node);
        return NULL;
    }
    return node;
}

void init_vfs()
{
    // Allocate memory for the root inode and directory
    root = inode_create("/", VFS_DIRECTORY, 0);
    inode_t *dev = inode_create("dev", VFS_DIRECTORY, 0);
    inode_add(root, dev);
}


int main()
{
    debug("VFS v.0.0.1 starting...\n");
    init_server();
    init_vfs();

    

    while (1)
    {
        inode_t *node;
        struct vfs_msg *msg = (struct vfs_msg *)get_ipc_buffer()->msg;
        uint64_t info = ipc_recv(vfs_cap, NULL);
        int ret = (int)label_from_msginfo_word(info);
        if (ret < 0 || !length_from_msginfo_word(info)) {
            debug("[VFS] Error receiving message %d info 0x%lX\n", ret, info);
        }

        switch (msg->type)
        {
        case VFS_OPEN:
//            debug("[VFS] open path %s\n", msg.path);
            node = vfs_lookup_path(root, msg->path);
            
            if (!node) {
                memset(msg, 0, sizeof(struct vfs_msg));
                msg->ret = -ENOENT;
            }
            else if (node->type == VFS_DEVICE) {
                    int g_cap = cap_grant(node->dev_data.cap_id, msg->pid, -1, CRIGHT_SND);
                    memset(msg, 0, sizeof(struct vfs_msg));
                    msg->ret = g_cap;
            } else {
                memset(msg, 0, sizeof(struct vfs_msg));
                msg->ret = node->id;
            }
                
            info = msginfo_word_new(0,sizeof(struct vfs_msg)/8, 0, 0);
            ipc_reply(info);
            break;
        
        case VFS_CREATE:
//            debug("[VFS] create path %s\n", msg->path);
             inode_t *exist = vfs_lookup_path(root, msg->path);
             if (exist) {
                memset(msg, 0, sizeof(struct vfs_msg));
                msg->ret = -EEXIST;
                info = msginfo_word_new(0,sizeof(struct vfs_msg)/8, 0, 0);
                ipc_reply(info);
            }
            const char *full = msg->path;
            const char *lastslash = strrchr(full, '/');
            if (!lastslash) {
                memset(msg, 0, sizeof(struct vfs_msg));
                msg->ret = -EINVAL;
                info = msginfo_word_new(0,sizeof(struct vfs_msg)/8, 0, 0);
                ipc_reply(info);
                break;
            }
            char parent_path[128];
            char name[32];
            size_t parent_len = lastslash - full;
            if (parent_len >= sizeof(parent_path)) parent_len = sizeof(parent_path)-1;
            strncpy(parent_path, full, parent_len);
            parent_path[parent_len] = '\0';
            strncpy(name, lastslash+1, sizeof(name)-1);
            name[sizeof(name)-1] = '\0'; 
            
            node = vfs_create_node(root, parent_path, name, msg->inode_type, msg->cap_id);

            memset(msg, 0, sizeof(struct vfs_msg)); 
            if (!node) {
                msg->ret = -EINVAL;
            } else {                
                msg->ret = node->id;
            }
            
            info = msginfo_word_new(0,sizeof(struct vfs_msg)/8, 0, 0);
            ipc_reply(info);
            break;

        default:
            break;
        }
    }
    
    return 0;
}