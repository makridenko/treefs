#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

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

void grow_handler(struct work_struct *work);

DECLARE_WORK(grow_struct, grow_handler);

/* Declaration of functions that are part of operations structures */
static int treefs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev);
static int treefs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
static int treefs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
static int treefs_readdir(struct file *filp, struct dir_context *ctx);
static struct dentry *treefs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);

struct leave {
    // Year that leave was created
    int created_at;
    // Leave name (path)
    char *name;
    unsigned long ino;
};

struct branch {
    // Year that branch was created
    int created_at;
    // Branch name (path)
    char *name;
    // Number of leaves on branch
    int num_of_leaves;

    struct leave **leaves;
    struct branch *sub_branches;

    int max_sub_branches;
    int max_sub_branches_for_leaves;
    unsigned long ino;
};

enum treeType {
    birch,
    spruce,
};

struct tree {
    int years;
    int current_year;
    struct branch **branches;
    treeType mode;
    size_t num_of_branches;
};

struct treefs_super_block {
    struct timer_list treefs_timer;
    struct tree tree;
    struct work_struct grow_struct;
    unsigned long next_ino;
};

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


static struct inode *treefs_iget(struct super_block *s, unsigned long ino) {
    struct inode *inode;
    struct branch *branch;

    // Allocate VFS inode
    inode = iget_locked(s, ino);
    if (inode == NULL) return ERR_PTR(-ENOMEM);

    // Return inode from cache
    if (!(inode->i_state & I_NEW)) return inode;

    inode->i_mode =  S_IFDIR|S_IRUGO|S_IXUGO;
    inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
    inode->i_op = &treefs_dir_inode_operations;
    inode->i_fop = &treefs_dir_operations;
    inc_nlink(inode);
    unlock_new_inode(inode);

    inode->i_private = branch;

    return inode;
}


static int treefs_readdir(struct file *filp, struct dir_context *ctx) {
    struct branch *branch;
    loff_t pos = ctx->pos;

    if (pos < 2) {
        if (!dir_emit_dots(filp, ctx)) return 0;
    }

    if (filp->f_inode->i_ino == 1) {
        struct leave *leaf = branch->leaves[pos-2];
        return dir_emit(ctx, leaf->name, strlen(leaf->name), leaf->ino, DT_REG);
    }

    branch = filp->f_inode->i_private;

    struct leave *leaf = branch->leaves[pos - 2];
    return dir_emit(ctx, leaf->name, strlen(leaf->name), leaf->ino, DT_REG);
}


static struct dentry *treefs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags) {
    struct super_block *sb = dir->i_sb;
    struct treefs_dir_entry *de;
    struct buffer_head *bh = NULL;
    struct inode *inode = NULL;

    dentry->d_op = sb->s_root->d_op;

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


void grow_handler(struct work_struct *work) {
    struct tree *tree;
    struct branch *branch;

    int leaves_years_max = 0;

    struct treefs_super_block *sb;
    sb = container_of(work, struct treefs_super_block, grow_struct);

    tree = &sb->tree;

    // New branch init for trunk growing
    branch = kzalloc(sizeof(*branch), GFP_KERNEL);
    branch->created_at = tree->current_year;
    branch->ino = sb->next_ino++;
    branch->max_sub_branches_for_leaves = 5;
    branch->name = kasprintf(GFP_KERNEL, "branch_%lu", branch->ino);

    if (tree->mode == birch) {
        leaves_years_max = 3;
        branch->num_of_leaves = 10;
    } else {
        leaves_years_max = 1;
        branch->num_of_leaves = 100;
    }
}

void grow_callback(struct timer_list *timer) {
    schedule_work(&grow_struct);
}

static int treefs_fill_super(struct super_block *sb, void *data, int silent) {
    struct inode *root_inode;
    struct dentry *root_dentry;
    struct treefs_super_block *tsb;

    // Fill superblock
    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = TREEFS_BLOCKSIZE;
    sb->s_blocksize = TREEFS_BLOCKSIZE_BITS;
    sb->s_magic = TREEFS_MAGIC;
    sb->s_op = &treefs_ops;
    tsb = kmalloc(sizeof(*tsb), GFP_KERNEL);

    // Tree init
    treeType type;
    type = birch;

    tsb->tree.mode = type;
    tsb->tree.years = 10;
    
    tsb->next_ino = 10;

    // FIXME
    timer_setup(&tsb->treefs_timer, &grow_callback, 0);

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