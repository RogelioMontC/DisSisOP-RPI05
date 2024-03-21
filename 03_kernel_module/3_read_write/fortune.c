#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

#define PROC_ENTRY_NAME "fortune"
#define MAX_COOKIE_LENGTH       PAGE_SIZE

static struct proc_dir_entry *proc_entry;
static char *cookie_pot;  // Space for fortune strings
static int cookie_index;  // Index to write next fortune
static int next_fortune;  // Index to read next fortune


// file operations
ssize_t fortune_write(struct file *filp,const char *buf, size_t count, loff_t *off)
{
    int space_available = (MAX_COOKIE_LENGTH-cookie_index)+1;
    if (count > space_available) {
        printk(KERN_INFO "fortune: cookie pot is full!\n");
        return -ENOSPC;
    }
    if (copy_from_user(&cookie_pot[cookie_index], buf, count)) {
        return -EFAULT;
    }
    cookie_index += count;
    *off += count;			// update file offset
    cookie_pot[cookie_index - 1] = 0;	// end of sentence
    return count;
}

ssize_t fortune_read(struct file *filp, char __user *buf, size_t count, loff_t *off )
{
    int len;
    int pos;
  
    if (*off > 0)  return 0;	// avoid multiple reads
    if (next_fortune >= cookie_index) next_fortune = 0; // circular buffer
    // scan searching null character (end of sentence)
    for (pos = next_fortune; (cookie_pot[pos] != '\0') &&
                             (pos < MAX_COOKIE_LENGTH); pos++);
    len = pos-next_fortune;	// compute length of the sentence
    if (len >= count) len = count - 1; // limited by requested bytes (count)
    // tranfer the sentence ended with newline character
    if (copy_to_user(buf, &cookie_pot[next_fortune], len)) return -EFAULT;
    if (copy_to_user(buf+len, "\n", 1)) return -EFAULT;	// end of line
    len++;
    next_fortune += len;	// update read pointer
    *off += len + 1;		// update file offset
 
    return len + 1;		// numbers of bytes transferrend to user
}

static struct proc_ops my_fops={
    .proc_read  = fortune_read,
    .proc_write = fortune_write
};


// Module init & cleanup
int init_fortune_module( void )
{
    printk(KERN_NOTICE "Loading module '%s'\n", KBUILD_MODNAME);
    cookie_pot = (char *)vmalloc(MAX_COOKIE_LENGTH); // alloc dynamic memory in the kernel
    if (!cookie_pot) return -ENOMEM;

    memset(cookie_pot, 0, MAX_COOKIE_LENGTH);
    proc_entry = proc_create(PROC_ENTRY_NAME,
                             S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,
                             NULL, &my_fops);

    if (proc_entry == NULL) {
        vfree(cookie_pot);	// free the allocated memory
        printk(KERN_INFO "%s: Couldn't create proc entry\n", KBUILD_MODNAME);
        return -ENOMEM;
    }

    cookie_index = 0;
    next_fortune = 0;
    printk(KERN_INFO "%s: Created /proc/%s\n", KBUILD_MODNAME, PROC_ENTRY_NAME);

    return 0;
}

void cleanup_fortune_module( void )
{
    printk(KERN_INFO "%s: Cleaning up module\n", KBUILD_MODNAME);
    remove_proc_entry(PROC_ENTRY_NAME, NULL);
    vfree(cookie_pot);		// free the allocated memory
    printk(KERN_INFO "Done.\n");
}

module_init( init_fortune_module );
module_exit( cleanup_fortune_module );


// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fortune Cookie Kernel Proc Module");
MODULE_AUTHOR("M. Tim Jones, IBM. Adapted by DAC-UMA");
