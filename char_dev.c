#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define DEVICE_NAME "my_buffer"
#define CLASS_NAME  "my_class"
#define BUF_SIZE    1024

/* Уникальные коды команд для ioctl */
#define IOCTL_GETBUFFER_SIZE _IOR('b', 1, int32_t*)
#define IOCTL_GET_USED_SPACE _IOR('b', 2, int32_t*)
#define IOCTL_GET_FREE_SPACE _IOR('b', 3, int32_t*)
#define IOCTL_CLEAR_BUFFER   _IO('b', 4)

/* СТРУКТУРА ДАННЫХ */
struct ring_buffer_dev {
    unsigned char buffer[BUF_SIZE];
    size_t read_ptr;
    size_t write_ptr;
    size_t bytes_in_buffer;
    struct mutex lock;
    struct cdev cdev;
} *my_device_data; 

/* ПРОТОТИПЫ ФУНКЦИЙ */
static int      dev_open(struct inode *, struct file *);
static int      dev_release(struct inode *, struct file *);
static ssize_t  dev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t  dev_write(struct file *, const char __user *, size_t, loff_t *);
static long     dev_ioctl(struct file *, unsigned int, unsigned long);

/* ФАЙЛОВЫЕ ОПЕРАЦИИ */
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .read    = dev_read,
    .write   = dev_write,
    .unlocked_ioctl = dev_ioctl,
    .release = dev_release,
};

/* ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ */
static int major_number;
static struct class* my_class = NULL;
static struct device* my_device = NULL;

/* ИНИЦИАЛИЗАЦИЯ МОДУЛЯ */
static int __init char_dev_demo_init(void) {
    printk(KERN_INFO "RingBuffer: Инициализация \n");

    /* Выделяем память только под структуру */
    my_device_data = kzalloc(sizeof(*my_device_data), GFP_KERNEL);
    if (!my_device_data) {
        return -ENOMEM;
    }

    mutex_init(&my_device_data->lock);

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        kfree(my_device_data);
        printk(KERN_ALERT "RingBuffer: Ошибка регистрации\n");
        return major_number;
    }

    my_class = class_create(CLASS_NAME);
    if (IS_ERR(my_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        kfree(my_device_data);
        return PTR_ERR(my_class);
    }

    /* Инициализируем cdev */
    cdev_init(&my_device_data->cdev, &fops);
    my_device_data->cdev.owner = THIS_MODULE;
    if (cdev_add(&my_device_data->cdev, MKDEV(major_number, 0), 1)) {
        class_destroy(my_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        kfree(my_device_data);
        return -1;
    }

    /* Передаем указатель на устройство (&my_device_data->cdev) */
    my_device = device_create(my_class, NULL, MKDEV(major_number, 0), &my_device_data->cdev, DEVICE_NAME);
    if (IS_ERR(my_device)) {
        class_destroy(my_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        kfree(my_device_data);
        return PTR_ERR(my_device);
    }

    printk(KERN_INFO "RingBuffer: Устройство /dev/%s создано\n", DEVICE_NAME);
    return 0;
}


/* ВЫГРУЗКА МОДУЛЯ */
static void __exit char_dev_demo_exit(void) {
    device_destroy(my_class, MKDEV(major_number, 0));
    class_destroy(my_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    cdev_del(&my_device_data->cdev);
    kfree(my_device_data);
    printk(KERN_INFO "RingBuffer: Модуль выгружен. Ресурсы освобождены.\n");
}

/* РЕАЛИЗАЦИЯ ФУНКЦИЙ */
// Открываем устройство
static int dev_open(struct inode *inodep, struct file *filep){
    filep->private_data = my_device_data;
    nonseekable_open(inodep, filep); 
    printk(KERN_INFO "RingBuffer: Устройство открыто\n");
    return 0;
}

// Вызывается при закрытии файла устройства
static int dev_release(struct inode *inodep, struct file *filep){
    printk(KERN_INFO "RingBuffer: Устройство закрыто\n");
    return 0;
}

// Вызывается, когда пользователь пишет в /dev/my_buffer
static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset){
    struct ring_buffer_dev *data = filep->private_data;
    
    size_t bytes_to_write = len;
    size_t bytes_written = 0;

    if (mutex_lock_interruptible(&data->lock))
        return -ERESTARTSYS;

    while (bytes_to_write > 0 && data->bytes_in_buffer < BUF_SIZE) {
        size_t free_space = BUF_SIZE - data->bytes_in_buffer;
        size_t space_to_end = BUF_SIZE - data->write_ptr;
        
        size_t bytes_this_pass = min(bytes_to_write, min(free_space, space_to_end));

        if (copy_from_user(data->buffer + data->write_ptr, buffer + bytes_written, bytes_this_pass)) {
            mutex_unlock(&data->lock);
            return -EFAULT;
        }

        data->write_ptr = (data->write_ptr + bytes_this_pass) % BUF_SIZE;
        data->bytes_in_buffer += bytes_this_pass;
        bytes_written += bytes_this_pass;
        bytes_to_write -= bytes_this_pass;

        printk(KERN_INFO "RingBuffer: Записано %zu байт. Всего в буфере: %zu\n", bytes_this_pass, data->bytes_in_buffer);
        
        if (data->bytes_in_buffer == BUF_SIZE)
            break;
    }
    
    mutex_unlock(&data->lock);
    
    // Возвращаем -ENOSPC (No space), если ничего не записали из-за переполнения
    return bytes_written ? bytes_written : -ENOSPC; 
}

// Вызывается, когда пользователь читает из /dev/my_buffer
static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset){
    struct ring_buffer_dev *data = filep->private_data;
    
    if (mutex_lock_interruptible(&data->lock))
        return -ERESTARTSYS;

     size_t available = data->bytes_in_buffer;
     size_t to_read = min(len, available);    
     size_t bytes_read = 0; 

     while (bytes_read < to_read && data->bytes_in_buffer > 0) {
         size_t data_to_end = BUF_SIZE - data->read_ptr;
         
         size_t bytes_this_pass = min(to_read - bytes_read, data_to_end);
 
         if (copy_to_user(buffer + bytes_read, data->buffer + data->read_ptr, bytes_this_pass)) {
             mutex_unlock(&data->lock);
             return -EFAULT;
         }
         
         data->read_ptr = (data->read_ptr + bytes_this_pass) % BUF_SIZE;
         data->bytes_in_buffer -= bytes_this_pass;
         bytes_read += bytes_this_pass;
 
         printk(KERN_INFO "RingBuffer: Прочитано %zu байт. Осталось в буфере: %zu\n", bytes_this_pass, data->bytes_in_buffer);
     }
     
     mutex_unlock(&data->lock);
     
     // Возвращаем -EAGAIN (Try again), если буфер был пуст и ничего не прочитали
     return bytes_read; 
}

// Вызывается для команд ioctl
static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg){
     struct ring_buffer_dev *data = filep->private_data;
     int32_t val;
 
     switch(cmd) {
         case IOCTL_GETBUFFER_SIZE:
             val = BUF_SIZE;
             if (copy_to_user((int32_t __user *)arg, &val, sizeof(val))) return -EFAULT;
             break;
         case IOCTL_GET_USED_SPACE:
             val = (int32_t)data->bytes_in_buffer;
             if (copy_to_user((int32_t __user *)arg, &val, sizeof(val))) return -EFAULT;
             break;
         case IOCTL_GET_FREE_SPACE:
             val = (int32_t)(BUF_SIZE - data->bytes_in_buffer);
             if (copy_to_user((int32_t __user *)arg, &val, sizeof(val))) return -EFAULT;
             break;
         case IOCTL_CLEAR_BUFFER:
             mutex_lock(&data->lock);
             data->read_ptr = 0;
             data->write_ptr = 0;
             data->bytes_in_buffer = 0;
             mutex_unlock(&data->lock);
             break;
         default:
              return -ENOTTY;
      }
      return 0;
}

module_init(char_dev_demo_init);
module_exit(char_dev_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Zubin");
MODULE_DESCRIPTION("Драйвер кольцевого буфера");
MODULE_VERSION("0.4");
