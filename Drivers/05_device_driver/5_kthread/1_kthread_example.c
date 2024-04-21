#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#define DRIVER_AUTHOR "DAC-UMA"
#define DRIVER_DESC   "kthread example"

// Globals
//  proc entry
#define ENTRY_NAME "counter5"
#define PERMS 0644
#define PARENT NULL
//  kthread id & data
#define KTHREAD_NAME "kthread counter5"
static struct task_struct *kthread; // shared between r_init & r_cleanup
static char counter5_data[] = "arg passed to the thread";


// kthread run function
//  Kthread structure:
/*
int kthread_function(void *arg)
{
   int res = 0;
   while (!kthread_should_stop()) {
      some_blocking_or_sleep();
      res = do_work();
   }
   return res;
}
*/
int counter5_run(void *arg) 
{
    static int counter = 0;
    char *data = (char *)arg;
    printk(KERN_INFO "%s: %s\n", KBUILD_MODNAME, data);
    printk(KERN_INFO "%s: in_interrup()=%d\tin_hardirq()=%d\tin_softirq()=%d\n", KBUILD_MODNAME, !!in_interrupt(), !!in_hardirq(), !!in_softirq());
    while (!kthread_should_stop()) {
        ssleep(5);	// long rem = msleep_interruptible(5000);
        counter++;
        printk(KERN_INFO "%s: counter5 = %d\n", KBUILD_MODNAME, counter);
    }
    printk(KERN_NOTICE "%s: the kthread counter5 has terminated\n", KBUILD_MODNAME);
    return counter;
}

// Module init & cleanup
int r_init(void) 
{
    printk(KERN_NOTICE "%s: module loading\n", KBUILD_MODNAME);
    kthread = kthread_run(counter5_run, counter5_data, ENTRY_NAME);
    printk(KERN_NOTICE "%s: kthread counter5 created\n", KBUILD_MODNAME);
    if (IS_ERR(kthread)) 
    { 
        printk(KERN_ERR "%s: kthread_run: ERROR!\n", KBUILD_MODNAME);
        return PTR_ERR(kthread);
    }
    return 0;
}

void r_cleanup(void) 
{
    int ret;
    printk(KERN_NOTICE "%s: cleaning up\n", KBUILD_MODNAME);
    ret = kthread_stop(kthread);
    if (ret != -EINTR) printk(KERN_NOTICE "%s: counter thread has stopped\n", KBUILD_MODNAME);
    printk(KERN_NOTICE "%s: DONE\n", KBUILD_MODNAME);
    return;
}

module_init(r_init);
module_exit(r_cleanup);

// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
