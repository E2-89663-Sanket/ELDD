#ifndef __IOCTL_H
#define __IOCTL_H

#include<linux/module.h>


typedef struct{
    short size;
    short len;
    short avail;
}fifo_info_t;

#define FIFO_CLEAR   _IO('x',1)

#define FIFO_GET_INFO   _IOR('x', 2, fifo_info_t)
#define FIFO_RESIZE     _IOW('x', 3, long)

#endif