#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>

#define DRIVER_AUTHOR "DAC-UMA"
#define DRIVER_DESC   "Device driver with a threaded IRQ handler for berryclip"

//GPIO numbers for button 1
#define GPIO_BUTTON1 2

// Interrupts variables
static short int irq_BUTTON1 = 0;
static short int value = 0;

// An ascii name for the claiming device, see /proc/interrupt
#define GPIO_BUTTON1_DESC           "Button 1"

// A cookie passed back to the handler function
#define GPIO_BUTTON1_DEVICE_DESC    "Berryclip"


// IRQ handler - fired on interrupt
#define MAX_GAP_IN_JIFFIES 20
static irqreturn_t irq_handler(int irq, void *dev_id)
{
    static unsigned long long last = 0;
    unsigned long long now = get_jiffies_64();
    printk(KERN_INFO "%s: in_interrup()=%d\tin_hardirq()=%d\tin_softirq()=%d\n" KBUILD_MODNAME, !!in_interrupt(), !!in_hardirq(), !!in_softirq());
    // due to switch bouncing this handler will be fired few times for every button push
    if (now - last < MAX_GAP_IN_JIFFIES) {
        printk(KERN_INFO "%s: bounce detected\n", KBUILD_MODNAME);
        return IRQ_HANDLED; // Totally handled (ignore bounce)
    }
    last = now;
    return IRQ_WAKE_THREAD; // Actual PUSH! Need to be dealed by threaded IRQ
}

// IRQ thread function - scheduled by irq_handler if needed
static irqreturn_t irq_thread_function(int irq, void *dev_id)
{
    short int local_value;
    char *cookie = (char*)dev_id;
    printk(KERN_INFO "%s: in_interrup()=%d\tin_hardirq()=%d\tin_softirq()=%d\n", KBUILD_MODNAME, !!in_interrupt(), !!in_hardirq(), !!in_softirq());

    // we will increment value with button push
    local_value = ++value;
    printk(KERN_INFO "%s: cookie: %s, value = %d\n", KBUILD_MODNAME, cookie, local_value);

    return IRQ_HANDLED; // Handled
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
    if ((irq_BUTTON1 = gpio_to_irq(GPIO_BUTTON1)) < 0) {
        printk(KERN_ERR "%s: GPIO to IRQ mapping faiure %s\n", KBUILD_MODNAME, GPIO_BUTTON1_DESC);
        return irq_BUTTON1;
    }
    printk(KERN_NOTICE "%s: Mapped int %d for button1 in gpio %d\n", KBUILD_MODNAME, irq_BUTTON1, GPIO_BUTTON1);
    if ((res = request_threaded_irq(irq_BUTTON1,
                    (irq_handler_t) irq_handler,
                    (irq_handler_t) irq_thread_function,
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
    printk(KERN_NOTICE "%s: DONE\n", KBUILD_MODNAME);
    return;
}

static int r_init(void)
{
    int res;
    printk(KERN_NOTICE "%s: module loading\n", KBUILD_MODNAME);
    printk(KERN_NOTICE "%s: intr. config\n", KBUILD_MODNAME);
    if ((res = r_int_config()))
    {
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
