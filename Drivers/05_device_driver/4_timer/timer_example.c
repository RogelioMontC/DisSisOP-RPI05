#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

#define DRIVER_AUTHOR "DAC-UMA"
#define DRIVER_DESC   "Timer example"

static unsigned long ticks_in_a_sec;

static void timer_handler(struct timer_list *);

DEFINE_TIMER(mytimer /*name*/, timer_handler /*function*/);

static void timer_handler(struct timer_list *ptr)
{
    unsigned long long now, when;
    printk(KERN_INFO "%s: in_interrup()=%d\tin_hardirq()=%d\tin_softirq()=%d\n", KBUILD_MODNAME, !!in_interrupt(), !!in_hardirq(), !!in_softirq());
    now = get_jiffies_64();
    when = now + ticks_in_a_sec * 5;
    printk(KERN_NOTICE "%s: timer run at %llu rescheduled %llu\n", KBUILD_MODNAME, now, when);
    mod_timer(&mytimer, when);
}

// Module init & cleanup
static int my_init(void)
{
    unsigned long long now, when;
    ticks_in_a_sec = msecs_to_jiffies(1000);  // compute ticks in a second
    printk(KERN_NOTICE "%s: module loading, ticks in a second: %lu\n", KBUILD_MODNAME, ticks_in_a_sec);
    now = get_jiffies_64();
    when = now + ticks_in_a_sec * 5;
    mod_timer(&mytimer, when );  // schedule after 5 seconds

    printk(KERN_NOTICE "%s: timer scheduled at %llu\n", KBUILD_MODNAME, when);

    return 0;
}

static void my_exit(void)
{
    printk(KERN_NOTICE "%s: cleaning up\n", KBUILD_MODNAME);
    del_timer(&mytimer);
    printk(KERN_NOTICE "%s: DONE\n", KBUILD_MODNAME);
}

module_init(my_init);
module_exit(my_exit);

// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

