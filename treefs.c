#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("Alexey Makridenko");
MODULE_LICENSE("GPL");


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