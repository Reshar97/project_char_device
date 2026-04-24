#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "my_buffer"
#define CLASS_NAME  "my_class"
#define BUF_SIZE    1024

/* СТРУКТУРА ДАННЫХ */
struct my_device_data {
    unsigned char buffer[BUF_SIZE];
    int head;
    int tail;
    struct mutex lock;
    struct cdev cdev;
};

/* ПРОТОТИПЫ ФУНКЦИЙ (TODO)
   Объявляем все функции */
static int      dev_open(struct inode *, struct file *);
static int      dev_release(struct inode *, struct file *);
static ssize_t  dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t  dev_write(struct file *, const char *, size_t, loff_t *);
static long     dev_ioctl(struct file *, unsigned int, unsigned long);

/* ФАЙЛОВЫЕ ОПЕРАЦИИ (ГОТОВО)
   Связка имен функций с системными вызовами. */
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .read    = dev_read,
    .write   = dev_write,
    .unlocked_ioctl = dev_ioctl,
    .release = dev_release,
};

/* ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (ГОТОВО)
   Переменные для регистрации устройства. */
static int major_number;
static struct class* my_class = NULL;
static struct device* my_device = NULL;

/* ИНИЦИАЛИЗАЦИЯ МОДУЛЯ (TODO) 
  Эта функция вызывается при команде 'insmod'.
  TODO: Выделить память, зарегистрировать устройство. */
static int __init char_dev_demo_init(void) {
    printk(KERN_INFO "Driver: Инициализация (TODO)\n");

    // TODO: Реализовать регистрацию главного номера устройства
    // major_number = ...

    // TODO: Реализовать создание класса и устройства
    // my_class = ...
    // my_device = ...

    if (/* TODO: Проверка, что все создалось успешно */) {
        printk(KERN_INFO "Driver: Устройство /dev/%s создано\n", DEVICE_NAME);
        return 0;
    } else {
        // TODO: Реализовать очистку ресурсов в случае ошибки
        return -1;
    }
}

/* ВЫГРУЗКА МОДУЛЯ (TODO)
 Эта функция вызывается при команде 'rmmod'.
 TODO: Освободить память и отменить регистрацию. */
static void __exit char_dev_demo_exit(void) {
    printk(KERN_INFO "Driver: Выгрузка (TODO)\n");

    // TODO: Освободить ресурсы в обратном порядке
}

/* РЕАЛИЗАЦИЯ ФУНКЦИЙ (TODO)
   Здесь будет логика работы драйвера. */

// Вызывается при открытии файла устройства
static int dev_open(struct inode *inodep, struct file *filep){
    printk(KERN_INFO "Driver: Устройство открыто\n");
    return 0;
}

// Вызывается при закрытии файла устройства
static int dev_release(struct inode *inodep, struct file *filep){
    printk(KERN_INFO "Driver: Устройство закрыто\n");
    return 0;
}

// Вызывается, когда пользователь читает из /dev/my_buffer
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
    printk(KERN_INFO "Driver: Операция чтения (TODO)\n");
    // TODO: Реализовать чтение данных из буфера
    return 0;
}

// Вызывается, когда пользователь пишет в /dev/my_buffer
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
    printk(KERN_INFO "Driver: Операция записи (TODO). Получено %zu байт\n", len);
    // TODO: Реализовать запись данных в буфер
    return len; // Пока просто подтверждаем получение
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
