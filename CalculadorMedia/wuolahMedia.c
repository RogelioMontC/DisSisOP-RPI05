/*---------------------------------------------
* Diseño de Sistemas Operativos (DAC) *
para compilar el módulo:
$ MODULE=media_mod_v2 make
para instalar el módulo:
$ sudo insmod media_mod_v2.ko
para comprobar si el módulo fue cargado:
$ sudo lsmod
$ dmesg | tail
para desinstalar el módulo:
$ sudo rmmod media_mod_v2
----------------------------------------------*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

#define MAX_COOKIE_LENGTH PAGE_SIZE
static struct proc_dir_entry *proc_entry;
static int suma_numeros = 0;
static int cantidad_numeros = 0;
/****************************************************************************/
/* file operations
 */
/****************************************************************************/
// escritura del fichero
ssize_t media_write(struct file *filp, const char *buf, size_t count, loff_t *off)
{
    char copia_buffer[512];
    int space_avaiable = 512;
    int variable;
    printk(KERN_INFO "media: write\n");
    if (count > space_avaiable)
        count = space_avaiable;
    if (*off > 0)
        return 0;

    printk(KERN_INFO "media: write %d\n", count);

    if (strncmp(copia_buffer, "CLEAR", 5) == 0)
    {
        cantidad_numeros = 0;
        suma_numeros = 0;
    }
    else if (sscanf(copia_buffer, " %d ", &variable) == 1)
    {
        suma_numeros += variable;
        cantidad_numeros++;
    }
    else
    {
        return -EINVAL;
    }
    *off += count;
    return count;
}
// lectura del fichero
ssize_t media_read(struct file *filp, char __user *buf, size_t count, loff_t *off)
{
    char respuesta[1024];
    int entera, fraccion, len;
    if (*off > 0)
        return 0;
    entera = suma_numeros / cantidad_numeros;
    fraccion = (suma_numeros % cantidad_numeros) * 100 / cantidad_numeros;
    if (cantidad_numeros > 0)
    {
        len = sprintf(respuesta, "La media es: %d.%d (de %d numeros)\n",
                      entera, fraccion, cantidad_numeros);
    }
    else
    {
        len = sprintf(respuesta, "No hay respuesta\n");
    }
    if (copy_to_user(buf, respuesta, len))
    {
        return -EFAULT;
    }
    *off += len;
    return len;
}
struct file_operations proc_fops = {
    read : media_read,
    write : media_write
};
// al cargar el modulo
int init_media_module(void)
{
    int ret = 0;
    proc_entry = proc_create("media", 0666, NULL, &proc_fops);
    if (proc_entry == NULL)
    {
        ret = -ENOMEM;
        printk(KERN_INFO "media: Couldn't create proc entry\n");
    }
    else
    {
        printk(KERN_INFO "media: Module loaded.\n");
    }
    return ret;
}
// al descargar el modulo
void cleanup_media_module(void)
{
    remove_proc_entry("media", NULL);
    printk(KERN_INFO "media: Module unloaded.\n");
}
module_init(init_media_module);
module_exit(cleanup_media_module);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Media Cookie Kernel Module (DSO)");
MODULE_AUTHOR("basado en el trabajo de M. Tim Jones, adaptado para kernel >
3.10");