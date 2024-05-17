#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/semaphore.h>

#define DRIVER_AUTHOR "Grupo J. Augusto y Rogelio"
#define DRIVER_DESC "Driver para raspberry pi. Asignatura: Diseño de Sistemas Operativos"

#define LED6 9
#define LED5 10
#define LED4 11
#define LED3 17
#define LED2 22
#define LED1 27

#define SPEAKER 4

#define BUTTON1 2
#define BUTTON2 3

static int leds[] = {LED1, LED2, LED3, LED4, LED5, LED6};
static char bounce[100];
static int bounce_cnt = 0;

static void byte2leds(char ch, int mode){
    int i;
    int val = (int) ch;
    if (mode == 0){
        for (i = 0; i<6; i++) gpio_set_value(leds[i], (val >> i) & 1);
    } else if (mode == 1){
        for (i = 0; i<6; i++) {
            if ((val >> i) & 0x01) gpio_set_value(leds[i], 1);
        }
    } else if (mode == 2){
        for (i = 0; i<6; i++) {
            if (((val >> i) & 0x01)) gpio_set_value(leds[i], 0);
        }
    } else if (mode == 3){
        for (i = 0; i<6; i++) {
            if ((val >> i) & 0x01) gpio_set_value(leds[i], 0);
            else gpio_set_value(leds[i], 1); 
        }
    }
}   

/**********
 * file operations para el driver leds
***********/

static ssize_t leds_read(struct file *file, char *buf, size_t count, loff_t *ppos){
    char copia_buffer[256];
    if(*ppos > 0) return 0;

    int len = sprintf(copia_buffer,
                        "%s: dev Leds: %i%i%i%i%i%i\n", KBUILD_MODNAME, 
                        gpio_get_value(LED6), gpio_get_value(LED5), 
                        gpio_get_value(LED4), gpio_get_value(LED3), 
                        gpio_get_value(LED2), gpio_get_value(LED1));
    if(copy_to_user(buf, copia_buffer, len)) return -EFAULT;
    *ppos += len;
    return len;
}

static ssize_t leds_write(struct file *file, const char *buf, size_t count, loff_t *ppos){
    unsigned char ch;
    if(copy_from_user(&ch, buf, 1)) return -EFAULT;
    printk(KERN_INFO "%s: dev leds: %c\n", (int)ch);
    byte2leds(ch, (ch >> 6));
    return 1;
}

static const struct file_operations leds_fops = {
    .owner = THIS_MODULE,
    .read = leds_read,
    .write = leds_write,
};

static struct miscdevice leds_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "leds",
    .fops = &leds_fops,
    .mode = S_IRUGO | S_IWUGO,
};

/**********
 * file operations para el driver speaker
***********/


static ssize_t speaker_write(struct file *file, const char *buf, size_t count, loff_t *ppos){
    unsigned char ch;
    if (copy_from_user(&ch, buf, 1)) return -EFAULT;
    printk(KERN_INFO "%s: dev Speaker: %c\n", (int)ch);
    if(ch == '1') gpio_set_value(SPEAKER, 1);
    else if(ch == '0') gpio_set_value(SPEAKER, 0);
    else return -EINVAL; 
    
    return 1;
}

static const struct file_operations speaker_fops = {
    .owner = THIS_MODULE,
    .write = speaker_write,
};

static struct miscdevice speaker_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "speaker",
    .fops = &speaker_fops,
    .mode = S_IWUGO,
};

/**********
 * file operations para el driver buttons
***********/

static short int button1_irq = 0;
static short int button2_irq = 0;
static short int button1_allow = 1;
static short int button2_allow = 1;
short int ocupado = 1;

static int time_waiting = 150;
static unsigned long jiffie;

module_param(time_waiting, int, 0);

static DEFINE_SEMAPHORE(mutex_button);
static DECLARE_WAIT_QUEUE_HEAD(wq);

static void tasklet_handler1(struct tasklet_struct *task){
    bounce[bounce_cnt] = '1';
    bounce_cnt++;
    if(bounce_cnt == 1) wake_up_interruptible(&wq);
}
static void tasklet_handler2(struct tasklet_struct *task){
    bounce[bounce_cnt] = '2';
    bounce_cnt++;
    if(bounce_cnt == 1) wake_up_interruptible(&wq);
}

static DECLARE_TASKLET(tasklet_B1, tasklet_handler1);
static DECLARE_TASKLET(tasklet_B2, tasklet_handler2);


static void timer_handler_B1(unsigned long data){
    button1_allow = 1;
}
static void timer_handler_B2(unsigned long data){
    button2_allow = 1;
}

static DEFINE_TIMER(timer_B1, timer_handler_B1);
static DEFINE_TIMER(timer_B2, timer_handler_B2);

static irqreturn_t irq_handler(int irq, void *dev_id){
    jiffie = msecs_to_jiffies(time_waiting);
    if (irq == button1_irq){
        if (button1_allow){
            button1_allow = 0;
            mod_timer(&timer_B1, jiffie + msecs_to_jiffies(time_waiting));
            tasklet_schedule(&tasklet_B1);
        }
    } else if (irq == button2_irq){
        if (button2_allow){
            button2_allow = 0;
            mod_timer(&timer_B2, jiffie + msecs_to_jiffies(time_waiting));
            tasklet_schedule(&tasklet_B2);
        }
    }
    return IRQ_HANDLED;
}
static ssize_t buttons_read(struct file *file, char *buf, size_t count, loff_t *ppos){
    char copia_buffer[sizeof(bounce) + 1];
    int len;
    if(*ppos > 0) return 0;
    if(down_interruptible(&mutex_button)) return -ERESTARTSYS;
    if(ocupado){
        ocupado = 0;
        up(&mutex_button);
        //BLOQUEO
        if(strlen(bounce) == 0){
            if(wait_event_interruptible(wq, strlen(bounce) > 0)){
                printk(KERN_INFO "Se ha interrumpido el proceso de lectura\n");
                return -ERESTARTSYS;
            }
        }
        if(down_interruptible(&mutex_button)) return -ERESTARTSYS;
        ocupado = 1;
        up(&mutex_button);
        len = sprintf(copia_buffer, "%s", bounce);
        for (int i = 0; i < bounce_cnt; i++) bounce[i] = '\0';
        bounce_cnt = 0;
    }else{
        up(&mutex_button);
        len = sprintf(copia_buffer, "Dispositivo ocupado.\n");
    }
    
    if(copy_to_user(buf, copia_buffer, len)) return -EFAULT;
    *ppos += len;

    return len;
}

static const struct file_operations buttons_fops = {
    .owner = THIS_MODULE,
    .read = buttons_read,
};

static struct miscdevice buttons_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "buttons",
    .fops = &buttons_fops,
    .mode = S_IRUGO,
};

// Inicialización de dispositivos

static int r_dev_config(void){
    if ((misc_register(&leds_dev)) < 0)printk(KERN_ERR "No se pudo registrar el dispositivo leds\n");
    else printk(KERN_INFO "Dispositivo leds registrado\n");
    
    if (misc_register(&speaker_dev) < 0) printk(KERN_ERR "No se pudo registrar el dispositivo speaker\n");
    else printk(KERN_INFO "Dispositivo speaker registrado\n");

    if (misc_register(&buttons_dev) < 0) printk(KERN_ERR "No se pudo registrar el dispositivo buttons\n");
    else printk(KERN_INFO "Dispositivo buttons registrado\n");

    return 0;
}

static void r_dev_cleanup(void){
    gpio_free(LED1);
    gpio_free(LED2);
    gpio_free(LED3);
    gpio_free(LED4);
    gpio_free(LED5);
    gpio_free(LED6);
    gpio_free(SPEAKER);
    gpio_free(BUTTON1);
    gpio_free(BUTTON2);
    
    if(leds_dev.this_device) misc_deregister(&leds_dev);
    if(speaker_dev.this_device) misc_deregister(&speaker_dev);
    if(buttons_dev.this_device) misc_deregister(&buttons_dev);

    if(button1_irq) free_irq(button1_irq, "boton 1 liberado");
    if(button2_irq) free_irq(button2_irq, "boton 2 liberado");

    del_timer(&timer_B1);
    del_timer(&timer_B2);

    tasklet_kill(&tasklet_B1);
    tasklet_kill(&tasklet_B2);

    printk(KERN_INFO "%s: Modulo descargado\n", KBUILD_MODNAME);
    return;
}

static int r_dev_init(void){
    if(r_dev_config() < 0){
        r_dev_cleanup();
        return -1;
    }

    if (gpio_request(LED1, "LED1") < 0) return -1;
    if (gpio_request(LED2, "LED2") < 0) return -1;
    if (gpio_request(LED3, "LED3") < 0) return -1;
    if (gpio_request(LED4, "LED4") < 0) return -1;
    if (gpio_request(LED5, "LED5") < 0) return -1;
    if (gpio_request(LED6, "LED6") < 0) return -1;
    if (gpio_request(SPEAKER, "SPEAKER") < 0) return -1;
    if (gpio_request(BUTTON1, "BUTTON1") < 0) return -1;
    if (gpio_request(BUTTON2, "BUTTON2") < 0) return -1;

    if (gpio_direction_output(LED1, 0) < 0) return -1;
    if (gpio_direction_output(LED2, 0) < 0) return -1;
    if (gpio_direction_output(LED3, 0) < 0) return -1;
    if (gpio_direction_output(LED4, 0) < 0) return -1;
    if (gpio_direction_output(LED5, 0) < 0) return -1;
    if (gpio_direction_output(LED6, 0) < 0) return -1;
    if (gpio_direction_output(SPEAKER, 0) < 0) return -1;

    if ((irq_B1 = gpio_to_irq(BUTTON1)) < 0){
        printk(KERN_ERR "Error al obtener la IRQ del boton 1\n");
        return button1_irq;
    }

    if ((irq_B2 = gpio_to_irq(BUTTON2)) < 0){
        printk(KERN_ERR "Error al obtener la IRQ del boton 2\n");
        return button2_irq;
    }
    int i = 0;
    if(i = request_irq(irq_B1, 
                        (irq_handler_t) irq_handler, 
                        IRQF_TRIGGER_RISING, 
                        "BOTON 1", "Boton 1 pulsado") < 0){
        printk(KERN_ERR "Error al solicitar la IRQ del boton 1\n");
        return i;
    }
    if(i = request_irq(irq_B2, 
                        (irq_handler_t) irq_handler, 
                        IRQF_TRIGGER_RISING, 
                        "BOTON 2", "Boton 2 pulsado") < 0){
        printk(KERN_ERR "Error al solicitar la IRQ del boton 2\n");
        return i;
    }

    printk(KERN_INFO "%s: Modulo cargado\n", KBUILD_MODNAME);
    return 0;
}

module_init(r_dev_init);
module_exit(r_dev_cleanup);


// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
