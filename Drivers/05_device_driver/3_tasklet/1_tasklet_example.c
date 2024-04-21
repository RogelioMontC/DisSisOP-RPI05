#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>

#define DRIVER_AUTHOR "DAC-UMA"
#define DRIVER_DESC   "Tasklet example"

static void tasklet_handler(struct tasklet_struct *);
DECLARE_TASKLET(mytasklet /*nombre*/, tasklet_handler /*funcion*/);

static void tasklet_handler(struct tasklet_struct *ptr)
{
    printk(KERN_INFO "%s: in_interrup()=%d\tin_hardirq()=%d\tin_softirq()=%d\n", KBUILD_MODNAME, !!in_interrupt(), !!in_hardirq(), !!in_softirq());
    printk(KERN_NOTICE "%s: tasklet run at %llu\n", KBUILD_MODNAME, get_jiffies_64());
    // tasklets can NOT block/sleep (not in task context)
}

// Module init & cleanup
static int my_init(void)
{
    printk(KERN_NOTICE "%s: module loading", KBUILD_MODNAME);
    tasklet_schedule(&mytasklet);   // launch deferred job
    printk(KERN_NOTICE "%s: tasklet scheduled at %llu\n", KBUILD_MODNAME, get_jiffies_64());
    return 0;
}

static void my_exit(void)
{
    printk(KERN_NOTICE "%s: cleaning up\n", KBUILD_MODNAME);
    tasklet_kill(&mytasklet);
    printk(KERN_NOTICE "%s: DONE\n", KBUILD_MODNAME);
}

module_init(my_init);
module_exit(my_exit);

// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
