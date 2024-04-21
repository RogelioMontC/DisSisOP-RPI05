#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h>         // file operations
#include <linux/uaccess.h>      // copy to/from user space
#include <linux/wait.h>       // waiting queue
#include <linux/sched.h>      // TASK_INTERRUMPIBLE
#include <linux/delay.h>      // udelay
#include <linux/interrupt.h>
#include <linux/gpio.h>

#define DRIVER_AUTHOR "DAC-UMA"
#define DRIVER_DESC   "locking example"

//GPIOS numbers as in BCM RPi

#define GPIO_BUTTON1 2

// counting semaphore
DEFINE_SEMAPHORE(semaphore);    // semaphore value = 1
static unsigned int value = 1;

// Interrupts variables
static short int irq_BUTTON1 = 0;

// text below will be seen in 'cat /proc/interrupts' command
#define GPIO_BUTTON1_DESC           "Button 1"

// below is optional, used in more complex code, in our case, this could be
#define GPIO_BUTTON1_DEVICE_DESC    "Berryclip"

// IRQ handler - fired on interrupt
static irqreturn_t r_irq_handler(int irq, void *dev_id)
{
    value++;        // incr. the number of button push
    printk(KERN_INFO " interrupt -> value=%d\n", value);
    up(&semaphore); // counting the number of button push
 
    return IRQ_HANDLED;
}

static int r_int_config(void)
{
    int res;
    if ((res = gpio_is_valid(GPIO_BUTTON1)) < 0) {
        printk(KERN_ERR "%s: Invalid GPIO %d\n", KBUILD_MODNAME, GPIO_BUTTON1);
        return res;
    }
    if ((res = gpio_request(GPIO_BUTTON1, GPIO_BUTTON1_DESC))) {
        printk(KERN_ERR "GPIO request faiure: %s\n", GPIO_BUTTON1_DESC);
        return res;
    }
    if ((irq_BUTTON1 = gpio_to_irq(GPIO_BUTTON1)) < 0) {
        printk(KERN_ERR "GPIO to IRQ mapping faiure %s\n", GPIO_BUTTON1_DESC);
        return irq_BUTTON1;
    }
    printk(KERN_NOTICE "Mapped int %d for button1 in gpio %d\n", irq_BUTTON1, GPIO_BUTTON1);
    if ((res = request_irq(irq_BUTTON1,
                    (irq_handler_t) r_irq_handler,
                    IRQF_TRIGGER_FALLING,
                    GPIO_BUTTON1_DESC,
                    GPIO_BUTTON1_DEVICE_DESC))) {
        printk(KERN_ERR "Irq Request failure\n");
        return res;
    }
    return 0;
}

// device file operations
static ssize_t b_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    char *respuesta="OK\n";
    int len;

    if (*ppos != 0) return 0;
    
    if (down_interruptible(&semaphore)) return -ERESTARTSYS;
    value--;
    printk(KERN_INFO " read -> value=%d\n", value);

    len = (count < 3)? count: 3;
    if (copy_to_user(buf, respuesta, len)) return -EFAULT;
    *ppos += len;
    return len;
}

static const struct file_operations b_fops = {
    .owner      = THIS_MODULE,
    .read       = b_read,
};

// device struct
static struct miscdevice b_miscdev = {
    .minor      = MISC_DYNAMIC_MINOR,
    .name       = "locking",
    .fops       = &b_fops,
    .mode       = S_IRUGO | S_IWUGO,
};

// This functions registers devices, requests GPIOs and configures interrupts
static int r_dev_config(void)
{
    int ret = 0;
    ret = misc_register(&b_miscdev);
    if (ret < 0) printk(KERN_ERR "misc_register failed\n");
    else printk(KERN_NOTICE "misc_register OK... b_miscdev.minor=%d\n", b_miscdev.minor);
    return ret;
}


// Module init & cleanup
static void r_cleanup(void)
{
    printk(KERN_NOTICE "cleaning up module '%s'\n", KBUILD_MODNAME);
    if (b_miscdev.this_device) misc_deregister(&b_miscdev);
    
    if (irq_BUTTON1) free_irq(irq_BUTTON1, GPIO_BUTTON1_DEVICE_DESC);
    gpio_free(GPIO_BUTTON1);
    
    printk(KERN_NOTICE "Done. Bye from module '%s'\n", KBUILD_MODNAME);
    return;
}

static int r_init(void)
{
    int res;
    printk(KERN_NOTICE "Loading module '%s'\n", KBUILD_MODNAME);

    printk(KERN_NOTICE "%s - devices config...\n", KBUILD_MODNAME);
    if((res = r_dev_config()))
    {
        r_cleanup();
        return res;
    }

    printk(KERN_NOTICE "%s - INT config...\n", KBUILD_MODNAME);
    if((res = r_int_config()))
    {
        r_cleanup();
        return res;
    }

    return 0;
}

module_init(r_init);
module_exit(r_cleanup);

/****************************************************************************/
/* Module licensing/description block.                                      */
/****************************************************************************/
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

