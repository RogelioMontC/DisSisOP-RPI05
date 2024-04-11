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
#define ENTRY_NAME "counter"
#define PERMS 0644
#define PARENT NULL
static struct task_struct *kthread;
static int counter;

// kthread run function
int counter_run(void *data) 
{
    printk(KERN_INFO "%s: in_interrup()=%d\tin_hardirq()=%d\tin_softirq()=%d\n", KBUILD_MODNAME, !!in_interrupt(), !!in_hardirq(), !!in_softirq());
    while (!kthread_should_stop()) {
        ssleep(1);	// long rem = msleep_interruptible(1000);
        // enter critical section: shared resource 'counter'
        counter++;
        // end critical section
    }
    printk(KERN_NOTICE "%s: the counter thread has terminated\n", KBUILD_MODNAME);
    return counter;
}

// proc file operations (read only)
ssize_t counter_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset)
{
    char message[20];
    int len, err;
    int local_counter;
    if (*offset > 0) return 0; // only first read allowed, offset==0
    // enter critical section: shared resource 'counter'
    local_counter = counter;
    // exit critical section
    printk(KERN_NOTICE "%s: read: value = %d\n", KBUILD_MODNAME, local_counter);
    len = sprintf(message, "%d\n", local_counter);
    err = copy_to_user(buf, message, len);
    len -= err;
    *offset += len;
    return len;
}

static struct proc_ops fops = {
    .proc_read = counter_proc_read,
};


// Module init & cleanup
int r_init(void) 
{
    printk(KERN_NOTICE "%s: module loading\n", KBUILD_MODNAME);
    counter=0;
    printk(KERN_NOTICE "%s: kthread counter created\n", KBUILD_MODNAME);
    kthread = kthread_run(counter_run, NULL, ENTRY_NAME);
    if (IS_ERR(kthread)) 
    { 
        printk(KERN_ERR "%s: kthread_run: ERROR!\n", KBUILD_MODNAME);
        return PTR_ERR(kthread);
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

void r_cleanup(void) 
{
    int ret;
    printk(KERN_NOTICE "%s: cleaning up\n", KBUILD_MODNAME);
    ret = kthread_stop(kthread);
    if (ret != -EINTR) printk(KERN_NOTICE "%s: counter thread has stopped\n", KBUILD_MODNAME);
    printk(KERN_NOTICE "%s: removing entry /proc/%s\n", KBUILD_MODNAME, ENTRY_NAME);
    remove_proc_entry(ENTRY_NAME, NULL);
    printk(KERN_NOTICE "%s: DONE\n", KBUILD_MODNAME);
    return;
}

module_init(r_init);
module_exit(r_cleanup);

// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
