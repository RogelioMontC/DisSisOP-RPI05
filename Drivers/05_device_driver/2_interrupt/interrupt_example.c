#include <linux/module.h>
#include <linux/kernel.h>
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
#define DRIVER_DESC   "Device driver with interrupt handler for berryclip"

//GPIO numbers for button 1
#define GPIO_BUTTON1 2

// Interrupts variables
static short int irq_BUTTON1 = 0;
static short int value = 0;

// text below will be seen in 'cat /proc/interrupt' command
#define GPIO_BUTTON1_DESC           "Button 1"

// below is optional, used in more complex code, in our case, this could be
#define GPIO_BUTTON1_DEVICE_DESC    "Berryclip"


// IRQ handler - fired on interrupt
// From https://www.linuxjournal.com/article/6916
// Interrupt handlers run with their current interrupt line disabled on
//  all processors. This ensures that two interrupt handlers for the
//  same interrupt line do not run concurrently.
//  It also prevents device driver writers from having to handle
//  recursive interrupts, which complicate programming.
static irqreturn_t r_irq_handler(int irq, void *dev_id)
{
    char *cookie = (char *)dev_id;
    printk(KERN_INFO "%s: in_interrup()=%d\tin_hardirq()=%d\tin_softirq()=%d\n", KBUILD_MODNAME, !!in_interrupt(), !!in_hardirq(), !!in_softirq());

    // we will increment value with button push
    // due to switch bouncing this handler will be fired few times for every button push
    value++;
    printk(KERN_INFO "%s: %s: value = %d\n", KBUILD_MODNAME, cookie, value);

    return IRQ_HANDLED;
}


// This functions configures interrupt for button 1
static int r_int_config(void)
{
    int res;
    if ((res = gpio_is_valid(GPIO_BUTTON1)) < 0) {
        printk(KERN_ERR "%s: Invalid GPIO %d\n", KBUILD_MODNAME, GPIO_BUTTON1);
        return res;
    }
    if ((res = gpio_request(GPIO_BUTTON1, GPIO_BUTTON1_DESC)) < 0) {
        printk(KERN_ERR "%s: GPIO request faiure: %s\n", KBUILD_MODNAME, GPIO_BUTTON1_DESC);
        return res;
    }
    if ((res = gpio_set_debounce(GPIO_BUTTON1, 200))) {
        printk(KERN_WARNING "GPIO set_debounce failure: %s, error: %d\n", GPIO_BUTTON1_DESC, res);
        printk(KERN_WARNING "errno: 524 => ENOTSUPP, Operation is not supported\n");
        //return res;   // We can live without it
    }
    if ((irq_BUTTON1 = gpio_to_irq(GPIO_BUTTON1)) < 0) {
        printk(KERN_ERR "%s: GPIO to IRQ mapping faiure %s\n", KBUILD_MODNAME, GPIO_BUTTON1_DESC);
        return irq_BUTTON1;
    }
    printk(KERN_NOTICE "%s: Mapped int %d for button1 in gpio %d\n", KBUILD_MODNAME, irq_BUTTON1, GPIO_BUTTON1);
    if ((res = request_irq(irq_BUTTON1,
                    (irq_handler_t) r_irq_handler,
                    IRQF_TRIGGER_FALLING,
                    GPIO_BUTTON1_DESC,
                    GPIO_BUTTON1_DEVICE_DESC)) < 0) {
        printk(KERN_ERR "%s: Irq Request failure\n", KBUILD_MODNAME);
        return res;
    }
    return 0;
}


// Module init & cleanup
static void r_cleanup(void)
{
    printk(KERN_NOTICE "%s: cleaning up\n", KBUILD_MODNAME);
    if (irq_BUTTON1 > 0) free_irq(irq_BUTTON1, GPIO_BUTTON1_DEVICE_DESC);
    gpio_free(GPIO_BUTTON1);
    printk(KERN_NOTICE "%s: Done.\n", KBUILD_MODNAME);
    return;
}

static int r_init(void)
{
    int res;
    printk(KERN_NOTICE "%s: module loading\n", KBUILD_MODNAME);
    if ((res = r_int_config()) < 0) {
        r_cleanup();
        return res;
    }
    return 0;
}

module_init(r_init);
module_exit(r_cleanup);

// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
