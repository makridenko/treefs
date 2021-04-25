#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("Alexey Makridenko");
MODULE_LICENSE("GPL");

static int __init exmpl_init(void) {
    pr_debug("Hi\n");
    return 0;
}

static void __exit exmpl_exit(void) {
    pr_debug("Bye\n");
}

module_init(exmpl_init);
module_exit(exmpl_exit);