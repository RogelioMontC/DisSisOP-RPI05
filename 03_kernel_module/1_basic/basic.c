#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h> 

#define DRIVER_AUTHOR "DAC-UMA"
#define DRIVER_DESC   "Loadable kernel module example"

// Module init & cleanup
int r_init(void)
{

    printk(KERN_NOTICE "Loading module '%s'\n", KBUILD_MODNAME);
    return 0;
}

void r_cleanup(void)
{
    printk(KERN_NOTICE "%s: Cleaning up module\n", KBUILD_MODNAME);
    printk(KERN_NOTICE "%s: Done\n", KBUILD_MODNAME);
    return;
}

module_init(r_init);
module_exit(r_cleanup);


// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

