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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "my_buffer_ioctl.h"

#define DEVICE_NAME "my_buffer"
#define CLASS_NAME  "my_class"
#define BUF_SIZE    1024

/* Структура данных */
struct ring_buffer_dev {
    unsigned char buffer[BUF_SIZE];
    size_t read_ptr;
    size_t write_ptr;
    size_t bytes_in_buffer;
    struct mutex lock;
    struct cdev cdev;
} *my_device_data; 

/* Прототипы функций */
static int      dev_open(struct inode *, struct file *);
static int      dev_release(struct inode *, struct file *);
static ssize_t  dev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t  dev_write(struct file *, const char __user *, size_t, loff_t *);
static long     dev_ioctl(struct file *, unsigned int, unsigned long);
static int      my_proc_show(struct seq_file *m, void *v);

/* Файловые операции */
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .read    = dev_read,
    .write   = dev_write,
    .unlocked_ioctl = dev_ioctl,
    .release = dev_release,
};

/* Глобальные переменные */
static dev_t dev_num;
static struct class* my_class = NULL;
static struct device* my_device = NULL;
static struct proc_dir_entry *proc_entry;

/* Инициализация модуля */
static int __init char_dev_init(void) {
    printk(KERN_INFO "RingBuffer: Инициализация \n");

    /* Выделяем память под структуру */
    my_device_data = kzalloc(sizeof(*my_device_data), GFP_KERNEL);
    if (!my_device_data) {
        return -ENOMEM;
    }

    mutex_init(&my_device_data->lock);

    /* Регистрируем номера устройств динамически */
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
        kfree(my_device_data);
        return -1;
    }

    /* Инициализируем и добавляем cdev */
    cdev_init(&my_device_data->cdev, &fops);
    my_device_data->cdev.owner = THIS_MODULE;
    if (cdev_add(&my_device_data->cdev, dev_num, 1) < 0) {
        unregister_chrdev_region(dev_num, 1);
        kfree(my_device_data);
        return -1;
    }

    my_class = class_create(CLASS_NAME);
    if (IS_ERR(my_class)) {
        cdev_del(&my_device_data->cdev);
        unregister_chrdev_region(dev_num, 1);
        kfree(my_device_data);
        return PTR_ERR(my_class);
    }

    /* Создаем устройство */
    my_device = device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(my_device)) {
        class_destroy(my_class);
        cdev_del(&my_device_data->cdev);
        unregister_chrdev_region(dev_num, 1);
        kfree(my_device_data);
        return PTR_ERR(my_device);
    }

    /* Регистрация в /proc через seq_file */
    proc_entry = proc_create_single_data(DEVICE_NAME, 0444, NULL, my_proc_show, my_device_data);
    
    printk(KERN_INFO "RingBuffer: Устройство /dev/%s и /proc/%s созданы\n", DEVICE_NAME, DEVICE_NAME);
    return 0;
}

/* Выгрузка модуля */
static void __exit char_dev_exit(void) {
    if (proc_entry) {
        proc_remove(proc_entry);
    }
    
    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_device_data->cdev);
    unregister_chrdev_region(dev_num, 1);
    kfree(my_device_data);
    
    printk(KERN_INFO "RingBuffer: Модуль выгружен. Ресурсы освобождены.\n");
}

/* Реализация функций драйвера
 * Открываем устройство */
static int dev_open(struct inode *inodep, struct file *filep){
    filep->private_data = my_device_data;
    printk(KERN_INFO "RingBuffer: Устройство открыто \n");
    nonseekable_open(inodep, filep); 
    return 0;
}

/* Закрываем устройство */
static int dev_release(struct inode *inodep, struct file *filep){
    printk(KERN_INFO "RingBuffer: Устройство закрыто \n");
    return 0;
}

/* Пишем в буфер */
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

        printk(KERN_INFO"RingBuffer: Записано %zu байт. Всего в буфере: %zu\n", bytes_this_pass, data->bytes_in_buffer);
    }
    
    mutex_unlock(&data->lock);
    return bytes_written; 
}

/* Читаем из буфера */
static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset){
    struct ring_buffer_dev *data = filep->private_data;
    
    if (mutex_lock_interruptible(&data->lock))
        return -ERESTARTSYS;

     size_t to_read = min(len, data->bytes_in_buffer);    
     size_t bytes_read = 0; 

     while (bytes_read < to_read) {
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
     return bytes_read; 
}

/* Функции ioctl */
static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg){
     struct ring_buffer_dev *data = filep->private_data;
     int32_t val;
 
     switch(cmd) {
         case IOCTL_GETBUFFER_SIZE:
             val = BUF_SIZE;
             break;
         case IOCTL_GET_USED_SPACE:
             val = (int32_t)data->bytes_in_buffer;
             break;
         case IOCTL_GET_FREE_SPACE:
             val = (int32_t)(BUF_SIZE - data->bytes_in_buffer);
             break;
         case IOCTL_CLEAR_BUFFER:
             mutex_lock(&data->lock); 
             data->read_ptr = data->write_ptr = data->bytes_in_buffer = 0;
             mutex_unlock(&data->lock);
             printk(KERN_INFO "RingBuffer: Буфер очищен через IOCTL\n");
             return 0;
         default:
              return -ENOTTY;
      }
      
      if (copy_to_user((int32_t __user *)arg, &val, sizeof(val)))
          return -EFAULT;
          
      return 0;
}

/* Реализация функции для /proc */
static int my_proc_show(struct seq_file *m, void *v) {
     struct ring_buffer_dev *data = (struct ring_buffer_dev *)m->private;
     
     seq_printf(m, "--- Состояние кольцевого буфера ---\n");
     if (mutex_lock_interruptible(&data->lock)) return -ERESTARTSYS;
     seq_printf(m, "Занято: %zu\nСвободно: %zu\nRead Ptr: %zu\nWrite Ptr: %zu\n",
                data->bytes_in_buffer, BUF_SIZE - data->bytes_in_buffer,
                data->read_ptr, data->write_ptr);
     mutex_unlock(&data->lock);
     return 0;
}

module_init(char_dev_init);
module_exit(char_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Zubin");
MODULE_DESCRIPTION("Драйвер кольцевого буфера");
MODULE_VERSION("1.0");
