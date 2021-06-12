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
static int treefs_readdir(struct file *filp, struct dir_context *ctx);
static struct dentry *treefs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
struct inode *treefs_get_inode(struct super_block *sb, const struct inode *dir, int mode);

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

    int leaves_years_max;

    size_t current_num_of_leaves;
    size_t current_num_of_sub_branches;

    struct leave **leaves;
    struct branch **sub_branches;

    int max_sub_branches;
    int max_sub_branches_for_leaves;
    unsigned long ino;
};

enum treeType {
    birch,
    spruce,
};


struct treefs_super_block {
    struct timer_list treefs_timer;
    struct work_struct grow_struct;
    unsigned long next_ino;

    // Tree fields
    int years;
    int current_year;
    struct branch tree;
    enum treeType mode;

    // Current num of branches in the tree
    size_t num_of_branches;
};

static const struct super_operations treefs_ops = {
    .statfs = simple_statfs,
    .drop_inode = generic_drop_inode,
};


static const struct inode_operations treefs_dir_inode_operations = {
    .lookup = simple_lookup,
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

static int treefs_readdir(struct file *filp, struct dir_context *ctx) {
    printk(LOG_LEVEL "readdir start\n");
    struct branch *branch;
    loff_t pos = ctx->pos;

    if (pos < 2) {
        if (!dir_emit_dots(filp, ctx)) return 0;
    }

    branch = filp->f_inode->i_private;

    if (filp->f_inode->i_ino == 1) {
        struct leave *leaf = branch->leaves[pos-2];
        return dir_emit(ctx, leaf->name, strlen(leaf->name), leaf->ino, DT_REG);
    }

    struct leave *leaf = branch->leaves[pos - 2];
    printk(LOG_LEVEL "readdir end\n");
    return dir_emit(ctx, leaf->name, strlen(leaf->name), leaf->ino, DT_REG);
}


static struct dentry *treefs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags) {
    printk(LOG_LEVEL "lookup start\n");

    struct super_block *sb = dir->i_sb;
    struct treefs_dir_entry *de;
    struct inode *inode = NULL;
    struct branch *branch = dir->i_private;

    int i;

    for (i=0; i < branch->current_num_of_sub_branches; i++) {
        if (branch->sub_branches[i]->name == dentry->d_name.name) {
            inode = treefs_get_inode(sb, dir, S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
            inode->i_ino = branch->sub_branches[i]->ino;
            inode->i_private = branch->sub_branches[i];
            d_add(dentry, inode);

            printk(LOG_LEVEL "lookup end with dir\n");
            return NULL;
        }
    }

    for (i=0; i < branch->current_num_of_leaves; i++) {
        if (branch->sub_branches[i]->name == dentry->d_name.name) {
            inode = treefs_get_inode(sb, dir, S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            inode->i_ino = branch->leaves[i]->ino;
            inode->i_private = branch->leaves[i];
            d_add(dentry, inode);
            printk(LOG_LEVEL "lookup end with file\n");
            return NULL;
        }
    }


    printk(LOG_LEVEL "lookup end\n");
    return ERR_PTR(-ENOENT);
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
    // inode->i_ino = 1;

    // Init i_ino using get_next_ino
    // inode->i_ino = get_next_ino();

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

int fib(int num) {
    if (num == -1) return 0;
    if (num == 0) return 0;
    if (num == 1) return 1;
    return fib(num - 1) + fib(num -2);
}

bool is_young(struct branch *b, int current_year) {
    if (current_year - b->created_at < b->leaves_years_max) 
        return true;
    return false;
}

void grow_leaf(struct treefs_super_block *tsb, struct branch *b) {
    struct leave *leaf;
    struct leave **new_leafs;
    struct leave **tmp_leafs;

    if (b->max_sub_branches == b->max_sub_branches_for_leaves) {
        return;
    }

    // New leaf init for growing
    leaf = kzalloc(sizeof(*leaf), GFP_KERNEL);
    leaf->created_at = tsb->current_year;
    leaf->ino = tsb->next_ino++;

    new_leafs = kzalloc(sizeof(b->leaves) * (b->current_num_of_leaves+1), GFP_KERNEL);
    memcpy(new_leafs, b->leaves, sizeof(new_leafs[0])*b->current_num_of_leaves);
    new_leafs[b->num_of_leaves++] = leaf;
    tmp_leafs = b->leaves;
    b->leaves = new_leafs;
    kvfree(tmp_leafs);

}

void grow_branch(struct treefs_super_block *tsb, struct branch *tree) {
    printk(LOG_LEVEL "start grow_branch\n");
    struct branch *branch;
    struct branch **tmp_branches;
    struct branch **new_branches;
    int i, j;

    // ISO C90 warning
    int fib_num = fib(tsb->current_year);

    // New branch init for trunk growing
    branch = kzalloc(sizeof(*branch), GFP_KERNEL);
    branch->created_at = tsb->current_year;
    branch->ino = tsb->next_ino++;
    branch->max_sub_branches_for_leaves = 5;
    branch->name = kasprintf(GFP_KERNEL, "branch_%lu", branch->ino);

    if (tsb->mode == birch) {
        branch->leaves_years_max = 3;
        branch->num_of_leaves = 10;
    } else {
        branch->leaves_years_max = 1;
        branch->num_of_leaves = 100;
    }

    printk(LOG_LEVEL "new branch\n");

    new_branches = kzalloc(sizeof(tree->sub_branches[0]) * (tree->current_num_of_sub_branches+1), GFP_KERNEL);
    memcpy(new_branches, tree->sub_branches, sizeof(new_branches[0])*tree->current_num_of_sub_branches);
    new_branches[tree->current_num_of_sub_branches++] = branch;
    tmp_branches = tree->sub_branches;
    tree->sub_branches = new_branches;
    kvfree(tmp_branches);

    printk(LOG_LEVEL "cycle\n");
    // Идем по массиву веток и вызываем grow_branch
    for (i=0; i < branch->current_num_of_sub_branches; i++) {
        if (is_young(branch, tsb->current_year)) {
            for (j=0; j < fib_num; j++) {
                grow_branch(tsb, branch);
            }
            grow_leaf(tsb, tree->sub_branches[i]);
        }
    }

    mod_timer(&tsb->treefs_timer, jiffies + msecs_to_jiffies(10000));
    printk(LOG_LEVEL "end grow_branch\n");
}


void grow_handler(struct work_struct *work) {
    struct treefs_super_block *sb;
    sb = container_of(work, struct treefs_super_block, grow_struct);
    sb->current_year++;
    grow_branch(sb, &sb->tree);
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
    tsb = kzalloc(sizeof(*tsb), GFP_KERNEL);

    // Tree init
    enum treeType type = birch;

    tsb->mode = type;
    tsb->years = 10;
    
    tsb->next_ino = 10;

    // FIXME
    timer_setup(&tsb->treefs_timer, &grow_callback, 0);

    // Mode - directory and access rights
    root_inode = treefs_get_inode(
        sb, NULL,
        S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
    );
    root_inode->i_ino = 1;
    tsb->tree.ino = 1;
    root_inode->i_private = &tsb->tree;

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

    mod_timer(&tsb->treefs_timer, jiffies + msecs_to_jiffies(10000));
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