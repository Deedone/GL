#ifndef DEVICE_H
#define DEVICE_H
#include <linux/platform_device.h>
#include <linux/io.h>

#define DEVICE_NAME "ddone_device"

#define BUF_SIZE 2048
#define MEM_SIZE 1024
#define REG_SIZE 8
#define MEM_BASE1  0x60000000
#define REG_BASE1  0x60001000
#define MEM_BASE2  0x60002000
#define REG_BASE2  0x60003000

#define DATA_READY 1
#define SIZE_REG 4
#define FLAGS_REG 0

//In flags register bit 0 indicates that device buffer is full

int __init setup_devices(void);
void remove_devices(void);






#endif
