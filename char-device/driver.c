#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include "driver.h"

unsigned int DEV_MAJOR;
unsigned int DEV_MINOR;

static ssize_t chardev_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos);


static int     chardev_open(struct inode *inode, struct file *filep);
static ssize_t chardev_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos);
static ssize_t chardev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos);
static long    chardev_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg);


static int  device_remove(struct platform_device *pdev);
static int  device_probe(struct platform_device *pdev);
static void device_work_f(struct work_struct *work);
static void device_try_write_to(struct ddone_device *ddev);
static void device_try_read_from(struct ddone_device *ddev);

static int  ring_write(struct ddone_device *ddev, char **buf, size_t *count);
static void ring_read(struct ddone_device *ddev, char **buf, size_t *count);

static u32  ddone_device_read_mem8(struct ddone_device *dev, u32 offset);
static u32  ddone_device_read_reg32(struct ddone_device *dev, u32 offset);
static void ddone_device_write_mem8(struct ddone_device *dev, u32 offset,
		u8 val);
static void ddone_device_write_reg32(struct ddone_device *dev, u32 offset,
		u32 val);


static struct platform_driver ddone_driver = {
	.driver = {
		.name = DEVICE_NAME,
	},
	.probe = device_probe,
	.remove = device_remove
};

static const struct file_operations fops = {
	.owner	= THIS_MODULE,
	.read	= chardev_read,
	.write	= chardev_write,
	.open	= chardev_open,
	.unlocked_ioctl  = chardev_ioctl
};

static u32 ddone_device_read_mem8(struct ddone_device *dev, u32 offset)
{
	return ioread8(dev->mem + offset);
}
static u32 ddone_device_read_reg32(struct ddone_device *dev, u32 offset)
{
	return ioread32(dev->regs + offset);
}
static void ddone_device_write_mem8(struct ddone_device *dev, u32 offset,
		u8 val)
{
	iowrite8(val, dev->mem+offset);
}
static void ddone_device_write_reg32(struct ddone_device *dev, u32 offset,
		u32 val)
{
	iowrite32(val, dev->regs+offset);
}

static int ring_write(struct ddone_device *ddev, char **buf, size_t *count)
{
	int oldw;
	size_t end;
	int err;

	while ((ddev->w + 1) % BUF_SIZE == ddev->r) {//If buffer full
		mutex_unlock(&ddev->mutex);
		err = wait_event_interruptible(ddev->wq,
				(ddev->w + 1) % BUF_SIZE != ddev->r);
		if (err) {
			*count = 0;
			mutex_lock(&ddev->mutex);
			return -EFBIG;
		}
		mutex_lock(&ddev->mutex);
	}

	oldw = ddev->w;
	//Here we can write safely to r - 1
	ddev->w = (ddev->w + 1) % BUF_SIZE;
	if (ddev->r > ddev->w)
		end = ddev->r;
	else
		end = BUF_SIZE;

	if (*count >= (end - ddev->w))
		*count = end - ddev->w;

	*buf = ddev->data + ddev->w;


	ddev->w = (ddev->w + *count - 1) % BUF_SIZE;

	pr_info("W %d => %d      %lu\n", oldw, ddev->w, *count);
	return 0;
}

static ssize_t chardev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct ddone_device *ddev;
	char *start;
	int err;

	ddev = filp->private_data;

	mutex_lock(&ddev->mutex);

	err = ring_write(ddev, &start, &count);
	if (err)
		goto err_write;

	if (copy_from_user(start, buf, count))
		goto err_write;
	*f_pos += count;

	ddev->bytes_to_write += count;
	mutex_unlock(&ddev->mutex);
	wake_up_interruptible(&ddev->rq);//Notify readers

	return count;

err_write:
	mutex_unlock(&ddev->mutex);

	return err;

}

static void ring_read(struct ddone_device *ddev, char **buf, size_t *count)
{
	size_t end;
	int oldr, err;

	while (ddev->r == ddev->w) {//Wait for new data
		mutex_unlock(&ddev->mutex);
		err = wait_event_interruptible(ddev->rq, ddev->r != ddev->w);
		if (err) {
			*count = 0;
			//To prevent mutex unlock errors later
			mutex_lock(&ddev->mutex);
			return;
		}

		mutex_lock(&ddev->mutex);

	}

	oldr = ddev->r;
	//We can safely read here
	ddev->r = (ddev->r + 1) % BUF_SIZE;
	if (ddev->w >= ddev->r)
		end = ddev->w;
	else
		end = BUF_SIZE - 1;

	if (*count > end - ddev->r)
		*count = end - ddev->r;

	*buf = ddev->data + ddev->r;

	ddev->r = (ddev->r + *count) % BUF_SIZE;

	*count = *count + 1;
	pr_info("R %d => %d   %lu\n", oldr, ddev->r, *count);
}

static ssize_t chardev_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	struct ddone_device *ddev;
	char *start;
	int err;

	ddev = filp->private_data;

	mutex_lock(&ddev->mutex);

	while (ddev->bytes_to_write != 0) {
		pr_err("Device is busy\n");
		mutex_unlock(&ddev->mutex);
		err = wait_event_interruptible(ddev->rq,
				ddev->bytes_to_write == 0);
		mutex_lock(&ddev->mutex);
		if (err) {
			err = 0;//Return 0 count to indicate end of stream
			goto stop;
		}
	}
	ring_read(ddev, &start, &count);

	if (count != 0) {
		if (copy_to_user(buf, start, count)) {
			err = -ENOMEM;
			goto stop;
		}
	}
	*f_pos += count;




	mutex_unlock(&ddev->mutex);
	wake_up_interruptible(&ddev->wq);//Notify writers
	return count;
stop:
	mutex_unlock(&ddev->mutex);
	return err;

}

static long chardev_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct ddone_device *ddev = filp->private_data;

	if (_IOC_TYPE(cmd) != DDONE_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
	case DDONE_SET_POLL:
		if (arg > MAX_POLL_INTERVAL || arg < MIN_POLL_INTERVAL)
			return -EINVAL;
		ddev->poll_time = msecs_to_jiffies(arg);
		pr_info("Poll interval set to %lu\n", arg);
		break;
	default: return -ENOTTY;
	}


	return 0;
}

static int chardev_open(struct inode *inode, struct file *filep)
{
	struct ddone_device *ddev;

	ddev = container_of(inode->i_cdev, struct ddone_device, cdev);
	filep->private_data = ddev;


	pr_info("Chardev open\n");



	return 0;
}

static void device_try_write_to(struct ddone_device *ddev)
{
	u32 flags;
	u8 data;
	size_t size;
	char *start;
	int i;

	size = MEM_SIZE;


	flags = ddone_device_read_reg32(ddev, FLAGS_REG);


	if (flags & DATA_READY)
		return;

	ring_read(ddev, &start, &size);

	for (i = 0; i < size; i++) {
		data = (u8)start[i];
		ddone_device_write_mem8(ddev, i, data);

	}
	ddev->bytes_to_write -= size;

	//We transfered all data
	ddone_device_write_reg32(ddev, FLAGS_REG, flags | DATA_READY);
	ddone_device_write_reg32(ddev, SIZE_REG, size);

	wake_up_interruptible(&ddev->wq);//Notify writers

}

static void device_try_read_from(struct ddone_device *ddev)
{
	u32 flags;
	u8 data;
	size_t size, orig_size;
	char *start;
	int i;



	flags = ddone_device_read_reg32(ddev, FLAGS_REG);
	size = (size_t)ddone_device_read_reg32(ddev, SIZE_REG);


	if (!(flags & DATA_READY))
		return;

	orig_size = size;
	size -= ddev->mem_offset;
	ring_write(ddev, &start, &size);

	for (i = 0; i < size; i++) {
		data = ddone_device_read_mem8(ddev, ddev->mem_offset);
		ddev->mem_offset++;

		start[i] = (char)data;
	}

	if (ddev->mem_offset == orig_size) {
		//We transfered all data
		ddone_device_write_reg32(ddev, FLAGS_REG, flags & ~DATA_READY);
		ddev->mem_offset = 0;
	}

	wake_up_interruptible(&ddev->rq);//Notify readers

}



static void device_work_f(struct work_struct *work)
{

	struct ddone_device *ddev;



	ddev = container_of(work, struct ddone_device, dwork.work);
	mutex_lock(&ddev->mutex);
	if (ddev->bytes_to_write > 0)
		device_try_write_to(ddev);
	else
		device_try_read_from(ddev);

	queue_delayed_work(ddev->device_wq, &ddev->dwork, ddev->poll_time);
	mutex_unlock(&ddev->mutex);

	wake_up_interruptible(&ddev->rq);//Notify readers
	wake_up_interruptible(&ddev->wq);//Notify readers

}


static int device_remove(struct platform_device *pdev)
{

	struct ddone_device *ddev;

	ddev = platform_get_drvdata(pdev);

	destroy_workqueue(ddev->device_wq);
	cancel_delayed_work_sync(&ddev->dwork);
	cdev_del(&ddev->cdev);

	return 0;
}



static int device_probe(struct platform_device *pdev)
{

	struct ddone_device *ddev;
	struct resource *res;
	int err;


	ddev = devm_kzalloc(&pdev->dev, sizeof(struct ddone_device),
			GFP_KERNEL);
	ddev->poll_time = msecs_to_jiffies(2000);
	mutex_init(&ddev->mutex);
	init_waitqueue_head(&ddev->rq);
	init_waitqueue_head(&ddev->wq);
	INIT_DELAYED_WORK(&ddev->dwork, device_work_f);
	ddev->device_wq = alloc_workqueue("DDONE_DRIVER_READ", WQ_UNBOUND, 1);
	if (!ddev->device_wq) {
		err = -ENOMEM;
		goto fail_no_dealloc;
	}


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pr_info("Res = %p\n", res);
	ddev->mem_size = res->end - res->start;
	ddev->mem = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ddev->mem)) {
		err = PTR_ERR(ddev->mem);
		goto fail;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	pr_info("Res = %p\n", res);
	ddev->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ddev->regs)) {
		err = PTR_ERR(ddev->regs);
		goto fail;
	}

	if (DEV_MINOR == DEVICE_COUNT) {
		err = -ENODEV;
		goto fail;
	}
	ddev->dev = MKDEV(DEV_MAJOR, DEV_MINOR++);
	pr_info("Device major %d, minor %d\n", MAJOR(ddev->dev),
			MINOR(ddev->dev));


	cdev_init(&ddev->cdev, &fops);
	ddev->cdev.owner = THIS_MODULE;
	ddev->cdev.ops = &fops;
	err = cdev_add(&ddev->cdev, ddev->dev, 1);
	if (err)
		goto fail;

	platform_set_drvdata(pdev, ddev);

	queue_delayed_work(ddev->device_wq, &ddev->dwork, 0);

	pr_info("Device probed\n");


	return 0;

fail:

	destroy_workqueue(ddev->device_wq);
fail_no_dealloc:
	return err;
}


int __init setup_driver(void)
{


	int err;
	dev_t dev;

	err = alloc_chrdev_region(&dev, 0, (int)DEVICE_COUNT, DEVICE_NAME);
	if (err)
		goto err_setup;
	DEV_MAJOR = MAJOR(dev);
	DEV_MINOR = MINOR(dev);
	err = platform_driver_register(&ddone_driver);
	if (err)
		goto err_setup;
	pr_info("Driver registered\n");
	return 0;
err_setup:
	pr_info("Error registering driver");
	return err;
}

void remove_driver(void)
{
	unregister_chrdev_region(MKDEV(DEV_MAJOR, 0), DEVICE_COUNT);
	platform_driver_unregister(&ddone_driver);
}
