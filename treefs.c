#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("Alexey Makridenko");
MODULE_LICENSE("GPL");


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