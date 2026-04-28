#ifndef MY_BUFFER_IOCTL_H
#define MY_BUFFER_IOCTL_H

#include <linux/ioctl.h>

#ifndef __KERNEL__
#include <stdint.h>
#endif

/* По логике b - buffer */
#define MY_BUF_MAGIC 'b'

/* Используем int32_t (тип), а не указатель, для корректного вычисления размера */
#define IOCTL_GETBUFFER_SIZE _IOR(MY_BUF_MAGIC, 1, int32_t)
#define IOCTL_GET_USED_SPACE _IOR(MY_BUF_MAGIC, 2, int32_t)
#define IOCTL_GET_FREE_SPACE _IOR(MY_BUF_MAGIC, 3, int32_t)
#define IOCTL_CLEAR_BUFFER   _IO(MY_BUF_MAGIC, 4)

#endif

