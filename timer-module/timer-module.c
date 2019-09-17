#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/timer.h>



MODULE_AUTHOR("Mykyta Poturai <mykyta.poturai@globallogic.com");
MODULE_DESCRIPTION("Timer testing module");
MODULE_LICENSE("Dual BSD/GPL");


static unsigned long delay ;
static struct timer_list* timer;

void timer_fn(struct timer_list* arg){

    pr_err("Timer fired");

    mod_timer(arg,jiffies+delay);
}

static int __init hello_init(void)
{
    delay = msecs_to_jiffies(1000);
    
    timer = (struct timer_list*)kmalloc(sizeof(struct timer_list),GFP_KERNEL);
    timer->expires = jiffies+delay;


    timer_setup(timer,timer_fn,0);
    add_timer(timer);


    return 0;
}

static void __exit hello_exit(void)
{
    /* Do nothing here right now */
    del_timer(timer);
    kfree(timer);
}

module_init(hello_init);
module_exit(hello_exit);
