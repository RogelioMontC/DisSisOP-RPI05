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

static int *ptr_num;  // Space for numbers
static int start_id;  // Index to write next number
static int next_id;  // Index to read next number

int suma(int *ptr_num, int count) {
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += ptr_num[i];
        printk(KERN_INFO "media: %d sumado. Total: %d\n", ptr_num[i], sum);
    }
    return sum;
}

// Funcion para leer en el modulo
ssize_t media_read(struct file *filp, char __user *buf, size_t count, loff_t *off) {
    char response[512];
    int entero;
    int decimales;
    int len;
    if(*off > 0) return 0; // Evitar multiples lecturas
    if(next_id <= 0){
        len = sprintf(response, "No hay numeros en el buffer.\n");
    }else{
        int contador = 1;
        if(next_id-1 != 0){
            contador = next_id-1;
        }
        int sum = suma(ptr_num, contador);
        entero = sum / contador;
        decimales = (sum % contador) * 100 / contador;
        len = sprintf(response, "La media de %d numeros es: %d.%d\n",contador, entero, decimales);
    }

    if (copy_to_user(buf, response, len)) {
        return -EFAULT;
    }
    *off += len;
    return len;
}

ssize_t media_write(struct file *filp, const char *buf, size_t count, loff_t *off) {
    int space_available = (MAX_COOKIE_LENGTH-start_id)+1;
    char copia_buf[512];
    int espacios = 0;

    if (count > space_available) {
        printk(KERN_INFO "media: Buffer lleno\n");
        return -ENOSPC;
    }
    if (copy_from_user(&copia_buf, buf, count)) {
        printk(KERN_INFO "media: Error al copiar desde el usuario.\n");
        return -EFAULT;
    }
    if (strncmp(copia_buf, "CLEAR\n", 6) == 0 ){
        start_id = 0;
        next_id = 0;
        vfree(ptr_num);
        ptr_num = (int *)vmalloc(MAX_COOKIE_LENGTH);
        printk(KERN_INFO "media: Se ha limpiado el buffer.\n");
        return count;
    }
    int heLeido = 0;
    for (int i = 0; i < count-1; i++) {
        if(copia_buf[i] != ' ' && heLeido == 0){
            if(sscanf(&copia_buf[i], "%d", &ptr_num[next_id + i - espacios]) != 1) {
                printk(KERN_INFO "media: Error al leer el numero.\n");
                return -EINVAL;
            }else{
                printk(KERN_INFO "media: %d cargado.\n", ptr_num[next_id + i - espacios]);
                heLeido++;
            }
            
        }else{
            if(copia_buf[i] == ' '){
                heLeido--;
            }
            espacios++;
        }
    }
    next_id += count-espacios;
    *off += count;			    // update file offset
    ptr_num[next_id - 1] = 0;	// end of sentence
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
    ptr_num = (int *)vmalloc(MAX_COOKIE_LENGTH);
    if (!ptr_num) return -ENOMEM;
    memset(ptr_num, 0, MAX_COOKIE_LENGTH);

    proc_entry = proc_create(PROC_ENTRY_NAME,
                             S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,
                             NULL, &proc_fops);
    if (proc_entry == NULL)
    {
        vfree(ptr_num);
        printk(KERN_NOTICE "media: No se pudo cargar el modulo\n");
        return -ENOMEM;
    }
    start_id = 0;
    next_id = 0;

    printk(KERN_NOTICE "media: Modulo cargado.\n");
    return 0;
}

// Module exit function
static void __exit media_exit(void) {
    // Remove the proc file
    remove_proc_entry("media", NULL);
    vfree(ptr_num);
    printk(KERN_NOTICE "media: Modulo eliminado.\n");
}

module_init(media_init);
module_exit(media_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rogelio y Augusto");
MODULE_DESCRIPTION("Calculador de media. Grupo J.");