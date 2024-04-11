#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>

#define DRIVER_AUTHOR "DAC-UMA"
#define DRIVER_DESC   "Workqueue example"

// Read: https://www.linuxjournal.com/article/6916 from Robert Love

// the actual queue to hold the scheduled works (create & destroy it!)
static struct workqueue_struct * my_workqueue;

typedef struct my_work_wrapper {
	struct work_struct the_work;	// the element in the workqueue
	// The actual work data
	int id;
	unsigned long long sched_time;
    unsigned long data;	// rest of the work data
} my_work_wrapper_t;

static void work_function(struct work_struct *work)
{
	struct my_work_wrapper * my_wrapper;
    printk(KERN_INFO "%s: work_function: in_interrup()=%d\tin_hardirq()=%d\tin_softirq()=%d\n", KBUILD_MODNAME, !!in_interrupt(), !!in_hardirq(), !!in_softirq());
	my_wrapper = container_of(work, struct my_work_wrapper, the_work);	// extract the wrapper from the element in the queue
    printk(KERN_NOTICE "%s: work_function: work %d executed at %lld, was scheduled at %lld\n", KBUILD_MODNAME, my_wrapper->id, get_jiffies_64(), my_wrapper->sched_time);
    // work functions may block/sleep because they are dealed by a kthread
    msleep_interruptible(my_wrapper->data);
    printk(KERN_NOTICE "%s: work_function: work %d ends at %lld", KBUILD_MODNAME, my_wrapper->id, get_jiffies_64());
    vfree(my_wrapper);
}

my_work_wrapper_t * create_work_wrapper(unsigned long data)
{
    static int count = 1;
    my_work_wrapper_t *wrapper;
    wrapper = vmalloc(sizeof(struct my_work_wrapper));
    if (!wrapper) return NULL;
    wrapper->id = count++;
    wrapper->sched_time = get_jiffies_64();
    wrapper->data = data;
    INIT_WORK(&wrapper->the_work, work_function);	// Bind the work element with the corresponding function
    return wrapper;
}

// Module init & cleanup
static int my_init(void)
{
	struct my_work_wrapper *wrapper1, *wrapper2;

	printk(KERN_NOTICE "%s: module loading\n", KBUILD_MODNAME);
	// create the work_queue
	my_workqueue = create_workqueue("my_workqueue");

	// create works (do it, for example, in an interrupt handler)
    if ((wrapper1 = create_work_wrapper(1000)) == NULL) return -ENOMEM;
    if ((wrapper2 = create_work_wrapper(10000)) == NULL) {
        vfree(wrapper1);
        return -ENOMEM;
    }
    
    // don't forget to enqueue the work in the work_queue
    queue_work(my_workqueue, &wrapper1->the_work);
    printk(KERN_NOTICE "%s: work %d scheduled at %lld\n", KBUILD_MODNAME, wrapper1->id, wrapper1->sched_time);
    queue_work(my_workqueue, &wrapper2->the_work);
    printk(KERN_NOTICE "%s: work %d scheduled at %lld\n", KBUILD_MODNAME, wrapper2->id, wrapper2->sched_time);
    // end of creation and queueing of works
    
    return 0;
}

static void my_exit(void)
{
    printk(KERN_NOTICE "%s: cleaning up, flushing workqueue\n", KBUILD_MODNAME);
    // flush queue or cancel works and then destroy the workqueue
    flush_workqueue(my_workqueue);	// block until the work is completed
    printk(KERN_NOTICE "%s: destroying workqueue\n", KBUILD_MODNAME);
    destroy_workqueue(my_workqueue);
    printk(KERN_NOTICE "%s: DONE\n", KBUILD_MODNAME);
}

module_init(my_init);
module_exit(my_exit);

// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
