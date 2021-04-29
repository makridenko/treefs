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


static const struct super_operations treefs_ops = {
    .statfs = simple_statfs,
    .drop_inode = generic_drop_inode,
};


static const struct inode_operations treefs_fir_inode_operations = {
    .create = treefs_create,
    .lookup = simple_lookup,
    .mkdir = treefs_mkdir,
    .rmdir = simple_rmdir,
    .rename = simple_rename,
};


static const struct file_operations = treefs_file_operations = {
    .read_iter = generic_file_read_iter,
    .write_iter = generic_file_write_iter,
    .mmap = generic_file_mmap,
    .llseek = generic_file_llseek,
};


static const struct inode_operations treefs_file_inode_operations = {
    .getattr = simple_getattr,
};


static const struct address_space_operations treefs_aops = {
    .readpage = simple_readpage,
    .write_begin = simple_write_begin,
    .write_end = simple_write_end,
};


struct inode *trefs_get_inode(
    struct superblock *sb, const struct inode *dir, int mode
) {
    struct inode *inode = new_inode(sb);

    if (!inode) {
        return NULL;
    }

    // Fill inode struct
    inode_init_owner(inode, dir, mode);
    inode -> i_atime = inode -> i_mtime = inode -> i_ctime = current_time(inode);
    inode -> i_ino = 1;

    // Init i_ino using get_next_ino
    inode -> i_ino = get_next_ino();

    // Init address space operations
    inode -> i_mapping -> a_ops = &myfs_aops;

    if (S_ISDIR(mode)) {
        // set inode operations for dir inodes
        inode -> i_op = &simple_dir_inode_operations;
        inode -> i_fop = &simple_dir_operations;
        inode -> i_op = &treefs_dir_inode_operations;

        inc_nlink(inode);
    }

    if (S_ISREG(mode)) {
        inode -> i_op = &treefs_file_inode_operations;
        inode -> i_fop = &treefs_file_operations;
    }

    return inode;
} 

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