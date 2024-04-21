#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h>         // file operations
#include <linux/uaccess.h>    // copy to/from user space
#include <linux/wait.h>       // waiting queue
#include <linux/sched.h>      // TASK_INTERRUMPIBLE
#include <linux/delay.h>      // udelay

#include <linux/interrupt.h>
#include <linux/gpio.h>

#define DRIVER_AUTHOR "DAC-UMA"
#define DRIVER_DESC   "Example of interrupt with tasklet"

//GPIO numbers for button 1
#define GPIO_BUTTON1 2

// Interrupt variable
static short int irq_BUTTON1 = 0;

//  proc entry
#define ENTRY_NAME "counter"
#define PERMS 0644
#define PARENT NULL

// shared resources
static unsigned value = 0;
static long long sched_time;

// text below will be seen in 'cat /proc/interrupt' command
#define GPIO_BUTTON1_NAME   "Button 1"

// below is optional, used in more complex code, in our case, this could be
#define GPIO_BUTTON1_DEV    "Interrupt"

static void tasklet_handler(struct tasklet_struct *);
DECLARE_TASKLET(mytasklet /*nombre*/, tasklet_handler /*funcion*/);


// irq handlers and tasklets run in atomic context, can NOT block/sleep
//  So, use spinlock to deal with sinchronization issues
static DEFINE_SPINLOCK(irq_tasklet_lock);
static DEFINE_SPINLOCK(tasklet_proc_lock);

static void tasklet_handler(struct tasklet_struct *ptr)
{
    long long now, when;
    unsigned long local_data;
    now = get_jiffies_64();
    printk(KERN_INFO "%s: tasklet: in_interrup()=%d\tin_hardirq()=%d\tin_softirq()=%d\tin_atomic()=%d\n", KBUILD_MODNAME, !!in_interrupt(), !!in_hardirq(), !!in_softirq(), !!in_atomic());
    spin_lock_irq(&irq_tasklet_lock);    // enter critical section (with irq)
    when = sched_time;
    spin_unlock_irq(&irq_tasklet_lock); // exit critical section
    spin_lock(&tasklet_proc_lock);      // enter critical section (with process)
    local_data = ++value;    // increment value with button push
    spin_unlock(&tasklet_proc_lock);    // exit critical section
    printk(KERN_NOTICE "%s: tasklet: value = %lu, sched at: %llu, run at %llu\n", KBUILD_MODNAME, local_data, now, when);
}

// IRQ handler - fired on interrupt
static irqreturn_t r_irq_handler(int irq, void *dev_id)
{
    char *cookie = (char *)dev_id;
    long long now = get_jiffies_64();

    // due to switch bouncing this handler will be fired few times for every button push
    // do here ONLY the hardware related issue
    printk(KERN_INFO "%s: %s: in_interrup()=%d\tin_hardirq()=%d\tin_softirq()=%d\tin_atomic()=%d\n", KBUILD_MODNAME, cookie, !!in_interrupt(), !!in_hardirq(), !!in_softirq(), !!in_atomic());
    spin_lock(&irq_tasklet_lock);       // enter critical section (with tasklet)
    sched_time = now;
    spin_unlock(&irq_tasklet_lock);     // exit critical section
    tasklet_schedule(&mytasklet); // launch deferred job (time consuming processing)
    printk(KERN_NOTICE "%s: %s: tasklet scheduled by irq at %llu\n", KBUILD_MODNAME, cookie, now);

    return IRQ_HANDLED;
}

// proc file operations (read only)
ssize_t counter_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset)
{
    char message[20];
    int len;
    int local_counter;
    if (*offset > 0) return 0; // only first read allowed, offset==0
    printk(KERN_INFO "%s: read: in_interrup()=%d\tin_hardirq()=%d\tin_softirq()=%d\tin_atomic()=%d\n", KBUILD_MODNAME, !!in_interrupt(), !!in_hardirq(), !!in_softirq(), !!in_atomic());
    spin_lock_bh(&tasklet_proc_lock);   // enter critical section (with tasklet)
    printk(KERN_INFO "%s: read: <<<spin lock grabbed>>>, in_atomic()=%d\n", KBUILD_MODNAME, !!in_atomic());
    local_counter = value;
    spin_unlock_bh(&tasklet_proc_lock); // exit critical section
    printk(KERN_NOTICE "%s: read: value = %d, in_atomic()=%d\n", KBUILD_MODNAME, local_counter, !!in_atomic());
    len = sprintf(message, "%d\n", local_counter);
    if (len > size) len = size;
    if (copy_to_user(buf, message, len)) return -EFAULT;
    *offset += len;
    return len;
}

static struct proc_ops fops = {
    .proc_read = counter_proc_read,
};

// This functions configures interrupt for button 1
static int r_int_config(void)
{
    int res;
    
    if ((res = gpio_is_valid(GPIO_BUTTON1)) < 0) {
        printk(KERN_ERR "%s: Invalid GPIO %d\n", KBUILD_MODNAME, GPIO_BUTTON1);
        return res;
    }
    if ((res = gpio_request(GPIO_BUTTON1, GPIO_BUTTON1_NAME)) < 0) {
        printk(KERN_ERR "%s: GPIO request faiure: %s\n", KBUILD_MODNAME, GPIO_BUTTON1_NAME);
        return res;
    }
    if ((irq_BUTTON1 = gpio_to_irq(GPIO_BUTTON1)) < 0) {
        printk(KERN_ERR "%s: GPIO to IRQ mapping faiure %s\n", KBUILD_MODNAME, GPIO_BUTTON1_NAME);
        return irq_BUTTON1;
    }
    printk(KERN_NOTICE "%s: Mapped int %d for button1 in gpio %d\n", KBUILD_MODNAME, irq_BUTTON1, GPIO_BUTTON1);
    if ((res = request_irq(irq_BUTTON1,
                    (irq_handler_t) r_irq_handler,
                    IRQF_TRIGGER_FALLING,
                    GPIO_BUTTON1_NAME,
                    GPIO_BUTTON1_DEV)) < 0) {
        printk(KERN_ERR "%s: Irq Request failure\n", KBUILD_MODNAME);
        return res;
    }
    return 0;
}

// Module init & cleanup
static void r_cleanup(void)
{
    printk(KERN_NOTICE "%s: cleaning up\n", KBUILD_MODNAME);
    if (irq_BUTTON1 > 0) free_irq(irq_BUTTON1, GPIO_BUTTON1_DEV);
    gpio_free(GPIO_BUTTON1);

    tasklet_kill(&mytasklet);    // destroy tasklet
    printk(KERN_NOTICE "%s: removing entry /proc/%s\n", KBUILD_MODNAME, ENTRY_NAME);
    remove_proc_entry(ENTRY_NAME, NULL);
    printk(KERN_NOTICE "%s: DONE\n", KBUILD_MODNAME);
    return;
}

static int r_init(void)
{
    int res;

    printk(KERN_NOTICE "%s: module loading\n", KBUILD_MODNAME);
    value = 0;
    if ((res = r_int_config()) < 0) {
        r_cleanup();
        return res;
    }
    printk(KERN_NOTICE "%s: creating entry /proc/%s\n", KBUILD_MODNAME, ENTRY_NAME);
    if (!proc_create(ENTRY_NAME, PERMS, PARENT, &fops)) 
    {
        printk(KERN_ERR "%s: proc_create: ERROR!\n", KBUILD_MODNAME);
        remove_proc_entry(ENTRY_NAME, PARENT);
        return -ENOMEM;
    }
    return 0;
}

module_init(r_init);
module_exit(r_cleanup);

// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
