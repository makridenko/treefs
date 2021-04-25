static int __init treefs_init(void) {
    return register_filesystem(&treefs_type);
}

static void __exit treefs_exit(void) {
    unregister_filesystem(&treefs_type);
}

module_init(treefs_init);
module_exit(treefs_exit);