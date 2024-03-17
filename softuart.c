#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#define DEVICE_NAME "softuart"
#define CLASS_NAME "softuart"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Linux char driver for soft UART on Raspberry Pi Zero 2 W");
MODULE_VERSION("0.1");

static int majorNumber;
static char message[256] = {0};
static short size_of_message;
static struct class* softuartClass = NULL;
static struct device* softuartDevice = NULL;
static struct hrtimer tx_timer;
static struct hrtimer rx_timer;
static ktime_t ktime_tx;
static ktime_t ktime_rx;

static int gpio_tx = 4;
module_param(gpio_tx, int, S_IRUGO);
MODULE_PARM_DESC(gpio_tx, "GPIO TX pin number");

static int gpio_rx = 2;
module_param(gpio_rx, int, S_IRUGO);
MODULE_PARM_DESC(gpio_rx, "GPIO RX pin number");

static int baudrate = 9600;
module_param(baudrate, int, S_IRUGO);
MODULE_PARM_DESC(baudrate, "Baud rate for soft UART");

static int __init softuart_init(void);
static void __exit softuart_exit(void);
static int softuart_open(struct inode *, struct file *);
static int softuart_release(struct inode *, struct file *);
static ssize_t softuart_read(struct file *, char *, size_t, loff_t *);
static ssize_t softuart_write(struct file *, const char *, size_t, loff_t *);
static enum hrtimer_restart tx_timer_callback(struct hrtimer *timer);
static enum hrtimer_restart rx_timer_callback(struct hrtimer *timer);

static struct file_operations fops =
{
   .open = softuart_open,
   .read = softuart_read,
   .write = softuart_write,
   .release = softuart_release,
};

static int __init softuart_init(void) {
    printk(KERN_INFO "SoftUART: Initializing the SoftUART LKM\n");

    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0) {
        printk(KERN_ALERT "SoftUART failed to register a major number\n");
        return majorNumber;
    }
    printk(KERN_INFO "SoftUART: registered correctly with major number %d\n", majorNumber);

    softuartClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(softuartClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(softuartClass);
    }
    printk(KERN_INFO "SoftUART: device class registered correctly\n");

    softuartDevice = device_create(softuartClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(softuartDevice)) {
        class_destroy(softuartClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(softuartDevice);
    }
    printk(KERN_INFO "SoftUART: device class created correctly\n");

    if (!gpio_is_valid(gpio_tx)) {
        printk(KERN_INFO "SoftUART: invalid TX GPIO\n");
        return -ENODEV;
    }

    gpio_request(gpio_tx, "sysfs");
    gpio_direction_output(gpio_tx, 0);
    gpio_export(gpio_tx, false);

    if (!gpio_is_valid(gpio_rx)) {
        printk(KERN_INFO "SoftUART: invalid RX GPIO\n");
        return -ENODEV;
    }

    gpio_request(gpio_rx, "sysfs");
    gpio_direction_input(gpio_rx);
    gpio_export(gpio_rx, false);

    ktime_tx = ktime_set(0, 1000000000 / baudrate);
    hrtimer_init(&tx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    tx_timer.function = &tx_timer_callback;

    ktime_rx = ktime_set(0, 1000000000 / baudrate);
    hrtimer_init(&rx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    rx_timer.function = &rx_timer_callback;

    return 0;
}

static void __exit softuart_exit(void) {
    gpio_unexport(gpio_tx);
       gpio_free(gpio_tx);
    gpio_unexport(gpio_rx);
    gpio_free(gpio_rx);

    hrtimer_cancel(&tx_timer);
    hrtimer_cancel(&rx_timer);

    device_destroy(softuartClass, MKDEV(majorNumber, 0));
    class_unregister(softuartClass);
    class_destroy(softuartClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_INFO "SoftUART: Goodbye from the LKM!\n");
}

static int softuart_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "SoftUART: Device has been opened\n");
    return 0;
}

static int softuart_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "SoftUART: Device successfully closed\n");
    return 0;
}

static ssize_t softuart_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int error_count = 0;

    error_count = copy_to_user(buffer, message, size_of_message);

    if (error_count == 0) {
        printk(KERN_INFO "SoftUART: Sent %d characters to the user\n", size_of_message);
        return (size_of_message = 0);
    } else {
        printk(KERN_INFO "SoftUART: Failed to send %d characters to the user\n", error_count);
        return -EFAULT;
    }
}

static ssize_t softuart_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    sprintf(message, "%s(%zu letters)", buffer, len);
    size_of_message = strlen(message);
    printk(KERN_INFO "SoftUART: Received %zu characters from the user\n", len);
    return len;
}

static enum hrtimer_restart tx_timer_callback(struct hrtimer *timer) {
    // Implement your transmit logic here
    return HRTIMER_NORESTART;
}

static enum hrtimer_restart rx_timer_callback(struct hrtimer *timer) {
    // Implement your receive logic here
    return HRTIMER_NORESTART;
}

module_init(softuart_init);
module_exit(softuart_exit);
