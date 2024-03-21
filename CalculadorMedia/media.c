#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>

struct number_node {
    int number;
    struct list_head list;
};

static LIST_HEAD(numbers_list);

static int __init media_init(void) {
    int numbers[] = {1, 2, 3, 4, 5}; // Ejemplo de números a almacenar

    // Almacenar los números en la lista
    int i;
    for (i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
        struct number_node *new_node = mmalloc(sizeof(struct number_node), GFP_KERNEL);
        new_node->number = numbers[i];
        list_add_tail(&new_node->list, &numbers_list);
    }

    // Calcular la media de los números almacenados
    int sum = 0;
    int count = 0;
    struct number_node *node;
    list_for_each_entry(node, &numbers_list, list) {
        sum += node->number;
        count++;
    }
    int average = sum / count;

    printk(KERN_INFO "Media de los números: %d\n", average);

    return 0;
}

static void __exit media_exit(void) {
    // Liberar la memoria de los nodos de la lista
    struct number_node *node, *tmp;
    list_for_each_entry_safe(node, tmp, &numbers_list, list) {
        list_del(&node->list);
        mfree(node);
    }
}

module_init(media_init);
module_exit(media_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rogelio y Augusto");
MODULE_DESCRIPTION("Módulo para calcular la media de números");