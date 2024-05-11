#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/types.h>

#define PROC_ENTRY_NAME "mean"
#define MAX_COOKIE_LENGTH       PAGE_SIZE

static struct proc_dir_entry *proc_entry;

static int suma;            // Variable para guardar la suma de los números
static int contador;        // Variable para guardar la cantidad de números que tenemos

// Funcion para leer en el modulo
ssize_t mean_read(struct file *filp, char __user *buf, size_t count, loff_t *off) {
    char response[512];     // String que mostrará la respuesta en la consola
    int entero;             // Parte entera de la media que calcularemos
    int decimales;          // Parte decimal de la media que calcularemos
    int len;                // Longitud de la respuesta

    if(*off > 0) return 0;  // Evitar multiples lecturas
    if(contador <= 0)       // En caso de que el contador sea 0 o un número menor, mostrará que no se puede calcular la media
    {
        len = sprintf(response, "No hay numeros para calcular la media.\n");
    }else                   // En caso de que el contador valga más que 0, calculará la media de la siguiente forma
    {
        entero = suma / contador;   // Haremos la división entera de la suma que tenemos entre la cantidad de números 
        decimales = (suma % contador) * 100 / contador; // En decimales guardaremos el resto de la división anterior y lo multiplicaremos por 100
        len = sprintf(response, "La media de %d numeros es: %d.%d\n",contador, entero, decimales);  // Mostramos la respuesta y la guardamos en la variable len
    }

    if (copy_to_user(buf, response, len)) { // Si entra aquí, significa que se ha intentado hacer un cat vacio
        return -EFAULT;
    }

    *off += len;            // Incrementamos la variable off según el valor de len
    return len;             
}

ssize_t mean_write(struct file *filp, const char *buf, size_t count, loff_t *off) {
    int space_available = (MAX_COOKIE_LENGTH);      // Variable que indica la cantidad de espacio disponible según MAX_COOKIE_LENGTH que tiene tamaño de página
    char copia_buf[512];                            // Variable para hacer una copia del buffer
    int numero;                                     // Número con el que trabajaremos

    if (count > space_available) {                  // En caso de que count sea más grande que el espacio del que dispones, entrará aquí
        printk(KERN_INFO "mean: Buffer lleno\n");  // Mostrará un mensaje informando de la situación
        return -ENOSPC;                             
    }
    if (copy_from_user(&copia_buf, buf, count)) {   // Copia el buffer de entrada en la variable copia_buf 
        printk(KERN_INFO "mean: Error al copiar desde el usuario.\n"); // Mostrará un mensaje informando del error 
        return -EFAULT;
    }
    if (strncmp(copia_buf, "CLEAR", 5) == 0 ){      // En caso de que se introduzca la cadena CLEAR entrará aquí
        suma = 0;                                   // Ponemos la variable suma a 0
        contador = 0;                               // Ponemos el contador a 0
        printk(KERN_INFO "mean: Se ha limpiado el buffer.\n"); // Imprimimos un mensaje informando de lo hecho
        return count;                               
    }
    int heLeido = 0;                                // Variable para saber el número que se ha leido
    for (int i = 0; i < count-1; i++) {             // Bucle en el que si se lee un espacio, se ignora y si no, se opera con él
        if(copia_buf[i] != ' ' && heLeido == 0){    // En caso de que no se lea un espacio y no se halla leido ningún valor antes, entra aquí
            if(sscanf(&copia_buf[i], "%d", &numero) != 1) {     // Si se ha leido más o menos de 1 dato, manda error
                printk(KERN_INFO "mean: Error al leer el numero.\n");  // Imprime un mensaje informando de lo sucedido
                return -EINVAL;
            }else{                                              // Si lo ha leido bien, entra aquí y lee
                suma += numero;                                 // Incrementa la variable suma en el valor de numero
                contador++;                                     // Incrementa el contador porque ha leido un número
                printk(KERN_INFO "mean: %d cargado. Total: %d (%d numeros)\n", numero, suma, contador);    // Imprime el valor de la media y el total de los números
                heLeido++;                                      // Incrementamos el valor de heLeido al haber leido un dato
            }
            
        }else{                                          // En caso de entrar aquí significa que o se ha leido un espacio o un número de más de una cifra
            if(copia_buf[i] == ' ' && contador != 0){   // Si es un espacio y hay al menos un número dentro del buffer, decrementamos el valor de heLeido
                heLeido--;
            }
        }
    }
    *off += count;			    // Actualiza el offset
    return count;
}
// Estructura de operaciones de fichero
static struct proc_ops proc_fops = {
    .proc_read  = mean_read,       // Direccionamos la lectura a la función mean_read
    .proc_write = mean_write       // Direccionamos la escritura a la función mean_write
};


static int __init mean_init(void) {                                        // Creamos la entrada en proc
    printk(KERN_NOTICE "Cargando el modulo: '%s'\n", KBUILD_MODNAME);       // Informamos de que se ha cargado el módulo
    suma = 0;                                                               // Ponemos suma a 0
    contador = 0;                                                           // Ponemos contador a 0
    proc_entry = proc_create(PROC_ENTRY_NAME,                               // Crea la entrada en el directorio proc
                             S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,
                             NULL, &proc_fops);
    if (proc_entry == NULL)                                                 // Si lo anterior ha asignado un valor nulo entra imprimiendo un mensaje 
    {
        printk(KERN_NOTICE "mean: No se pudo cargar el modulo\n");         // Se informa de que hubo un error al cargar el módulo
        return -ENOMEM;
    }

    printk(KERN_NOTICE "mean: Modulo cargado.\n");                         // Si no ha habido ningún fallo, se informa de que se ha cargado el módulo
    return 0;
}


static void __exit mean_exit(void) {                   // Elimina la entrada en proc
    remove_proc_entry("mean", NULL);                   
    printk(KERN_NOTICE "mean: Modulo eliminado.\n");   // Muestra el mensaje informando de que el módulo se ha eliminado correctamente
}

module_init(mean_init);    // Direcciona la inicialización del módulo a mean_init
module_exit(mean_exit);    // Direcciona la finalización del módulo a mean_exit

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rogelio y Augusto");
MODULE_DESCRIPTION("Calculador de media. Grupo J.");