#include "blkdev.h"

/**
 * Crea un dispositivo de bloques de imagen leyendo desde un archivo de imagen especificado.
 *
 * @param path la ruta al archivo de imagen
 * @return el dispositivo de bloques o NULL si no se puede abrir o leer el archivo de imagen
 */
extern struct blkdev *image_create(char *path);