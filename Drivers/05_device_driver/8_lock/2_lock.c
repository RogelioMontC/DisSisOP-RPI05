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

// declaramos la cola para bloqueo
DECLARE_WAIT_QUEUE_HEAD(cola_de_espera);
DEFINE_SPINLOCK(lock);

// condición de bloqueo
int condicion_de_bloqueo = 1;

// Interrupts variables
static short int irq_BUTTON1    = 0;

// text below will be seen in 'cat /proc/interrupts' command
#define GPIO_BUTTON1_DESC           "Button 1"

// below is optional, used in more complex code, in our case, this could be
#define GPIO_BUTTON1_DEVICE_DESC    "Berryclip"

// IRQ handler - fired on interrupt
static irqreturn_t r_irq_handler1(int irq, void *dev_id, struct pt_regs *regs)
{
    // semaphores cannot be used in interrupt or soft IRQ or tasklet code
    //  since may block (only possible in process context: process syscall,
    //  kernel thread or work queue (dealed by kthread)
    spin_lock(&lock);    // A softirq never preempts another softirq, spin_lock_bh() not necesary
    condicion_de_bloqueo = 0; // lock condition, it should be complex
    spin_unlock(&lock);
    printk(KERN_INFO "%s: [INTR] Wake up process\n", KBUILD_MODNAME);
    wake_up(&cola_de_espera); //despierta a los procesos en espera
 
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
        printk(KERN_ERR "%s: GPIO request faiure: %s\n", KBUILD_MODNAME, GPIO_BUTTON1_DESC);
        return res;
    }
    if ((irq_BUTTON1 = gpio_to_irq(GPIO_BUTTON1)) < 0) {
        printk(KERN_ERR "%s:  GPIO to IRQ mapping faiure %s\n", KBUILD_MODNAME, GPIO_BUTTON1_DESC);
        return irq_BUTTON1;
    }
    printk(KERN_NOTICE "%s: Mapped int %d for button1 in gpio %d\n", KBUILD_MODNAME, irq_BUTTON1, GPIO_BUTTON1);
    if ((res = request_irq(irq_BUTTON1,
                    (irq_handler_t ) r_irq_handler1,
                    IRQF_TRIGGER_FALLING,
                    GPIO_BUTTON1_DESC,
                    GPIO_BUTTON1_DEVICE_DESC))) {
        printk(KERN_ERR "%s:  Irq Request failure\n", KBUILD_MODNAME);
        return res;
    }
    return 0;
}

// device file operations
static ssize_t b_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    char *respuesta="OK\n";
    int len;
    unsigned long flags;

    len = (count < 3)? count: 3; 

    if (*ppos == 0) *ppos += len;
    else return 0;
    
    spin_lock_irqsave(&lock, flags); // disable softirqs on that CPU and then grabs the lock
    while (condicion_de_bloqueo) // check lock condition (should be complex)
    {   // we are blocked!
        spin_unlock_irqrestore(&lock, flags);   // release the spin_lock, softirq may grab it
        printk( KERN_INFO "%s: (read) start waiting\n", KBUILD_MODNAME);
        if (wait_event_interruptible(cola_de_espera, !condicion_de_bloqueo))  return -ERESTARTSYS;
        spin_lock_irqsave(&lock, flags);     // grab the spinlock to re-check safely the condition
    }
    printk(KERN_INFO "%s: (read) end of waiting\n", KBUILD_MODNAME);
    condicion_de_bloqueo = 1; // vuelve a dejar la condición de bloqueo a 1
    spin_unlock_irqrestore(&lock, flags);

    if (copy_to_user(buf, respuesta, len)) return -EFAULT;

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
    if (ret < 0) printk(KERN_ERR "%s: misc_register failed\n", KBUILD_MODNAME);
    else printk(KERN_NOTICE "%s: misc_register OK... b_miscdev.minor=%d\n", KBUILD_MODNAME, b_miscdev.minor);
    return ret;
}


// Module init & cleanup
static void r_cleanup(void)
{
    printk(KERN_NOTICE "%s: Cleaning up\n", KBUILD_MODNAME);
    if (b_miscdev.this_device) misc_deregister(&b_miscdev);
    
    if (irq_BUTTON1) free_irq(irq_BUTTON1, GPIO_BUTTON1_DEVICE_DESC);
    gpio_free(GPIO_BUTTON1);
    
    printk(KERN_NOTICE "%s: Done.\n", KBUILD_MODNAME);
    return;
}

static int r_init(void)
{
    int res;
    printk(KERN_NOTICE "Loading module '%s'\n", KBUILD_MODNAME);

    printk(KERN_NOTICE "%s: devices config...\n", KBUILD_MODNAME);
    if((res = r_dev_config()))
    {
        r_cleanup();
        return res;
    }

    printk(KERN_NOTICE "%s: INT config...\n", KBUILD_MODNAME);
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

