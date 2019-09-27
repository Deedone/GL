#ifndef IOCTL_H
#define IOCTL_H

#define DDONE_IOC_MAGIC 'x'
#define DDONE_IOC_MAXNR 1
#define DDONE_SET_POLL _IOW(DDONE_IOC_MAGIC,1,uint32_t)


#endif
