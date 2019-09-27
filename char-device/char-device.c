#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include "device.h"
#include "driver.h"





static int __init chardev_init(void){
	int err;

	err = setup_devices();
	if(err){
		pr_err("Error setting up devices");
		return err;
	}
	err = setup_driver();
	if(err){
		pr_err("Error setting up driver");
		remove_devices();
		return err;
	}

	pr_info("Module inited\n");

	return 0;
}

static void __exit chardev_exit(void){
	remove_driver();
	remove_devices();

}


module_init(chardev_init);
module_exit(chardev_exit);


MODULE_AUTHOR("Mykyta Poturai <mykyta.poturai@globallogic.com");

MODULE_DESCRIPTION("Hello world module");
MODULE_LICENSE("Dual BSD/GPL");

