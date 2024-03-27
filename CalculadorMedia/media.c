#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/types.h>

// Function to calculate the average of an array of numbers
double calculate_average(int *numbers, int size) {
    int sum = 0;
    for (int i = 0; i < size; i++) {
        sum += numbers[i];
    }
    return (double)sum / size;
}

// Function to read the input from the user and calculate the average
ssize_t read_proc(struct file *filp, char __user *buf, size_t count, loff_t *offp) {
    int numbers[100];
    int num_count = 0;
    char input[10];
    int i = 0;

    // Read the input from the user
    if (copy_from_user(input, buf, count)) {
        return -EFAULT;
    }

    // Parse the input and store the numbers in an array
    char *token = strtok(input, " ");
    while (token != NULL) {
        numbers[i++] = atoi(token);
        token = strtok(NULL, " ");
        num_count++;
    }

    // Calculate the average
    double average = calculate_average(numbers, num_count);

    // Convert the average to a string
    char result[20];
    sprintf(result, "Average: %.2f\n", average);

    // Copy the result to the user buffer
    if (copy_to_user(buf, result, strlen(result))) {
        return -EFAULT;
    }

    return strlen(result);
}

// File operations structure
static const struct file_operations proc_fops = {
    .owner = THIS_MODULE,
    .read = read_proc,
};

// Module initialization function
static int __init media_init(void) {
    // Create the proc file
    proc_create("media", 0666, NULL, &proc_fops);
    return 0;
}

// Module exit function
static void __exit media_exit(void) {
    // Remove the proc file
    remove_proc_entry("media", NULL);
}

module_init(media_init);
module_exit(media_exit);

MODULE_LICENSE("GPL");