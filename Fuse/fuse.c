#define FUSE_USE_VERSION 27

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/select.h>

#include "fuseStructures.h"
#include "blkdev.h"

/* 
 * acceso al disco - la variable global 'disk' apunta a una estructura blkdev
 * que ha sido inicializada para acceder al archivo de imagen.
 *
 * NOTA - el acceso a blkdev se realiza en bloques de 1024 bytes
 */
extern struct blkdev *disk;

/* al definir los mapas de bits como punteros a 'fd_set', se pueden utilizar
 * macros existentes para manejarlos.
 *   FD_ISSET(##, inode_map);
 *   FD_CLR(##, block_map);
 *   FD_SET(##, block_map);
 */

/** puntero al mapa de bits de inodos para determinar inodos libres */
static fd_set *inode_map;
static int     inode_map_base;

/** puntero a los bloques de inodos */
static struct fs_inode *inodes;
/** número de inodos del superbloque */
static int   n_inodes;
/** número del primer bloque de inodos */
static int   inode_base;

/** puntero al mapa de bits de bloques para determinar bloques libres */
fd_set *block_map;
/** número del primer bloque de datos */
static int     block_map_base;

/** número de bloques disponibles del superbloque */
static int   n_blocks;

/** número de inodo raíz del superbloque */
static int   root_inode;

/** array de bloques de metadatos sucios para escribir -- opcional */
static void **dirty;

/** longitud del array dirty -- opcional */
static int    dirty_len;

/** tamaño total de los bloques directos */
static int DIR_SIZE = BLOCK_SIZE * N_DIRECT;
static int INDIR1_SIZE = (BLOCK_SIZE / sizeof(uint32_t)) * BLOCK_SIZE;
static int INDIR2_SIZE = (BLOCK_SIZE / sizeof(uint32_t)) * (BLOCK_SIZE / sizeof(uint32_t)) * BLOCK_SIZE;

/* Funciones sugeridas para implementar - puedes ignorarlas
 * e implementar las tuyas propias si lo deseas
 */

/**
 * Encuentra el inodo para una entrada de directorio existente.
 *
 * @param fs_dirent puntero al primer dirent en el directorio
 * @param name el nombre de la entrada de directorio
 * @return el inodo de la entrada, o 0 si no se encuentra.
 */
static int find_in_dir(struct fs_dirent *de, char *name)
{
    for (int i = 0; i < DIRENTS_PER_BLK; i++) {
        // encontrado, devuelve su inodo
        if (de[i].valid && strcmp(de[i].name, name) == 0) {
            return de[i].inode;
        }
    }
    return 0;
}

/**
 * Busca una sola entrada de directorio en un directorio.
 *
 * Errores
 *   -EIO     - error al leer el bloque
 *   -ENOENT  - un componente de la ruta no está presente.
 *   -ENOTDIR - el componente intermedio de la ruta no es un directorio
 *
 */
static int lookup(int inum, char *name)
{
    // obtiene el directorio correspondiente
    struct fs_inode cur_dir = inodes[inum];
    // inicializa las entradas del buffer
    struct fs_dirent entries[DIRENTS_PER_BLK];
    memset(entries, 0, DIRENTS_PER_BLK * sizeof(struct fs_dirent));
    if (disk->ops->read(disk, cur_dir.direct[0], 1, &entries) < 0) exit(1);
    int inode = find_in_dir(entries, name);
    return inode == 0 ? -ENOENT : inode;
}

/**
 * Analiza el nombre de la ruta en tokens, como máximo nnames tokens después
 * de normalizar las rutas eliminando los elementos '.' y '..'.
 *
 * Si names es NULL, la ruta no se altera y la función devuelve
 * la cantidad de nombres de ruta. De lo contrario, la ruta se altera mediante strtok()
 * y la función devuelve los nombres en el array names, que apuntan
 * a elementos de la cadena de ruta.
 *
 * @param path la ruta del directorio
 * @param names el array de tokens de argumento o NULL
 * @param nnames el número máximo de nombres, 0 = ilimitado
 * @return el número de tokens de nombre de ruta
 */
static int parse(char *path, char *names[], int nnames)
{
    char *_path = strdup(path);
    int count = 0;
    char *token = strtok(_path, "/");
    while (token != NULL) {
        if (strlen(token) > FS_FILENAME_SIZE - 1) return -EINVAL;
        if (strcmp(token, "..") == 0 && count > 0) count--;
        else if (strcmp(token, ".") != 0) {
            if (names != NULL && count < nnames) {
                names[count] = (char*)malloc(sizeof(char*));
                memset(names[count], 0, sizeof(char*));
                strcpy(names[count], token);
            }
            count++;
        }
        token = strtok(NULL, "/");  // obtiene el siguiente token
    }
    // si el número de nombres en la ruta excede el máximo
    if (nnames != 0 && count > nnames) return -1;
    return count;
}

/**
 * libera el array de punteros a char asignado
 * @param arr array a liberar
 */
static void free_char_ptr_array(char *arr[], int len) {
    for (int i = 0; i < len; i++) {
        free(arr[i]);
    }
}

/**
 * Devuelve el número de inodo para el archivo o
 * directorio especificado.
 *
 * Errores
 *   -ENOENT  - un componente de la ruta no está presente.
 *   -ENOTDIR - un componente intermedio de la ruta no es un directorio
 *
 * @param path la ruta del archivo
 * @return inodo del nodo de la ruta o error
 */
static int translate(char *path)
{
    if (strcmp(path, "/") == 0 || strlen(path) == 0) return root_inode;
    int inode_idx = root_inode;
    // obtiene el número de nombres
    int num_names = parse(path, NULL, 0);
    // si el número de nombres en la ruta excede el máximo, devuelve un error, tipo de error a corregir si es necesario
    if (num_names < 0) return -ENOTDIR;
    if (num_names == 0) return root_inode;
    // copia todos los nombres
    char *names[num_names];
    parse(path, names, num_names);
    // busca el inodo

    for (int i = 0; i < num_names; i++) {
        // si el token no es un directorio, devuelve un error
        if (!S_ISDIR(inodes[inode_idx].mode)) {
            free_char_ptr_array(names, num_names);
            return -ENOTDIR;
        }
        // busca y registra el inodo
        inode_idx = lookup(inode_idx, names[i]);
        if (inode_idx < 0) {
            free_char_ptr_array(names, num_names);
            return -ENOENT;
        }
    }
    free_char_ptr_array(names, num_names);
    return inode_idx;
}

/**
 *  Devuelve el número de inodo para la ruta al archivo
 *  o directorio, y un nombre de hoja que puede no existir todavía.
 *
 * Errores
 *   -ENOENT  - un componente de la ruta no está presente.
 *   -ENOTDIR - un componente intermedio de la ruta no es un directorio
 *
 * @param path la ruta del archivo
 * @param leaf puntero al espacio para el nombre de hoja de tamaño FS_FILENAME_SIZE
 * @return inodo del nodo de la ruta o error
 */
static int translate_1(char *path, char *leaf)
{
    if (strcmp(path, "/") == 0 || strlen(path) == 0) return root_inode;
    int inode_idx = root_inode;
    // obtiene el número de nombres
    int num_names = parse(path, NULL, 0);
    // si el número de nombres en la ruta excede el máximo, devuelve un error, tipo de error a corregir si es necesario
    if (num_names < 0) return -ENOTDIR;
    if (num_names == 0) return root_inode;
    // copia todos los nombres
    char *names[num_names];
    parse(path, names, num_names);
    // busca el inodo

    for (int i = 0; i < num_names - 1; i++) {
        // si el token no es un directorio, devuelve un error
        if (!S_ISDIR(inodes[inode_idx].mode)) {
            free_char_ptr_array(names, num_names);
            return -ENOTDIR;
        }
        // busca y registra el inodo
        inode_idx = lookup(inode_idx, names[i]);
        if (inode_idx < 0) {
            free_char_ptr_array(names, num_names);
            return -ENOENT;
        }
    }
    strcpy(leaf, names[num_names - 1]);
    free_char_ptr_array(names, num_names);
    return inode_idx;
}

/**
 * Marca un inodo como sucio.
 *
 * @param in puntero a un inodo
 */
static void mark_inode(struct fs_inode *in)
{
    int inum = in - inodes;
    int blk = inum / INODES_PER_BLK;
    dirty[inode_base + blk] = (void*)inodes + blk * FS_BLOCK_SIZE;
}

/**
 * Escribe los bloques de metadatos sucios en el disco.
 */
void flush_metadata(void)
{
    int i;
    for (i = 0; i < dirty_len; i++) {
        if (dirty[i]) {
            disk->ops->write(disk, i, 1, dirty[i]);
            dirty[i] = NULL;
        }
    }
}

/**
 * Cuenta el número de bloques libres
 * @return número de bloques libres
 */
int num_free_blk() {
    int count = 0;
    for (int i = 0; i < n_blocks; i++) {
        if (!FD_ISSET(i, block_map)) {
            count++;
        }
    }
    return count;
}

/**
 * Devuelve un número de bloque libre o -ENOSPC si no hay ninguno disponible.
 *
 * @return número de bloque libre o -ENOSPC si no hay ninguno disponible
 */
static int get_free_blk(void)
{
    for (int i = 0; i < n_blocks; i++) {
        if (!FD_ISSET(i, block_map)) {
            char buff[BLOCK_SIZE];
            memset(buff, 0, BLOCK_SIZE);
            if (disk->ops->write(disk, i, 1, buff) < 0) exit(1);
            FD_SET(i, block_map);
            return i;
        }
    }
    return -ENOSPC;
}

/**
 * Devuelve un bloque a la lista de bloques libres
 *
 * @param  blkno el número de bloque
 */
static void return_blk(int blkno)
{
    FD_CLR(blkno, block_map);
}

static void update_blk(void)
{
    if (disk->ops->write(disk,
                         block_map_base,
                         inode_base - block_map_base,
                         block_map) < 0)
        exit(1);
}

/**
 * Devuelve un número de inodo libre
 *
 * @return un número de inodo libre o -ENOSPC si no hay ninguno disponible
 */
static int get_free_inode(void)
{
    for (int i = 2; i < n_inodes; i++) {
        if (!FD_ISSET(i, inode_map)) {
            FD_SET(i, inode_map);
            return i;
        }
    }
    return -ENOSPC;
}

/**
 * Devuelve un inodo a la lista de inodos libres.
 *
 * @param  inum el número de inodo
 */
static void return_inode(int inum)
{
    FD_CLR(inum, inode_map);
}

static void update_inode(int inum)
{
    if (disk->ops->write(disk,
                         inode_base + inum / INODES_PER_BLK,
                         1,
                         &inodes[inum - (inum % INODES_PER_BLK)]) < 0)
        exit(1);
    if (disk->ops->write(disk,
                         inode_map_base,
                         block_map_base - inode_map_base,
                         inode_map) < 0)
        exit(1);
}

/**
 * Encuentra una entrada de directorio libre.
 *
 * @return el índice de la entrada libre del directorio o -ENOSPC
 *   si no hay espacio para una nueva entrada en el directorio
 */
static int find_free_dir(struct fs_dirent *de)
{
    for (int i = 0; i < DIRENTS_PER_BLK; i++) {
        if (!de[i].valid) {
            return i;
        }
    }
    return -ENOSPC;
}

/**
 * Determina si el directorio está vacío.
 *
 * @param de puntero a la primera entrada del directorio
 * @return 1 si está vacío, 0 si tiene entradas
 */
static int is_empty_dir(struct fs_dirent *de)
{
    for (int i = 0; i < DIRENTS_PER_BLK; i++) {
        if (de[i].valid) {
            return 0;
        }
    }
    return 1;
}

/**
 * Copia el stat desde el inodo a sb
 * @param inode inodo a copiar desde
 * @param sb holder para contener el stat copiado
 * @param inode_idx inode_idx
 */
static void cpy_stat(struct fs_inode *inode, struct stat *sb) {
    memset(sb, 0, sizeof(*sb));
    sb->st_uid = inode->uid;
    sb->st_gid = inode->gid;
    sb->st_mode = (mode_t) inode->mode;
    sb->st_atime = inode->mtime;
    sb->st_ctime = inode->ctime;
    sb->st_mtime = inode->mtime;
    sb->st_size = inode->size;
    sb->st_blksize = FS_BLOCK_SIZE;
    sb->st_nlink = 1;
    sb->st_blocks = (inode->size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
}

/* Funciones de Fuse
 */

/**
 * init - esto se llama una vez por el marco de trabajo de FUSE al inicio.
 *
 * Este es un buen lugar para leer el superbloque y configurar cualquier
 * variables globales que necesites. No tienes que preocuparte por el
 * argumento o el valor de retorno.
 *
 * @param conn información de conexión de Fuse - no utilizado
 * @return no utilizado - devuelve NULL
 */
void* fs_init(struct fuse_conn_info *conn)
{
    // leer el superbloque
    struct fs_super sb;
    if (disk->ops->read(disk, 0, 1, &sb) < 0) {
        exit(1);
    }

    root_inode = sb.root_inode;

    /* El mapa de inodos y el mapa de bloques se escriben directamente en el disco después del superbloque */

    // leer el mapa de inodos
    inode_map_base = 1;
    inode_map = malloc(sb.inode_map_sz * FS_BLOCK_SIZE);
    if (disk->ops->read(disk, inode_map_base, sb.inode_map_sz, inode_map) < 0) {
        exit(1);
    }

    // leer el mapa de bloques
    block_map_base = inode_map_base + sb.inode_map_sz;
    block_map = malloc(sb.block_map_sz * FS_BLOCK_SIZE);
    if (disk->ops->read(disk, block_map_base, sb.block_map_sz, block_map) < 0) {
        exit(1);
    }

    /* Los datos de los inodos se escriben en el siguiente conjunto de bloques */
    inode_base = block_map_base + sb.block_map_sz;
    n_inodes = sb.inode_region_sz * INODES_PER_BLK;
    inodes = malloc(sb.inode_region_sz * FS_BLOCK_SIZE);
    if (disk->ops->read(disk, inode_base, sb.inode_region_sz, inodes) < 0) {
        exit(1);
    }

    // número de bloques en el dispositivo
    n_blocks = sb.num_blocks;

    // bloques de metadatos sucios
    dirty_len = inode_base + sb.inode_region_sz;
    dirty = calloc(dirty_len*sizeof(void*), 1);

    /* tu código aquí */

    return NULL;
}

/* Nota sobre errores de traducción de ruta:
 * Además de los errores específicos de cada método que se enumeran a continuación, casi
 * cada método puede devolver uno de los siguientes errores si no puede
 * localizar un archivo o directorio correspondiente a una ruta especificada.
 *
 * ENOENT - un componente de la ruta no está presente.
 * ENOTDIR - un componente intermedio de la ruta (por ejemplo, 'b' en
 *           /a/b/c) no es un directorio
 */

/* nota sobre la división de la variable 'path':
 * el valor pasado por el marco de trabajo de FUSE se declara como 'const',
 * lo que significa que no puedes modificarlo. Los mecanismos estándar para
 * dividir cadenas en C (strtok, strsep) modifican la cadena en su lugar,
 * por lo que tienes que copiar la cadena y luego liberar la copia cuando hayas terminado. Una forma de hacer esto:
 *
 *    char *_path = strdup(path);
 *    int inum = translate(_path);
 *    free(_path);
 */

/**
 * getattr - obtener atributos de un archivo o directorio. Para una descripción de
 * los campos en 'struct stat', consulta 'man lstat'.
 *
 * Nota - los campos no proporcionados en CS5600fs son:
 *    st_nlink - siempre se establece en 1
 *    st_atime, st_ctime - se establecen en el mismo valor que st_mtime
 *
 * Errores
 *   -ENOENT  - un componente de la ruta no está presente.
 *   -ENOTDIR - un componente intermedio de la ruta no es un directorio
 *
 * @param path la ruta del archivo
 * @param sb puntero a la estructura stat
 * @return 0 si tiene éxito, o el número de error
 */
static int fs_getattr(const char *path, struct stat *sb)
{
    fs_init(NULL);

    char *_path = strdup(path);
    int inode_idx = translate(_path);
    if (inode_idx < 0) return inode_idx;
    struct fs_inode* inode = &inodes[inode_idx];
    cpy_stat(inode, sb);
    return SUCCESS;
}

/**
 * readdir - obtener el contenido de un directorio.
 *
 * Para cada entrada en el directorio, invoca la función 'filler',
 * que se pasa como un puntero a función, de la siguiente manera:
 *     filler(buf, <nombre>, <statbuf>, 0)
 * donde <statbuf> es una estructura stat, al igual que en getattr.
 *
 * Errores
 *   -ENOENT  - un componente de la ruta no está presente.
 *   -ENOTDIR - un componente intermedio de la ruta no es un directorio
 *
 * @param path la ruta del directorio
 * @param ptr  puntero al búfer de relleno
 * @param filler función de relleno para llamar en cada entrada
 * @param offset el desplazamiento del archivo - no utilizado
 * @param fi la información del archivo de Fuse
 */
static int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi)
{
    char *_path = strdup(path);
    int inode_idx = translate(_path);
    if (inode_idx < 0) return inode_idx;
    struct fs_inode *inode = &inodes[inode_idx];
    if (!S_ISDIR(inode->mode)) return -ENOTDIR;
    struct fs_dirent entries[DIRENTS_PER_BLK];
    memset(entries, 0, DIRENTS_PER_BLK * sizeof(struct fs_dirent));
    struct stat sb;
    if (disk->ops->read(disk, inode->direct[0], 1, entries) < 0) exit(1);
    for (int i = 0; i < DIRENTS_PER_BLK; i++) {
        if (entries[i].valid) {
            cpy_stat(&inodes[entries[i].inode], &sb);
            filler(ptr, entries[i].name, &sb, 0);
        }
    }
    return SUCCESS;
}

/**
 * open - abrir un directorio de archivos.
 *
 * Puedes guardar información sobre el directorio abierto en
 * fi->fh. Si asignas memoria, libérala en fs_releasedir.
 *
 * Errores
 *   -ENOENT  - un componente de la ruta no está presente.
 *   -ENOTDIR - un componente intermedio de la ruta no es un directorio
 *
 * @param path la ruta del archivo
 * @param fi información del sistema de archivos de Fuse
 */
static int fs_opendir(const char *path, struct fuse_file_info *fi)
{
    char *_path = strdup(path);
    int inode_idx = translate(_path);
    if (inode_idx < 0) return inode_idx;
    if (!S_ISDIR(inodes[inode_idx].mode)) return -ENOTDIR;
    fi->fh = (uint64_t) inode_idx;
    return SUCCESS;
}

/**
 * Liberar recursos cuando se cierra el directorio.
 * Si asignas memoria en fs_opendir, libérala aquí.
 *
 * @param path la ruta del directorio
 * @param fi información del sistema de archivos de Fuse
 */
static int fs_releasedir(const char *path, struct fuse_file_info *fi)
{
    char *_path = strdup(path);
    int inode_idx = translate(_path);
    if (inode_idx < 0) return inode_idx;
    if (!S_ISDIR(inodes[inode_idx].mode)) return -ENOTDIR;
    fi->fh = (uint64_t) -1;
    return SUCCESS;
}

static int set_attributes_and_update(struct fs_dirent *de, char *name, mode_t mode, bool isDir)
{
    // obtener directorio e inodo libre
    int freed = find_free_dir(de);
    int freei = get_free_inode();
    int freeb = isDir ? get_free_blk() : 0;
    if (freed < 0 || freei < 0 || freeb < 0) return -ENOSPC;
    struct fs_dirent *dir = &de[freed];
    struct fs_inode *inode = &inodes[freei];
    strcpy(dir->name, name);
    dir->inode = freei;
    dir->isDir = true;
    dir->valid = true;
    inode->uid = getuid();
    inode->gid = getgid();
    inode->mode = mode;
    inode->ctime = inode->mtime = time(NULL);
    inode->size = 0;
    inode->direct[0] = freeb;
    // actualizar mapa e inodo
    update_inode(freei);
    update_blk();
    return SUCCESS;
}

/**
 * mknod - crear un nuevo archivo con permisos (mode & 01777)
 * números de dispositivo menores extraídos de mode. Comportamiento indefinido
 * cuando se utilizan bits de modo que no sean los 9 bits bajos.
 *
 * Los permisos de acceso de la ruta están limitados por el
 * umask(2) del proceso padre.
 *
 * Errores
 *   -ENOTDIR  - componente de la ruta no es un directorio
 *   -EEXIST   - el archivo ya existe
 *   -ENOSPC   - inodo libre no disponible
 *   -ENOSPC   - resulta en >32 entradas en el directorio
 *
 * @param path la ruta del archivo
 * @param mode el modo, indicando archivo especial de bloque o de caracteres
 * @param dev la especificación del dispositivo de E/S de caracteres o bloques
 */
static int fs_mknod(const char *path, mode_t mode, dev_t dev)
{
    // obtener inodos actual y padre
    mode |= S_IFREG;
    if (!S_ISREG(mode) || strcmp(path, "/") == 0) return -EINVAL;
    char *_path = strdup(path);
    char name[FS_FILENAME_SIZE];
    int inode_idx = translate(_path);
    int parent_inode_idx = translate_1(_path, name);
    if (inode_idx >= 0) return -EEXIST;
    if (parent_inode_idx < 0) return parent_inode_idx;
    // leer información del padre
    struct fs_inode *parent_inode = &inodes[parent_inode_idx];
    if (!(S_ISDIR(parent_inode->mode))) return -ENOTDIR;

    struct fs_dirent entries[DIRENTS_PER_BLK];
    memset(entries, 0, DIRENTS_PER_BLK * sizeof(struct fs_dirent));
    if (disk->ops->read(disk, parent_inode->direct[0], 1, entries) < 0)
        exit(1);
    // asignar inodo y directorio y actualizar
    int res = set_attributes_and_update(entries, name, mode, false);
    if (res < 0) return res;

    // escribir el búfer de entradas en el disco
    if (disk->ops->write(disk, parent_inode->direct[0], 1, entries) < 0)
        exit(1);
    return SUCCESS;
}

/**
 *  mkdir - crear un directorio con el modo dado. Comportamiento
 *  indefinido cuando se utilizan bits de modo que no sean los 9 bits bajos.
 *
 * Errores
 *   -ENOTDIR  - componente de la ruta no es un directorio
 *   -EEXIST   - el directorio ya existe
 *   -ENOSPC   - inodo libre no disponible
 *   -ENOSPC   - resulta en >32 entradas en el directorio
 *
 * @param path ruta del archivo
 * @param mode el modo para el nuevo directorio
 * @return 0 si tiene éxito, o el valor de error
 */ 
static int fs_mkdir(const char *path, mode_t mode)
{
    // obtener inodos actual y padre
    mode |= S_IFDIR;
    if (!S_ISDIR(mode) || strcmp(path, "/") == 0) return -EINVAL;
    char *_path = strdup(path);
    char name[FS_FILENAME_SIZE];
    int inode_idx = translate(_path);
    int parent_inode_idx = translate_1(_path, name);
    if (inode_idx >= 0) return -EEXIST;
    if (parent_inode_idx < 0) return parent_inode_idx;
    // leer información del padre
    struct fs_inode *parent_inode = &inodes[parent_inode_idx];
    if (!S_ISDIR(parent_inode->mode)) return -ENOTDIR;

    struct fs_dirent entries[DIRENTS_PER_BLK];
    memset(entries, 0, DIRENTS_PER_BLK * sizeof(struct fs_dirent));
    if (disk->ops->read(disk, parent_inode->direct[0], 1, entries) < 0)
        exit(1);
    // asignar inodo y directorio y actualizar
    int res = set_attributes_and_update(entries, name, mode, true);
    if (res < 0) return res;

    // escribir el búfer de entradas en el disco
    if (disk->ops->write(disk, parent_inode->direct[0], 1, entries) < 0)
        exit(1);
    return SUCCESS;
}

static void fs_truncate_dir(uint32_t *de) {
    for (int i = 0; i < N_DIRECT; i++) {
        if (de[i]) return_blk(de[i]);
        de[i] = 0;
    }
}

static void fs_truncate_indir1(int blk_num) {
    uint32_t entries[PTRS_PER_BLK];
    memset(entries, 0, PTRS_PER_BLK * sizeof(uint32_t));
    if (disk->ops->read(disk, blk_num, 1, entries) < 0)
        exit(1);
    // borrar cada bloque y eliminarlo del mapa de bloques
    for (int i = 0; i < PTRS_PER_BLK; i++) {
        if (entries[i]) return_blk(entries[i]);
        entries[i] = 0;
    }
}

static void fs_truncate_indir2(int blk_num) {
    uint32_t entries[PTRS_PER_BLK];
    memset(entries, 0, PTRS_PER_BLK * sizeof(uint32_t));
    if (disk->ops->read(disk, blk_num, 1, entries) < 0)
        exit(1);
    // borrar cada enlace doble
    for (int i = 0; i < PTRS_PER_BLK; i++) {
        if (entries[i]) fs_truncate_indir1(entries[i]);
        entries[i] = 0;
    }
}

/**
 * truncate - truncar el archivo a exactamente 'len' bytes.
 *
 * Errores:
 *   ENOENT  - el archivo no existe
 *   ENOTDIR - componente de la ruta no es un directorio
 *   EINVAL  - longitud no válida (solo admite 0)
 *   EISDIR	 - la ruta es un directorio (solo archivos)
 *
 * @param path la ruta del archivo
 * @param len la longitud
 * @return 0 si tiene éxito, o el valor de error
 */
static int fs_truncate(const char *path, off_t len)
{
    /* puedes hacer trampa implementando esto solo para el caso de len==0,
     * y un error en caso contrario.
     */
    // hacer trampa
    if (len != 0) return -EINVAL;		/* argumento no válido */

    // obtener inodo
    char *_path = strdup(path);
    int inode_idx = translate(_path);
    if (inode_idx < 0) return inode_idx;
    struct fs_inode *inode = &inodes[inode_idx];
    if (S_ISDIR(inode->mode)) return -EISDIR;

    // borrar directos
    fs_truncate_dir(inode->direct);

    // borrar indirecto1
    if (inode->indir_1) {
        fs_truncate_indir1(inode->indir_1);
        return_blk(inode->indir_1);
    }
    inode->indir_1 = 0;

    // borrar indirecto2
    if (inode->indir_2) {
        fs_truncate_indir2(inode->indir_2);
        return_blk(inode->indir_2);
    }
    inode->indir_2 = 0;

    inode->size = 0;

    // actualizar al final para mayor eficiencia
    update_inode(inode_idx);
    update_blk();

    return SUCCESS;
}

/**
 * unlink - eliminar un archivo.
 *
 * Errores
 *   -ENOENT   - el archivo no existe
 *   -ENOTDIR  - componente de la ruta no es un directorio
 *   -EISDIR   - no se puede eliminar un directorio
 *
 * @param path ruta del archivo
 * @return 0 si tiene éxito, o el valor de error
 */
static int fs_unlink(const char *path)
{
    // truncar primero
    int res = fs_truncate(path, 0);
    if (res < 0) return res;

    // obtener inodos y verificar
    char *_path = strdup(path);
    char name[FS_FILENAME_SIZE];
    int inode_idx = translate(_path);
    int parent_inode_idx = translate_1(_path, name);
    struct fs_inode *inode = &inodes[inode_idx];
    struct fs_inode *parent_inode = &inodes[parent_inode_idx];
    if (inode_idx < 0 || parent_inode_idx < 0) return -ENOENT;
    if (S_ISDIR(inode->mode)) return -EISDIR;
    if (!S_ISDIR(parent_inode->mode)) return -ENOTDIR;

    // eliminar entrada completa del directorio padre
    struct fs_dirent entries[DIRENTS_PER_BLK];
    memset(entries, 0, DIRENTS_PER_BLK * sizeof(struct fs_dirent));
    if (disk->ops->read(disk, parent_inode->direct[0], 1, entries) < 0)
        exit(1);
    for (int i = 0; i < DIRENTS_PER_BLK; i++) {
        if (entries[i].valid && strcmp(entries[i].name, name) == 0) {
            memset(&entries[i], 0, sizeof(struct fs_dirent));
        }
    }
    if (disk->ops->write(disk, parent_inode->direct[0], 1, entries) < 0)
        exit(1);

    // borrar inodo
    memset(inode, 0, sizeof(struct fs_inode));
    return_inode(inode_idx);

    // actualizar
    update_inode(inode_idx);
    update_blk();

    return SUCCESS;
}

/**
 * rmdir - eliminar un directorio.
 *
 * Errores:
 *   -ENOENT   - el archivo no existe
 *   -ENOTDIR  - el componente de la ruta no es un directorio
 *   -ENOTDIR  - la ruta no es un directorio
 *   -ENOEMPTY - el directorio no está vacío
 *
 * @param path la ruta del directorio
 * @return 0 si tiene éxito, o el valor de error
 */
static int fs_rmdir(const char *path)
{
    // no se puede eliminar el directorio raíz
    if (strcmp(path, "/") == 0) return -EINVAL;

    // obtener inodos y verificar
    char *_path = strdup(path);
    char name[FS_FILENAME_SIZE];
    int inode_idx = translate(_path);
    int parent_inode_idx = translate_1(_path, name);
    struct fs_inode *inode = &inodes[inode_idx];
    struct fs_inode *parent_inode = &inodes[parent_inode_idx];
    if (inode_idx < 0 || parent_inode_idx < 0) return -ENOENT;
    if (!S_ISDIR(inode->mode)) return -ENOTDIR;
    if (!S_ISDIR(parent_inode->mode)) return -ENOTDIR;

    // verificar si el directorio está vacío
    struct fs_dirent entries[DIRENTS_PER_BLK];
    memset(entries, 0, DIRENTS_PER_BLK * sizeof(struct fs_dirent));
    if (disk->ops->read(disk, inode->direct[0], 1, entries) < 0)
        exit(1);
    int res = is_empty_dir(entries);
    if (res == 0) return -ENOTEMPTY;

    // eliminar entrada del directorio padre
    memset(entries, 0, DIRENTS_PER_BLK * sizeof(struct fs_dirent));
    if (disk->ops->read(disk, parent_inode->direct[0], 1, entries) < 0)
        exit(1);
    for (int i = 0; i < DIRENTS_PER_BLK; i++) {
        if (entries[i].valid && strcmp(entries[i].name, name) == 0) {
            memset(&entries[i], 0, sizeof(struct fs_dirent));
        }
    }
    if (disk->ops->write(disk, parent_inode->direct[0], 1, entries) < 0)
        exit(1);

    // devolver bloque y borrar inodo
    return_blk(inode->direct[0]);
    return_inode(inode_idx);
    memset(inode, 0, sizeof(struct fs_inode));

    // actualizar
    update_inode(inode_idx);
    update_blk();

    return SUCCESS;
}

/**
 * rename - renombrar un archivo o directorio.
 *
 * Tenga en cuenta que esta es una versión simplificada de la funcionalidad de
 * renombrar de UNIX: consulte 'man 2 rename' para obtener la descripción completa.
 * En particular, la versión completa puede moverse entre directorios, reemplazar un
 * archivo de destino y reemplazar un directorio vacío por uno lleno.
 *
 * Errores:
 *   -ENOENT   - el archivo o directorio de origen no existe
 *   -ENOTDIR  - el componente de la ruta de origen o destino no es un directorio
 *   -EEXIST   - el destino ya existe
 *   -EINVAL   - el origen y el destino no están en el mismo directorio
 *
 * @param src_path la ruta de origen
 * @param dst_path la ruta de destino.
 * @return 0 si tiene éxito, o el valor de error
 */
static int fs_rename(const char *src_path, const char *dst_path)
{
    // copiar en profundidad ambas rutas
    char *_src_path = strdup(src_path);
    char *_dst_path = strdup(dst_path);
    // obtener inodos
    int src_inode_idx = translate(_src_path);
    int dst_inode_idx = translate(_dst_path);
    // si el inodo de origen no existe, devolver error
    if (src_inode_idx < 0) return src_inode_idx;
    // si el destino ya existe, devolver error
    if (dst_inode_idx >= 0) return -EEXIST;

    // obtener el inodo del directorio padre
    char src_name[FS_FILENAME_SIZE];
    char dst_name[FS_FILENAME_SIZE];
    int src_parent_inode_idx = translate_1(_src_path, src_name);
    int dst_parent_inode_idx = translate_1(_dst_path, dst_name);
    // el origen y el destino deben estar en el mismo directorio (mismo padre)
    if (src_parent_inode_idx != dst_parent_inode_idx) return -EINVAL;
    int parent_inode_idx = src_parent_inode_idx;
    if (parent_inode_idx < 0) return parent_inode_idx;

    // leer el inodo del directorio padre
    struct fs_inode *parent_inode = &inodes[parent_inode_idx];
    if (!S_ISDIR(parent_inode->mode)) return -ENOTDIR;

    struct fs_dirent entries[DIRENTS_PER_BLK];
    memset(entries, 0, DIRENTS_PER_BLK * sizeof(struct fs_dirent));
    if (disk->ops->read(disk, parent_inode->direct[0], 1, entries) < 0) exit(1);

    // hacer cambios en el búfer
    for (int i = 0; i < DIRENTS_PER_BLK; i++) {
        if (entries[i].valid && strcmp(entries[i].name, src_name) == 0) {
            memset(entries[i].name, 0, sizeof(entries[i].name));
            strcpy(entries[i].name, dst_name);
        }
    }

    // escribir el búfer en el inodo
    if (disk->ops->write(disk, parent_inode->direct[0], 1, entries)) exit(1);
    return SUCCESS;
}

/**
 * chmod - cambiar los permisos de un archivo
 *
 * Errores:
 *   -ENOENT   - el archivo no existe
 *   -ENOTDIR  - el componente de la ruta no es un directorio
 *
 * @param path la ruta del archivo o directorio
 * @param mode el valor de modo mode_t - consulte el man 'chmod'
 *   para obtener la descripción
 * @return 0 si tiene éxito, o el valor de error
 */
static int fs_chmod(const char *path, mode_t mode)
{
    char* _path = strdup(path);
    int inode_idx = translate(_path);
    if (inode_idx < 0) return inode_idx;
    struct fs_inode *inode = &inodes[inode_idx];
    // proteger el sistema de otros modos
    mode |= S_ISDIR(inode->mode) ? S_IFDIR : S_IFREG;
    // cambiar a través de referencia
    inode->mode = mode;
    update_inode(inode_idx);
    return SUCCESS;
}

/**
 * utime - cambiar los tiempos de acceso y modificación.
 *
 * Errores:
 *   -ENOENT   - el archivo no existe
 *   -ENOTDIR  - el componente de la ruta no es un directorio
 *
 * @param path la ruta del archivo o directorio.
 * @param ut utimbuf - consulte el man 'utime' para obtener la descripción.
 * @return 0 si tiene éxito, o el valor de error
 */
int fs_utime(const char *path, struct utimbuf *ut)
{
    char *_path = strdup(path);
    int inode_idx = translate(_path);
    if (inode_idx < 0) return inode_idx;
    struct fs_inode *inode = &inodes[inode_idx];
    // cambiar a través de referencia
    inode->mtime = ut->modtime;
    update_inode(inode_idx);
    return SUCCESS;
}

static void fs_read_blk(int blk_num, char *buf, size_t len, size_t offset) {
    char entries[BLOCK_SIZE];
    memset(entries, 0, BLOCK_SIZE * sizeof(char));
    if (disk->ops->read(disk, blk_num, 1, entries) < 0) exit(1);
    memcpy(buf, entries + offset, len);
}

static size_t fs_read_dir(size_t inode_idx, char *buf, size_t len, size_t offset) {
    struct fs_inode *inode = &inodes[inode_idx];
    size_t blk_num = offset / BLOCK_SIZE;
    size_t blk_offset = offset % BLOCK_SIZE;
    size_t len_to_read = len;
    while (blk_num < N_DIRECT && len_to_read > 0) {
        size_t cur_len_to_read = len_to_read > BLOCK_SIZE ? (size_t) BLOCK_SIZE - blk_offset : len_to_read;
        size_t temp = blk_offset + cur_len_to_read;

        if (!inode->direct[blk_num]) {
            return len - len_to_read;
        }

        fs_read_blk(inode->direct[blk_num], buf, temp, blk_offset);

        buf += temp;
        len_to_read -= temp;
        blk_num++;
        blk_offset = 0;
    }
    return len - len_to_read;
}

static size_t fs_read_indir1(size_t blk, char *buf, size_t len, size_t offset) {
    uint32_t blk_indices[PTRS_PER_BLK];
    memset(blk_indices, 0, PTRS_PER_BLK * sizeof(uint32_t));
    if (disk->ops->read(disk, (int) blk, 1, blk_indices) < 0) exit(1);

    size_t blk_num = offset / BLOCK_SIZE;
    size_t blk_offset = offset % BLOCK_SIZE;
    size_t len_to_read = len;
    while (blk_num < PTRS_PER_BLK && len_to_read > 0) {
        size_t cur_len_to_read = len_to_read > BLOCK_SIZE ? (size_t) BLOCK_SIZE - blk_offset : len_to_read;
        size_t temp = blk_offset + cur_len_to_read;

        if (!blk_indices[blk_num]) {
            return len - len_to_read;
        }

        fs_read_blk(blk_indices[blk_num], buf, temp, blk_offset);

        buf += temp;
        len_to_read -= temp;
        blk_num++;
        blk_offset = 0;
    }
    return len - len_to_read;
}

static size_t fs_read_indir2(size_t blk, char *buf, size_t len, size_t offset) {
    uint32_t blk_indices[PTRS_PER_BLK];
    memset(blk_indices, 0, PTRS_PER_BLK * sizeof(uint32_t));
    if (disk->ops->read(disk, (int) blk, 1, blk_indices) < 0) return 0;

    size_t blk_num = offset / INDIR1_SIZE;
    size_t blk_offset = offset % INDIR1_SIZE;
    size_t len_to_read = len;
    while (blk_num < PTRS_PER_BLK && len_to_read > 0) {
        size_t cur_len_to_read = len_to_read > INDIR1_SIZE ? (size_t) INDIR1_SIZE - blk_offset : len_to_read;
        size_t temp = blk_offset + cur_len_to_read;

        if (!blk_indices[blk_num]) {
            return len - len_to_read;
        }

        temp = fs_read_indir1(blk_indices[blk_num], buf, temp, blk_offset);

        buf += temp;
        len_to_read -= temp;
        blk_num++;
        blk_offset = 0;
    }
    return len - len_to_read;
}

/**
 * read - leer datos de un archivo abierto.
 *
 * Debe devolver exactamente el número de bytes solicitados, excepto:
 *   - si el desplazamiento >= tamaño del archivo, devolver 0
 *   - si el desplazamiento+len > tamaño del archivo, devolver bytes desde el desplazamiento hasta el final del archivo
 *   - en caso de error, devolver <0
 *
 * Errores:
 *   -ENOENT  - el archivo no existe
 *   -ENOTDIR - el componente de la ruta no es un directorio
 *   -EIO     - error al leer el bloque
 *
 * @param path la ruta al archivo
 * @param buf el búfer de lectura
 * @param len el número de bytes a leer
 * @param offset para comenzar a leer
 * @param fi información del archivo fuse
 */
static int fs_read(const char *path, char *buf, size_t len, off_t offset,
            struct fuse_file_info *fi)
{
    char *_path = strdup(path);
    int inode_idx = translate(_path);
    if (inode_idx < 0) return inode_idx;
    struct fs_inode *inode = &inodes[inode_idx];
    if (S_ISDIR(inode->mode)) return -EISDIR;
    if (offset >= inode->size) return 0;

    // si len va más allá del tamaño del inodo, leer hasta el final del archivo
    if (offset + len > inode->size) {
        len = (size_t) inode->size - offset;
    }

    // len a leer
    size_t len_to_read = len;

    // leer bloques directos
    if (len_to_read > 0 && offset < DIR_SIZE) {
        // len leído
        size_t temp = fs_read_dir(inode_idx, buf, len_to_read, (size_t) offset);
        len_to_read -= temp;
        offset += temp;
        buf += temp;
    }

    // leer bloques indirectos 1
    if (len_to_read > 0 && offset < DIR_SIZE + INDIR1_SIZE) {
        // len leído
        size_t temp = fs_read_indir1(inode->indir_1, buf, len_to_read, (size_t) offset - DIR_SIZE);
        len_to_read -= temp;
        offset += temp;
        buf += temp;
    }

    // leer bloques indirectos 2
    if (len_to_read > 0 && offset < DIR_SIZE + INDIR1_SIZE + INDIR2_SIZE) {
        // len leído
        size_t temp = fs_read_indir2(inode->indir_2, buf, len_to_read, (size_t) offset - DIR_SIZE - INDIR1_SIZE);
        len_to_read -= temp;
        offset += temp;
        buf += temp;
    }

    return (int) (len - len_to_read);
}

static void fs_write_blk(int blk_num, const char *buf, size_t len, size_t offset) {
    char entries[BLOCK_SIZE];
    memset(entries, 0, BLOCK_SIZE * sizeof(char));
    if (disk->ops->read(disk, blk_num, 1, entries) < 0) exit(1);
    memcpy(entries + offset, buf, len);
    if (disk->ops->write(disk, blk_num, 1, entries) < 0) exit(1);
}

static size_t fs_write_dir(size_t inode_idx, const char *buf, size_t len, size_t offset) {
    struct fs_inode *inode = &inodes[inode_idx];

    size_t blk_num = offset / BLOCK_SIZE;
    size_t blk_offset = offset % BLOCK_SIZE;
    size_t len_to_write = len;
    while (blk_num < N_DIRECT && len_to_write > 0) {
        size_t cur_len_to_write = len_to_write > BLOCK_SIZE ? (size_t) BLOCK_SIZE - blk_offset : len_to_write;
        size_t temp = blk_offset + cur_len_to_write;

        if (!inode->direct[blk_num]) {
            int freeb = get_free_blk();
            if (freeb < 0) return len - len_to_write;
            inode->direct[blk_num] = freeb;
            update_inode(inode_idx);
        }

        fs_write_blk(inode->direct[blk_num], buf, temp, blk_offset);

        buf += temp;
        len_to_write -= temp;
        blk_num++;
        blk_offset = 0;
    }
    return len - len_to_write;
}

static size_t fs_write_indir1(size_t blk, const char *buf, size_t len, size_t offset) {
    uint32_t blk_indices[PTRS_PER_BLK];
    memset(blk_indices, 0, PTRS_PER_BLK * sizeof(uint32_t));
    if (disk->ops->read(disk, (int) blk, 1, blk_indices) < 0) exit(1);

    size_t blk_num = offset / BLOCK_SIZE;
    size_t blk_offset = offset % BLOCK_SIZE;
    size_t len_to_write = len;
    while (blk_num < PTRS_PER_BLK && len_to_write > 0) {
        size_t cur_len_to_write = len_to_write > BLOCK_SIZE ? (size_t) BLOCK_SIZE - blk_offset : len_to_write;
        size_t temp = blk_offset + cur_len_to_write;

        if (!blk_indices[blk_num]) {
            int freeb = get_free_blk();
            if (freeb < 0) return len - len_to_write;
            blk_indices[blk_num] = freeb;
            //escribir de vuelta
            if (disk->ops->write(disk, blk, 1, blk_indices) < 0)
                exit(1);
        }

        fs_write_blk(blk_indices[blk_num], buf, temp, blk_offset);

        buf += temp;
        len_to_write -= temp;
        blk_num++;
        blk_offset = 0;
    }
    return len - len_to_write;
}

static size_t fs_write_indir2(size_t blk, const char *buf, size_t len, size_t offset) {
    uint32_t blk_indices[PTRS_PER_BLK];
    memset(blk_indices, 0, PTRS_PER_BLK * sizeof(uint32_t));
    if (disk->ops->read(disk, (int) blk, 1, blk_indices) < 0) return 0;

    size_t blk_num = offset / INDIR1_SIZE;
    size_t blk_offset = offset % INDIR1_SIZE;
    size_t len_to_write = len;
    while (blk_num < PTRS_PER_BLK && len_to_write > 0) {
        size_t cur_len_to_write = len_to_write > INDIR1_SIZE ? (size_t) INDIR1_SIZE - blk_offset : len_to_write;
        size_t temp = blk_offset + cur_len_to_write;
        len_to_write -= temp;
        if (!blk_indices[blk_num]) {
            int freeb = get_free_blk();
            if (freeb < 0) return len - len_to_write;
            blk_indices[blk_num] = freeb;
            //escribir de vuelta
            if (disk->ops->write(disk, blk, 1, blk_indices) < 0)
                exit(1);
        }

        temp = fs_write_indir1(blk_indices[blk_num], buf, temp, blk_offset);
        if (temp == 0) return len - len_to_write;
        buf += temp;
        blk_num++;
        blk_offset = 0;
    }
    return len - len_to_write;
}

/**
 *  write - escribir datos en un archivo
 *
 * Debe devolver exactamente la cantidad de bytes solicitados, excepto en caso de error.
 *
 * Errores:
 *   -ENOENT  - el archivo no existe
 *   -ENOTDIR - el componente de la ruta no es un directorio
 *   -EINVAL  - si 'offset' es mayor que la longitud actual del archivo.
 *              (Las semánticas POSIX admiten la creación de archivos con "agujeros", pero nosotros no)
 *
 * @param path la ruta del archivo
 * @param buf el búfer para escribir
 * @param len la cantidad de bytes a escribir
 * @param offset el desplazamiento para comenzar a escribir
 * @param fi la información del archivo Fuse para escribir
 */
static int fs_write(const char *path, const char *buf, size_t len,
             off_t offset, struct fuse_file_info *fi)
{
    char *_path = strdup(path);
    int inode_idx = translate(_path);
    if (inode_idx < 0) return inode_idx;
    struct fs_inode *inode = &inodes[inode_idx];
    if (S_ISDIR(inode->mode)) return -EISDIR;
    if (offset > inode->size) return 0;

    //len necesita escribir
    size_t len_to_write = len;

    //escribir bloques directos
    if (len_to_write > 0 && offset < DIR_SIZE) {
        //len escritos
        size_t temp = fs_write_dir(inode_idx, buf, len_to_write, (size_t) offset);
        len_to_write -= temp;
        offset += temp;
        buf += temp;
    }

    //escribir bloques indirectos 1
    if (len_to_write > 0 && offset < DIR_SIZE + INDIR1_SIZE) {
        //necesita asignar indir_1
        if (!inode->indir_1) {
            int freeb = get_free_blk();
            if (freeb < 0) return len - len_to_write;
            inode->indir_1 = freeb;
            update_inode(inode_idx);
        }
        size_t temp = fs_write_indir1(inode->indir_1, buf, len_to_write, (size_t) offset - DIR_SIZE);
        len_to_write -= temp;
        offset += temp;
        buf += temp;
    }

    //escribir bloques indirectos 2
    if (len_to_write > 0 && offset < DIR_SIZE + INDIR1_SIZE + INDIR2_SIZE) {
        //necesita asignar indir_2
        if (!inode->indir_2) {
            int freeb = get_free_blk();
            if (freeb < 0) return len - len_to_write;
            inode->indir_2 = freeb;
            update_inode(inode_idx);
        }
        //len escritos
        size_t temp = fs_write_indir2(inode->indir_2, buf, len_to_write, (size_t) offset - DIR_SIZE - INDIR1_SIZE);
        len_to_write -= temp;
        offset += len_to_write;
    }

    if (offset > inode->size) inode->size = offset;

    //actualizar inode y blk
    update_inode(inode_idx);
    update_blk();

    return (int) (len - len_to_write);
}

/**
 * Abrir un archivo o directorio del sistema de archivos.
 *
 * Errores:
 *   -ENOENT  - el archivo no existe
 *   -ENOTDIR - el componente de la ruta no es un directorio
 *
 * @param path la ruta
 * @param fi la información del archivo Fuse
 */
static int fs_open(const char *path, struct fuse_file_info *fi)
{
    char *_path = strdup(path);
    int inode_idx = translate(_path);
    if (inode_idx < 0) return inode_idx;
    if (S_ISDIR(inodes[inode_idx].mode)) return -EISDIR;
    fi->fh = (uint64_t) inode_idx;
    return SUCCESS;
}

/**
 * Liberar los recursos creados por la llamada abierta pendiente.
 *
 * Errores:
 *   -ENOENT  - el archivo no existe
 *   -ENOTDIR - el componente de la ruta no es un directorio
 *
 * @param path el nombre del archivo
 * @param fi la información del archivo Fuse
 */
static int fs_release(const char *path, struct fuse_file_info *fi)
{
    char *_path = strdup(path);
    int inode_idx = translate(_path);
    if (inode_idx < 0) return inode_idx;
    if (S_ISDIR(inodes[inode_idx].mode)) return -EISDIR;
    fi->fh = (uint64_t) -1;
    return SUCCESS;
}

/**
 * statfs - obtener estadísticas del sistema de archivos.
 * Consulte 'man 2 statfs' para obtener la descripción de 'struct statvfs'.
 *
 * Errores:
 *   ninguno - necesita funcionar
 *
 * @param path la ruta del archivo
 * @param st la estructura statvfs
 */
static int fs_statfs(const char *path, struct statvfs *st)
{
    /* necesita devolver los siguientes campos (establecer otros en cero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = imagen total - metadatos
     *   f_bfree = f_blocks - bloques utilizados
     *   f_bavail = f_bfree
     *   f_namelen = <la longitud máxima de nombre que tenga>
     *
     * esto debería funcionar bien, pero es posible que desee agregar código para
     * calcular los valores correctos más adelante.
     */

    //borrar estadísticas originales
    memset(st, 0, sizeof(*st));
    st->f_bsize = FS_BLOCK_SIZE;
    st->f_blocks = (fsblkcnt_t) (n_blocks - root_inode - inode_base);
    st->f_bfree = (fsblkcnt_t) num_free_blk();
    st->f_bavail = st->f_bfree;
    st->f_namemax = FS_FILENAME_SIZE - 1;

    return 0;
}

/**
 * Vector de operaciones. Por favor, no lo renombre, ya que el
 * código de esqueleto en misc.c asume que se llama 'fs_ops'.
 */
struct fuse_operations fs_ops = {
    .init = fs_init,
    .getattr = fs_getattr,
    .opendir = fs_opendir,
    .readdir = fs_readdir,
    .releasedir = fs_releasedir,
    .mknod = fs_mknod,
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .rename = fs_rename,
    .chmod = fs_chmod,
    .utime = fs_utime,
    .truncate = fs_truncate,
    .open = fs_open,
    .read = fs_read,
    .write = fs_write,
    .release = fs_release,
    .statfs = fs_statfs,
};