#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/slab.h>

#define DEVICE_NAME "my_buffer"
#define CLASS_NAME  "my_class"
#define BUF_SIZE    1024

/* Уникальные коды команд для ioctl */
#define IOCTL_GETBUFFER_SIZE _IOR('b', 1, int32_t*)
#define IOCTL_GET_USED_SPACE _IOR('b', 2, int32_t*)
#define IOCTL_GET_FREE_SPACE _IOR('b', 3, int32_t*)
#define IOCTL_CLEAR_BUFFER _IO('b', 4, int32_t*)

/* СТРУКТУРА ДАННЫХ */
struct ring_buffer_dev {
    unsigned char buffer[BUF_SIZE];
    int read_ptr;
    int write_ptr;
    int bytes_in_buffer;
    struct mutex lock;
} *my_device_data; 

/* ПРОТОТИПЫ ФУНКЦИЙ */
static int      dev_open(struct inode *, struct file *);
static int      dev_release(struct inode *, struct file *);
static ssize_t  dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t  dev_write(struct file *, const char *, size_t, loff_t *);
static long     dev_ioctl(struct file *, unsigned int, unsigned long);

/* ФАЙЛОВЫЕ ОПЕРАЦИИ
   Связка имен функций с системными вызовами. */
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .read    = dev_read,
    .write   = dev_write,
    .unlocked_ioctl = dev_ioctl,
    .release = dev_release,
};

/* ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
   Переменные для регистрации устройства. */
static int major_number;
static struct class* my_class = NULL;
static struct device* my_device = NULL;

/* ИНИЦИАЛИЗАЦИЯ МОДУЛЯ */
static int __init char_dev_demo_init(void) {
    printk(KERN_INFO "RingBuffer: Инициализация \n");

    /* Выделяем память только под структуру */
    my_device_data = kzalloc(sizeof(struct ring_buffer_dev), GFP_KERNEL);
    if (!my_device_data) {
        return -ENOMEM;
    }

    /* Инициализируем переменные */
    my_device_data->read_ptr = 0;
    my_device_data->write_ptr = 0;
    my_device_data->bytes_in_buffer = 0;
    mutex_init(&my_device_data->lock);

    /*  Регистрируем устройство */
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        kfree(my_device_data);
        printk(KERN_ALERT "RingBuffer: Ошибка регистрации\n");
        return major_number;
    }

    /* Создаем класс и устройство в /dev/ */
    my_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(my_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        kfree(my_device_data);
        return PTR_ERR(my_class);
    }

    my_device = device_create(my_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
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
    kfree(my_device_data);
    printk(KERN_INFO "RingBuffer: Модуль выгружен. Ресурсы освобождены.\n");
}

/* РЕАЛИЗАЦИЯ ФУНКЦИЙ */
// Открываем устройство
static int dev_open(struct inode *inodep, struct file *filep){
    filep->private_data = my_device_data;
    printk(KERN_INFO "RingBuffer: Устройство открыто\n");
    return 0;
}

// Вызывается при закрытии файла устройства
static int dev_release(struct inode *inodep, struct file *filep){
    printk(KERN_INFO "Driver: Устройство закрыто\n");
    return 0;
}

// Вызывается, когда пользователь пишет в /dev/my_buffer
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
    struct ring_buffer_dev *data = filep->private_data;
    int bytes_to_write = len;
    int bytes_written = 0;

    if (mutex_lock_interruptible(&data->lock))
        return -ERESTARTSYS;

    while (bytes_to_write > 0) {
        int free_space = BUF_SIZE - data->bytes_in_buffer;
        if (free_space == 0) break; // Буфер полон, выходим и возвращаем что успели записать

        // Проверяем сколько байт можем записать до конца буфера
        int space_to_end = BUF_SIZE - data->write_ptr;

        // Проверяем сколько байт реально запишем за один заход (минимум из свободного места и места до конца)
        int bytes_this_pass = min(bytes_to_write, min(free_space, space_to_end));

        // Копируем данные из пространства пользователся в буфер ядра
        if (copy_from_user(data->buffer + data->write_ptr, buffer + bytes_written, bytes_this_pass)) {
            mutex_unlock(&data->lock);
            return -EFAULT;
        }

        data->write_ptr = (data->write_ptr + bytes_this_pass) % BUF_SIZE;
        data->bytes_in_buffer += bytes_this_pass;
        bytes_written += bytes_this_pass;
        bytes_to_write -= bytes_this_pass;

        printk(KERN_INFO "RingBuffer: Записано %d байт. Всего в буфере: %d\n", bytes_this_pass, data->bytes_in_buffer);
    }
    
    mutex_unlock(&data->lock);
    return bytes_written;
    
}

// Вызывается, когда пользователь читает из /dev/my_buffer
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
    printk(KERN_INFO "Driver: Операция чтения (TODO)\n");
    // TODO: Реализовать чтение данных из буфера
    return 0;
}

// Вызывается для команд ioctl
static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg){
    printk(KERN_INFO "Driver: Операция ioctl (TODO). Команда: %u\n", cmd);
    // TODO: Реализовать обработку команд управления
    return 0;
}

/* РЕГИСТРАЦИЯ ТОЧЕК ВХОДА (ГОТОВО)
   Говорим ядру, какие функции вызывать при загрузке/выгрузке. */
module_init(char_dev_demo_init);
module_exit(char_dev_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Zubin");
MODULE_DESCRIPTION("Прототип драйвера кольцевого буфера");
MODULE_VERSION("0.1");
