#include <linux/module.h>
#include <linux/init.h>

static int __init my_init(void) {
    printk(KERN_INFO "Hello good morning module loaded\n");
    return 0;
}

static void __exit my_exit(void) {
    printk(KERN_INFO "Hello good night module unloaded\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak");
MODULE_DESCRIPTION("Simple Hello Module");
