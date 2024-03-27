#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/types.h>

// Funcion para leer en el modulo
ssize_t media_read(struct file *filp, const char __user *buf, size_t count, loff_t *offp) {
    
}

ssize_t media_write(struct file *filp, const char __user *buf, size_t count, loff_t *offp) {
    
}

// Estructura de operaciones de fichero
static struct proc_ops proc_fops = {
    .proc_read  = media_read,
    .proc_write = media_write
};

// Module initialization function
static int __init media_init(void) {
    // Create the proc file
    printk(KERN_NOTICE "Loading module '%s'\n", KBUILD_MODNAME);
    proc_entry = proc_create(PROC_ENTRY_NAME,
                             S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,
                             NULL, &proc_fops);
    if (proc_entry == NULL)
    {
        printk(KERN_NOTICE "media: Couldn't create proc entry\n");
        return -ENOMEM;
    }
    else
    {
        printk(KERN_NOTICE "media: Module loaded.\n");
    }
    return 0;
}

// Module exit function
static void __exit media_exit(void) {
    // Remove the proc file
    remove_proc_entry("media", NULL);
    printk(KERN_NOTICE "media: Module unloaded.\n");
}

module_init(media_init);
module_exit(media_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rogelio y Augusto");
MODULE_DESCRIPTION("Calculador de media. Grupo J.");