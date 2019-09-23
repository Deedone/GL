#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/init.h>
#include <linux/io.h>


#define DRV_NAME "ddone_device"

static dma_addr_t mem_base = 0x60000000;
static dma_addr_t reg_base = 0x60001000;

#define MEM_SIZE (4096)
#define REG_SIZE (8)
#define POLL_TIME_MS (500)


#define SIZE_REG_OFFSET (4)
#define FLAG_REG_OFFSET (0)
#define DATA_READY (1)
#define MAX_WORK_THREADS (1)

static void ddone_driver_work(struct work_struct *work);

struct ddone_device {
    void __iomem *mem;
    void __iomem *regs;
    struct delayed_work dwork;
    struct workqueue_struct *data_read_wq;
    u64 poll_time;
};

struct platform_device *pdev;

static u32 ddone_device_read_mem8(struct ddone_device* dev, u32 offset)
{
    return ioread8(dev->mem + offset);
}
static u32 ddone_device_read_reg32(struct ddone_device* dev, u32 offset)
{
    u32 data = ioread32(dev->regs + offset);

    pr_info("Reading reg offset %p data %u", dev->regs + offset, data);
    return data;
}
static void ddone_device_write_reg32(struct ddone_device* dev, u32 offset, u32 val)
{
    iowrite32(val, dev->regs+offset);
}


static int ddone_device_probe(struct platform_device *pdev)
{
    struct device *dev;
    struct ddone_device *my_dev;
    struct resource *res;

    dev = &pdev->dev;
    my_dev = devm_kzalloc(dev, sizeof(struct ddone_device), GFP_KERNEL);
    if (!my_dev) {
	return -ENOMEM;
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    my_dev->mem = devm_ioremap_resource(dev, res);
    if (IS_ERR(my_dev->mem))
	return PTR_ERR(my_dev->mem);

    res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    my_dev->regs = devm_ioremap_resource(dev, res);
    if (IS_ERR(my_dev->regs)) {
	return PTR_ERR(my_dev->regs);
    }


    platform_set_drvdata(pdev, my_dev);
    pr_info("Memory mapped to %p\n", my_dev->regs);
    pr_info("Registers mapped to %p\n", my_dev->mem);

    my_dev->data_read_wq = alloc_workqueue("ddone_driver_read", WQ_UNBOUND, MAX_WORK_THREADS);
    if (!my_dev->data_read_wq)
	return -ENOMEM;

    INIT_DELAYED_WORK(&my_dev->dwork, ddone_driver_work);

    my_dev->poll_time = msecs_to_jiffies(POLL_TIME_MS);

    queue_delayed_work(my_dev->data_read_wq, &my_dev->dwork, 0);

    return 0;

}



static int ddone_device_remove(struct platform_device *pdev)
{

    struct ddone_device *dev = platform_get_drvdata(pdev);

    if (dev->data_read_wq) {
	cancel_delayed_work_sync(&(dev->dwork));
	destroy_workqueue(dev->data_read_wq);
    }

    pr_info("Platform device workqueue stopped");

    return 0;

}



static struct platform_driver ddone_driver = {
    .driver = {
	.name = DRV_NAME,
    },
    .probe = ddone_device_probe,
    .remove = ddone_device_remove,
};

static int ddone_driver_register(void)
{
    int err;

    err = platform_driver_register(&ddone_driver);

    if (err) {
	pr_err("Error registering driver");
	return err;
    } else{
	pr_info("Driver registered\n");
	return 0;
    }
}

static void ddone_driver_unregister(void)
{
    platform_driver_unregister(&ddone_driver);
}

static void ddone_driver_work(struct work_struct* work)
{


    struct ddone_device * my_dev = container_of(work, struct ddone_device, dwork.work);

    u32 flag, size, i;

    flag = ddone_device_read_reg32(my_dev, FLAG_REG_OFFSET);
    if (flag & DATA_READY) {
	size = ddone_device_read_reg32(my_dev, SIZE_REG_OFFSET);
	if (size > MEM_SIZE) {
	    size = MEM_SIZE;
	}

        for (i = 0; i < size; i++) {
	    u8 data = ddone_device_read_mem8(my_dev, i);

            pr_err("MEM[%u] = 0x%x", i, data);
	}

	rmb();
	flag &= ~DATA_READY;
        ddone_device_write_reg32(my_dev, FLAG_REG_OFFSET, flag);


    }


    queue_delayed_work(my_dev->data_read_wq, &my_dev->dwork, my_dev->poll_time);


}


static int __init ddone_device_add(void)
{
    struct resource res[2] = {
	{
	    .start = mem_base,
	    .end = mem_base + MEM_SIZE - 1,
	    .name = "ddone_mem",
	    .flags = IORESOURCE_MEM
        }, {
	    .start = reg_base,
	    .end = reg_base + REG_SIZE - 1,
	    .name = "ddone_regs",
	    .flags = IORESOURCE_MEM

	}
    };
    int err = 0;

    pdev = platform_device_alloc(DRV_NAME, res[0].start);
    if (!pdev) {
	err = -ENOMEM;
	pr_err("Failed to allocate device");
	goto exit;
    }

    err = platform_device_add_resources(pdev, res, ARRAY_SIZE(res));
    if (err) {
	pr_err("Failed to allocate divice resources!");
	goto exit_free;
    }

    err = platform_device_add(pdev);
    if (err) {
	pr_err("Error adding device");
	goto exit_free;
    }


    pr_info("Platform device added\n");

    return 0;
exit_free:
	platform_device_put(pdev);
exit:
	return err;
}



int __init my_init_module(void)
{
    int res = ddone_driver_register();

    res  = ddone_device_add();

    return res;
}

void __exit my_exit_module(void)
{
    platform_device_unregister(pdev);
    ddone_driver_unregister();
}

module_init(my_init_module);
module_exit(my_exit_module);

MODULE_AUTHOR("Mykyta Poturai <mykyta.poturai@globallogic.com>");
MODULE_DESCRIPTION("Learn platform device drivers");
MODULE_LICENSE("Dual BSD/GPL");
