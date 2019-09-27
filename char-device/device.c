#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/ioport.h>

#include "device.h"

struct platform_device *__init setup_device(struct resource *res);
void remove_device(struct platform_device *ddev);

static struct platform_device *pdev1;
static struct platform_device *pdev2;
static struct resource res[2][2] = {
	{
		{
			.start = MEM_BASE1,
			.end = MEM_BASE1 + MEM_SIZE - 1,
			.name = "ddone_mem",
			.flags = IORESOURCE_MEM
		}, {
			.start = REG_BASE1,
			.end = REG_BASE1 + REG_SIZE - 1,
			.name = "ddone_regs",
			.flags = IORESOURCE_MEM

		}
	}, {
		{
			.start = MEM_BASE2,
			.end =  MEM_BASE2 + MEM_SIZE - 1,
			.name = "ddone_mem",
			.flags = IORESOURCE_MEM
		}, {
			.start = REG_BASE2,
			.end = REG_BASE2 +  REG_SIZE - 1,
			.name = "ddone_regs",
			.flags = IORESOURCE_MEM

		}
	}
};



int __init setup_devices(void)
{
	pdev1 = setup_device(res[0]);
	if (IS_ERR(pdev1))
		return PTR_ERR(pdev1);
	pdev2 = setup_device(res[1]);
	if (IS_ERR(pdev1)) {
		remove_device(pdev1);
		return PTR_ERR(pdev2);
	}
	return 0;

}
void remove_devices(void)
{
	remove_device(pdev1);
	remove_device(pdev2);
}

struct platform_device *__init setup_device(struct resource *res)
{

	struct platform_device *pdev;
	int err;

	pdev = platform_device_alloc(DEVICE_NAME, res[0].start);
	if (!pdev) {
		err = -ENOMEM;
		goto exit_err;
	}
	err = platform_device_add_resources(pdev, res, 2);
	if (err)
		goto exit_free;


	err = platform_device_add(pdev);
	if (err)
		goto exit_free;



	pr_info("Device set up\n");

	return pdev;

exit_free:
	platform_device_put(pdev);
	pr_err("Error adding device");
exit_err:
	return ERR_PTR(err);

}


void remove_device(struct platform_device *pdev)
{
	platform_device_unregister(pdev);
}
