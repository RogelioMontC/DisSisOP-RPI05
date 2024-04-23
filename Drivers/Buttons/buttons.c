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

#define DRIVER_AUTHOR "Rogelio y Augusto"
#define DRIVER_DESC   "buttons driver with wait_queue and locking"

//GPIOS numbers as in BCM RPi

#define GPIO_BUTTON1 2
#define GPIO_BUTTON2 3

//Variables de cola

// Interrupts variables
static short int irq_BUTTON1    = 0;
static short int irq_BUTTON2    = 0;

// text below will be seen in 'cat /proc/interrupts' command
#define GPIO_BUTTON1_DESC           "Button 1"
#define GPIO_BUTTON2_DESC           "Button 2"


static int r_int_config(void)
{
  
}

static ssize_t b_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{

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

static int r_dev_config(void)
{
  
}

static void r_cleanup(void)
{

}

static int r_init(void)
{
  
}



module_init(r_init);
module_exit(r_cleanup);

/****************************************************************************/
/* Module licensing/description block.                                      */
/****************************************************************************/
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
