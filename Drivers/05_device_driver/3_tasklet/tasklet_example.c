#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>

static void tasklet_handler(struct tasklet_struct *);

DECLARE_TASKLET(mytasklet /*nombre*/, tasklet_handler /*funcion*/);

static void tasklet_handler(struct tasklet_struct *ptr)
{
    printk(KERN_NOTICE "%s : Tasklet run at %u\n", KBUILD_MODNAME, (unsigned) jiffies);
}

static int my_init(void)
{
    printk(KERN_NOTICE "Loading module '%s'", KBUILD_MODNAME);
    printk(KERN_NOTICE "%s : Tasklet scheduled at %u\n", KBUILD_MODNAME, (unsigned) jiffies);

    tasklet_schedule(&mytasklet);

    return 0;
}

static void my_exit(void)
{
    tasklet_kill(&mytasklet);
    printk(KERN_NOTICE "Unloading module '%s'\n", KBUILD_MODNAME);
}

module_init(my_init);
module_exit(my_exit);

MODULE_DESCRIPTION("Tasklet example");
MODULE_LICENSE("GPL");

