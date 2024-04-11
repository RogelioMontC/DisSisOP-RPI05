#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h>         // file operations
#include <linux/cdev.h>       // cdev structure
#include <linux/uaccess.h>    // copy to/from user space
#include <linux/gpio.h>       // gpio_get/set_value

#define DRIVER_AUTHOR "DAC-UMA"
#define DRIVER_DESC   "Device driver for berryclip leds"

//Minor=0: all leds, minor=i: i'th led (from 1 to 6)
#define NUM_LEDS         6
#define NUM_MINORS       (NUM_LEDS+1)

//GPIOS numbers as in BCM RPir_GPIO_config
#define GPIO_RED_LED1    9
#define GPIO_RED_LED2    10
#define GPIO_YELLOW_LED1 11
#define GPIO_YELLOW_LED2 17
#define GPIO_GREEN_LED1  22
#define GPIO_GREEN_LED2  27

static int gpio_led[] = { GPIO_RED_LED1, GPIO_RED_LED2,
                          GPIO_YELLOW_LED1, GPIO_YELLOW_LED2,
                          GPIO_GREEN_LED1, GPIO_GREEN_LED2 };

static char *desc_led[] = { "RED1", "RED2",
                            "YELLOW1", "YELLOW2",
                            "GREEN1", "GREEN2" };


// LEDs device file operations
static ssize_t all_leds_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    char ch;
    int minor = iminor(file->f_path.dentry->d_inode);
    int led;

    if (*ppos > 0) return count;
    if (copy_from_user(&ch, buf, 1)) return -EFAULT;
    if (minor > NUM_LEDS) return -ENOSYS;
    if (minor == 0) {
        for (led = 0; led < NUM_LEDS; led++) {
            gpio_set_value(gpio_led[led], (int)ch & 1);
        }
        printk(KERN_INFO "%s: set all leds to %d\n", KBUILD_MODNAME, (int)ch & 1);
    } else {
        printk(KERN_WARNING "%s: invalid minor number %d\n", KBUILD_MODNAME, minor);
    }
    *ppos += 1;
    return 1;
}

static ssize_t one_led_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    char ch;
    int minor = iminor(file->f_path.dentry->d_inode);
    int led;

    if (*ppos > 0) return count;
    if (copy_from_user(&ch, buf, 1)) return -EFAULT;
    if (minor > NUM_LEDS) return -ENOSYS;
    if (minor == 0) {
        printk(KERN_WARNING "%s: invalid minor number %d\n", KBUILD_MODNAME, minor);
    } else {
        led = minor - 1;
        gpio_set_value(gpio_led[led], ch & 1);
        printk(KERN_INFO "%s: set led %s to %d\n", KBUILD_MODNAME, desc_led[led], (int)ch & 1);
    }
    *ppos += 1;
    return 1;
}

static ssize_t one_led_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    int ch = 0;
    int minor = iminor(file->f_path.dentry->d_inode);
    int led;

    if (*ppos > 0) return 0;
    if (minor > NUM_LEDS) return -ENOSYS;
    if (minor == 0) {
        printk(KERN_WARNING "%s: invalid minor number %d\n", KBUILD_MODNAME, minor);
    } else {
        led = minor - 1;
        ch = gpio_get_value(gpio_led[led]);
        printk(KERN_INFO "%s: get led %s (%#x)\n", KBUILD_MODNAME, desc_led[led], ch);
    }
    ch += '0';
    if (copy_to_user(buf, &ch, 1)) return -EFAULT;
    *ppos += 1;
    return 1;
}




// struct file operations
static const struct file_operations all_leds_fops = {
    .owner = THIS_MODULE,
    .write = all_leds_write,
};

static const struct file_operations one_led_fops = {
    .owner = THIS_MODULE,
    .write = one_led_write,
    .read  = one_led_read,
};


// This functions registers devices and requests GPIOs
struct class *berryclip_class;
struct cdev berryclip_cdev[NUM_MINORS];    
dev_t dev_num;
dev_t device[NUM_MINORS];

static char *berryclip_devnode(struct device *dev, umode_t *mode)
{
        if (!mode) return NULL;
        if (dev->devt == MKDEV(MAJOR(dev_num), 0)) *mode = 0222;
        else *mode = 0666;
        return NULL;
}

static int r_devices_config(void)
{
    int i;

    // Request the kernel for N_MINOR devices
    alloc_chrdev_region(&dev_num, 0, NUM_MINORS, "berryclip");

    // Create a class : appears at /sys/class
    berryclip_class = class_create(THIS_MODULE, "berryclip_class");
    if (IS_ERR(berryclip_class)) return PTR_ERR(berryclip_class);
    berryclip_class->devnode = berryclip_devnode;
    
    // Create /dev/leds for all leds together
    cdev_init(&berryclip_cdev[0], &all_leds_fops);
    device[0] = MKDEV(MAJOR(dev_num), MINOR(dev_num));
    device_create(berryclip_class, NULL, device[0], NULL, "leds");
    cdev_add(&berryclip_cdev[0], device[0], 1);

    // Initialize and create /dev/led{i} for each of the device(cdev)
    for (i = 1; i < NUM_MINORS; i++) {

        // Associate the cdev with a set of file_operations
        //    it could be a different set of functions if needed
        cdev_init(&berryclip_cdev[i], &one_led_fops);

        // Build up the current device number. To be used further
        device[i] = MKDEV(MAJOR(dev_num), MINOR(dev_num) + i);

        // Create a device node for this device. Look, the class is
        // being used here. The same class is associated with NUM_MINOR
        // devices. Once the function returns, device nodes will be
        // created as /dev/my_dev0, /dev/my_dev1,...
        // You can also view the devices under /sys/class/my_driver_class/...
        device_create(berryclip_class, NULL, device[i], NULL, "led%d", i);

        // Now make the device live for the users to access
        cdev_add(&berryclip_cdev[i], device[i], 1); 
    }

    return 0;
}

static int r_GPIO_config(void)
{
    int i;
    int res;
    for(i = 0; i < NUM_LEDS; i++) {
        if ((res = gpio_is_valid(gpio_led[i])) < 0) {
            printk(KERN_ERR "%s:   Invalid GPIO %d\n", KBUILD_MODNAME, gpio_led[i]);
            return res;
        }
        if ((res = gpio_request_one(gpio_led[i],GPIOF_INIT_LOW,desc_led[i])) < 0) {
            printk(KERN_ERR "%s:   GPIO request failure: led %s GPIO %d\n",
					KBUILD_MODNAME, desc_led[i], gpio_led[i]);
            return res;
        }
        if ((res = gpio_direction_output(gpio_led[i], 0)) < 0) {
            printk(KERN_ERR "%s:   GPIO set direction output failure: led %s GPIO %d\n",
					KBUILD_MODNAME, desc_led[i], gpio_led[i]);
            return res;
        }
	}
	return 0;
}


// Module init & cleanup
static void r_cleanup(void)
{
    int i;
    printk(KERN_NOTICE "%s: cleaning up ", KBUILD_MODNAME);
    
    for(i = 0; i < NUM_LEDS; i++) {
        gpio_free(gpio_led[i]);
    }
    for(i = 0; i < NUM_MINORS; i++) {
        cdev_del(&berryclip_cdev[i]); // release devs[i] fields
        device_destroy(berryclip_class, device[i]);
    }
    class_destroy(berryclip_class);
    unregister_chrdev_region(MKDEV(dev_num, 0), NUM_MINORS);
    
    printk(KERN_NOTICE "%s: DONE\n", KBUILD_MODNAME);
    return;
}

static int r_init(void)
{
	int res;
    printk(KERN_NOTICE "%s: module loading\n", KBUILD_MODNAME);

    printk(KERN_NOTICE "%s:  devices config\n", KBUILD_MODNAME);
    if ((res = r_devices_config())) {
        printk(KERN_ERR "%s:  failed\n", KBUILD_MODNAME);
        return res;
    }
    printk(KERN_NOTICE "%s:  ok\n", KBUILD_MODNAME);
	
    printk(KERN_NOTICE "%s:  GPIO config\n", KBUILD_MODNAME);
    if((res = r_GPIO_config())) {
        printk(KERN_ERR "%s:  failed\n", KBUILD_MODNAME);
        r_cleanup();
        return res;
    }
    printk(KERN_NOTICE "%s:  ok\n", KBUILD_MODNAME);
	
    return 0;
}

module_init(r_init);
module_exit(r_cleanup);


// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
