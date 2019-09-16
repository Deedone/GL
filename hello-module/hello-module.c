#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

MODULE_AUTHOR("Mykyta Poturai <mykyta.poturai@globallogic.com");

MODULE_DESCRIPTION("Hello world module");
MODULE_LICENSE("Dual BSD/GPL");

static int __init hello_init(void)
{
    printk(KERN_INFO "Hello world\n");
    pr_err("Test no newline");
    pr_err("Test newline\n");
    return 0;
}

static void __exit hello_exit(void)
{
    /* Do nothing here right now */
}

module_init(hello_init);
module_exit(hello_exit);
