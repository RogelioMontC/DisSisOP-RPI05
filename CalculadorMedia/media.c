#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/types.h>

#define PROC_ENTRY_NAME "media"
#define MAX_COOKIE_LENGTH       PAGE_SIZE

static struct proc_dir_entry *proc_entry;
static int suma; // Arreglo para guardar los numeros
static int cantidad_numeros = 0; // Cantidad de numeros en el arreglo

// Funcion para leer en el modulo
ssize_t media_read(struct file *filp, char __user *buf, size_t count, loff_t *off) {
    return 0;
}

ssize_t media_write(struct file *filp, const char *buf, size_t count, loff_t *off) {
    char copia_buffer[MAX_COOKIE_LENGTH];
    int num = 0;
    if (*off > 0)  return 0;

    if (copy_from_user(&copia_buffer, buf,count)) return -EFAULT;

    if (strncmp(copia_buffer, "CLEAR", 5) == 0) {
        cantidad_numeros = 0;
        printk(KERN_INFO "media: Numeros borrados\n");
    }else 
    {
        if (sscanf(copia_buffer, "%d", &num) > 1) {
            printk(KERN_INFO "media: Inserte solo un numero\n");
            return -EFAULT;
        }else{
            printk(KERN_INFO "media: Numero insertado: %d\n", &num);
            suma += num;
            cantidad_numeros++;
        }
    }
    
    return count;
}

// Estructura de operaciones de fichero
static struct proc_ops proc_fops = {
    .proc_read  = media_read,
    .proc_write = media_write
};

// Module initialization function
static int __init media_init(void) {
    // Create the proc file
    printk(KERN_NOTICE "Cargando el modulo: '%s'\n", KBUILD_MODNAME);
    proc_entry = proc_create(PROC_ENTRY_NAME,
                             S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,
                             NULL, &proc_fops);
    if (proc_entry == NULL)
    {
        printk(KERN_NOTICE "media: No se pudo cargar el modulo\n");
        return -ENOMEM;
    }
    else
    {
        printk(KERN_NOTICE "media: Modulo cargado.\n");
    }
    return 0;
}

// Module exit function
static void __exit media_exit(void) {
    // Remove the proc file
    remove_proc_entry("media", NULL);
    printk(KERN_NOTICE "media: Modulo eliminado.\n");
}

module_init(media_init);
module_exit(media_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rogelio y Augusto");
MODULE_DESCRIPTION("Calculador de media. Grupo J.");