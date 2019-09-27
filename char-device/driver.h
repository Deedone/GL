#ifndef DRIVER_H
#define DRIVER_H


#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/wait.h>

#include "device.h"
#include "ioctl.h"


#define MAX_POLL_INTERVAL 10000
#define MIN_POLL_INTERVAL 1

int __init setup_driver(void);
void remove_driver(void);


enum minors{
	DEVICE_1,
	DEVICE_2,
	DEVICE_COUNT
};
extern unsigned int DEV_MAJOR;
extern unsigned int DEV_MINOR;

struct ddone_device{
	struct platform_device *pdev;
	struct workqueue_struct *device_wq;
	void __iomem *mem;
	void __iomem *regs;
	struct cdev cdev;
	struct mutex mutex;
	struct delayed_work dwork;
	int major;
	int r, w; //Read and write pointers to cyclic buffer
	size_t mem_size;
	size_t bytes_to_write;
	size_t mem_offset;
	char data[BUF_SIZE + 1];
	u64 poll_time;
	wait_queue_head_t rq, wq;//Read and write queues
	dev_t dev;
};


#endif
