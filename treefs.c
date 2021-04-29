#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("Alexey Makridenko");
MODULE_LICENSE("GPL");

#define TREEFS_BLOCKSIZE 4096
#define TREEFS_BLOCKSIZE_BITS 12
#define TREEFS_MAGIC 0xbeefcafe
#define LOG_LEVEL KERN_ALERT


static int treefs_fill_super(struct super_block *sb, void *data, int silent) {
    struct inode *root_inode;
    struct dentry *root_dentry;

    // Fill superblock
    sb -> s_maxbytes = MAX_LFS_FILESIZE;
    sb -> s_blocksize = TREEFS_BLOCKSIZE;
    sb -> s_blocksize = TREEFS_BLOCKSIZE_BITS;
    sb -> s_magic = TREEFS_MAGIC;
    sb -> s_op = &myfs_ops;

    // Mode - directory and access rights
    root_inode = treefs_get_inode(
        sb, NULL,
        S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
    );

    printk(LOG_LEVEL "root inode has %d link(s)\n", root_inode -> i_nlink);

    if (!root_inode) {
        return _ENOMEM;
    }

    root_dentry = d_make_root(root_inode);
    if (!root_dentry) {
        iput(root_inode);
        return _ENOMEM;
    }

    sb -> s_root = root_dentry;
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

    err = register_filesyatem(&treefs_type);
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