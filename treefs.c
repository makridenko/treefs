#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("Alexey Makridenko");
MODULE_LICENSE("GPL");

#define TREEFS_BLOCKSIZE 4096
#define TREEFS_BLOCKSIZE_BITS 12
#define TREEFS_MAGIC 0xbeefcafe
#define LOG_LEVEL KERN_ALERT
#define TREEFS_NAME_LEN 16
#define TREEFS_INODE_BLOCK 1
#define TREEFS_NUM_ENTRIES 32

/* Declaration of functions that are part of operations structures */
static int treefs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev);
static int treefs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
static int treefs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
static int treefs_readdir(struct file *filp, struct dir_context *ctx);
static struct dentry *treefs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);

static const struct super_operations treefs_ops = {
    .statfs = simple_statfs,
    .drop_inode = generic_drop_inode,
};


static const struct inode_operations treefs_dir_inode_operations = {
    .create = treefs_create,
    .lookup = simple_lookup,
    .mkdir = treefs_mkdir,
    .rmdir = simple_rmdir,
    .rename = simple_rename,
};


static const struct file_operations treefs_file_operations = {
    .read_iter = generic_file_read_iter,
    .write_iter = generic_file_write_iter,
    .mmap = generic_file_mmap,
    .llseek = generic_file_llseek,
    .iterate = treefs_readdir,
};

static const struct file_operations treefs_dir_operations = {
    .read = generic_read_dir,
    .iterate = treefs_readdir,
};

static const struct inode_operations treefs_file_inode_operations = {
    .getattr = simple_getattr,
    .lookup = treefs_lookup,
};


static const struct address_space_operations treefs_aops = {
    .readpage = simple_readpage,
    .write_begin = simple_write_begin,
    .write_end = simple_write_end,
};

struct treefs_inode {
    __u32 mode;
    __u32 uid;
    __u32 gid;
    __u32 size;
    __u32 data_block;
};

struct treefs_inode_info {
    __u16 data_block;
    struct inode vfs_inode;
};

struct treefs_dir_entry {
    __u32 ino;
    char name[TREEFS_NAME_LEN];
};

static struct inode *treefs_iget(struct super_block *s, unsigned long ino) {
    struct treefs_inode *mi;
    struct buffer_head *bh;
    struct inode *inode;
    struct treefs_inode_info *mii;

    // Allocate VFS inode
    inode = iget_locked(s, ino);
    if (inode == NULL) return ERR_PTR(-ENOMEM);

    // Return inode from cache
    if (!(inode->i_state & I_NEW)) return inode;

    if (!(bh = sb_bread(s, TREEFS_INODE_BLOCK))) {
        iget_failed(inode);
        return NULL;
    }

    // Get inode with index ino from the block
    mi = ((struct treefs_inode *) bh->b_data) + ino;

    // Fill VFS inode
    inode->i_mode = mi->mode;
    i_uid_write(inode, mi->uid);
    i_gid_write(inode, mi->gid);
    inode->i_size = mi->size;
    inode->i_blocks = 0;
    inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);

    // Fill address space operations (inode->i_mapping->a_ops)
    inode->i_mapping->a_ops = &treefs_aops;

    if (S_ISDIR(inode->i_mode)) {
        inode->i_op = &simple_dir_inode_operations;
        inode->i_fop = &simple_dir_operations;

        inode->i_op = &treefs_dir_inode_operations;
        inode->i_fop = &treefs_dir_operations;

        inc_nlink(inode);
    }

    if (S_ISREG(inode->i_mode)) {
        inode->i_op = &treefs_file_inode_operations;
        inode->i_fop = &treefs_file_operations;
    }

    mii = container_of(inode, struct treefs_inode_info, vfs_inode);

    mii->data_block = mi->data_block;

    brelse(bh);
    unlock_new_inode(inode);
    return inode;
}


static int treefs_readdir(struct file *filp, struct dir_context *ctx) {
    struct buffer_head *bh;
    struct treefs_dir_entry *de;
    struct treefs_inode_info *mii;
    struct inode *inode;
    struct super_block *sb;
    int over;
    int err = 0;

    // Get inode of directory and container inode
    inode = file_inode(filp);
    mii = container_of(inode, struct treefs_inode_info, vfs_inode);

    // Get superblock from inode (i_sb)
    sb = inode->i_sb;

    // Read data block for directory inode
    bh = sb_bread(sb, mii->data_block);
    if (bh == NULL) {
        printk(LOG_LEVEL "Error: could not read block\n");
        err = -ENOMEM;
        return err;
    }

    for (; ctx->pos < TREEFS_NUM_ENTRIES; ctx->pos++) {
        // Data block contains an array of "struct treefs_dir_entry"
        de = (struct treefs_dir_entry *) bh->b_data + ctx->pos;

        // Step over empty entries
        if (de->ino == 0) continue;
    }

    over = dir_emit(ctx, de->name, TREEFS_NAME_LEN, de->ino, DT_UNKNOWN);
    if (over) {
        ctx->pos++;
        brelse(bh);
    }

    return err;
}

static struct treefs_dir_entry *treefs_find_entry(struct dentry *dentry, struct buffer_head **bhp) {
    struct buffer_head *bh;
    struct inode *dir = dentry->d_parent->d_inode;
    struct treefs_inode_info *mii = container_of(dir, struct treefs_inode_info, vfs_inode);
    struct super_block *sb = dir->i_sb;
    const char *name = dentry->d_name.name;
    struct treefs_dir_entry *final_de = NULL;
    struct treefs_dir_entry *de;
    int i;

    bh = sb_bread(sb, mii->data_block);
    if (bh == NULL) return NULL;

    *bhp = bh;

    for (i = 0; i < TREEFS_NUM_ENTRIES; i++) {
        de = ((struct treefs_dir_entry *) bh->b_data) + i;
        if (de->ino != 0) {
            if (strcmp(name, de->name) == 0) {
                final_de = de;
                break;
            }
        }
    }

    return final_de;
}

static struct dentry *treefs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags) {
    struct super_block *sb = dir->i_sb;
    struct treefs_dir_entry *de;
    struct buffer_head *bh = NULL;
    struct inode *inode = NULL;

    dentry->d_op = sb->s_root->d_op;

    de = treefs_find_entry(dentry, &bh);
    if (de != NULL) {
        inode = treefs_iget(sb, de->ino);
        if (IS_ERR(inode)) return ERR_CAST(inode);
    }

    d_add(dentry, inode);
    brelse(bh);

    return NULL;
}


struct inode *treefs_get_inode(
    struct super_block *sb, const struct inode *dir, int mode
) {
    struct inode *inode = new_inode(sb);

    if (!inode) {
        return NULL;
    }

    // Fill inode struct
    inode_init_owner(inode, dir, mode);
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    inode->i_ino = 1;

    // Init i_ino using get_next_ino
    inode->i_ino = get_next_ino();

    // Init address space operations
    inode->i_mapping->a_ops = &treefs_aops;

    if (S_ISDIR(mode)) {
        // set inode operations for dir inodes
        inode->i_op = &simple_dir_inode_operations;
        inode->i_fop = &simple_dir_operations;
        inode->i_op = &treefs_dir_inode_operations;

        inc_nlink(inode);
    }

    if (S_ISREG(mode)) {
        inode->i_op = &treefs_file_inode_operations;
        inode->i_fop = &treefs_file_operations;
    }

    return inode;
}


static int treefs_mknod(
    struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev
) {
    struct inode *inode = treefs_get_inode(dir->i_sb, dir, mode);

    if (inode == NULL) {
        return -ENOSPC;
    }

    d_instantiate(dentry, inode);
    dget(dentry);
    dir->i_mtime = dir->i_ctime = current_time(inode);

    return 0;
}


static int treefs_create(
    struct inode *dir, struct dentry *dentry, umode_t mode, bool excl
) {
    return treefs_mknod(dir, dentry, mode | S_IFREG, 0);
}


static int treefs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode) {
    int ret;

    ret = treefs_mknod(dir, dentry, mode | S_IFDIR, 0);
    if (ret != 0) {
        return ret;
    }

    inc_nlink(dir);

    return 0;
}


static int treefs_fill_super(struct super_block *sb, void *data, int silent) {
    struct inode *root_inode;
    struct dentry *root_dentry;

    // Fill superblock
    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = TREEFS_BLOCKSIZE;
    sb->s_blocksize = TREEFS_BLOCKSIZE_BITS;
    sb->s_magic = TREEFS_MAGIC;
    sb->s_op = &treefs_ops;

    // Mode - directory and access rights
    root_inode = treefs_get_inode(
        sb, NULL,
        S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
    );

    printk(LOG_LEVEL "root inode has %d link(s)\n", root_inode->i_nlink);

    if (!root_inode) {
        return -ENOMEM;
    }

    root_dentry = d_make_root(root_inode);
    if (!root_dentry) {
        iput(root_inode);
        return -ENOMEM;
    }

    sb->s_root = root_dentry;
    return 0;
}


static struct dentry *treefs_mount(
    struct file_system_type *fs_type, int flags, const char *dev_name,
    void *data
) {
    // Call superblock mount function
    return mount_nodev(fs_type, flags, data, treefs_fill_super);
}


/* Define file_system_type structure */
static struct file_system_type treefs_type = {
    .name = "treefs",
    .mount = treefs_mount,
};


static int __init treefs_init(void) {
    int err;

    err = register_filesystem(&treefs_type);
    if (err) {
        printk(LOG_LEVEL "register_filesystem failed\n");
        return err;
    }

    return 0;
}

static void __exit treefs_exit(void) {
    unregister_filesystem(&treefs_type);
}


module_init(treefs_init);
module_exit(treefs_exit);