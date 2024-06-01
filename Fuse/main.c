#define FUSE_USE_VERSION 27

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <fuse.h>

#include "image.h"
#include "fuseStructures.h"

struct stat sb;

/*********** NO MODIFIQUE ESTE ARCHIVO *************/
/**
 * Todas las funciones de tarea se acceden a través de la estructura de operaciones. */
extern struct fuse_operations fs_ops;

/** dispositivo de bloque de disco */
struct blkdev *disk;

struct data {
    char *image_name;
    int   part;
    int   cmd_mode;
} _data;

/**
 * Constante: longitud máxima de la ruta
 */
enum { MAX_PATH = 4096 };

/*
 * Vea los comentarios en /usr/include/fuse/fuse_opts.h para obtener detalles de
 * procesamiento de argumentos FUSE.
 *
 *  uso: ./homework -image disk.img directorio
 *              disk.img  - nombre del archivo de imagen para montar
 *              directorio - directorio en el que montarlo
 */
static struct fuse_opt opts[] = {
        {"-image %s", offsetof(struct data, image_name), 0},
        {"-cmdline", offsetof(struct data, cmd_mode), 1},
        FUSE_OPT_END
};

/* Funciones de utilidad
 */

/*
 * strmode - traduce un modo numérico en una cadena
 *
 * @param buf buffer de salida
 * @param mode el modo numérico
 * @return puntero al búfer de salida
 */
static char *strmode(char *buf, int mode)
{
    int mask = 0400;
    char *str = "rwxrwxrwx", *value = buf;
    *buf++ = S_ISDIR(mode) ? 'd' : '-';
    for (mask = 0400; mask != 0; str++, mask = mask >> 1)
        *buf++ = (mask & mode) ? *str : '-';
    *buf++ = 0;
    return value;
}

/**
 * Divide una cadena en una matriz de hasta n tokens.
 *
 * Si toks es NULL, p no se altera y la función devuelve
 * la cantidad de tokens. De lo contrario, p se altera por strtok()
 * y la función devuelve los tokens en la matriz toks, que
 * apuntan a elementos de la cadena p.
 *
 * @param p la cadena de caracteres
 * @param toks matriz de tokens
 * @param n número máximo de tokens para recuperar, 0 = ilimitado
 * @param delim la cadena delimitadora entre tokens
 */
static int split(char *p, char *toks[], int n, char *delim)
{
    if (n == 0) {
        n = INT_MAX;
    }
    if (toks == NULL) {
        // no alterar p si no se devuelven nombres
        p = strdup(p);
    }
    char *str;
    char *lasts = NULL;
    int i;
    for (i = 0; i < n && (str = strtok_r(p, delim, &lasts)) != NULL; i++) {
        p = NULL;
        if (toks != NULL) {
            toks[i] = str;
        }
    }
    if (toks == NULL) {
        free(p);
    }
    return i;
}

/** directorio de trabajo actual */
static char cwd[MAX_PATH];

/**
 * Actualiza cwd global a partir de los elementos de la matriz de rutas globales.
 *
 * @param array de elementos de ruta
 * @param depth número de elementos
 */
static void update_cwd(char *paths[], int npaths)
{
    char *p = cwd;
    if (npaths == 0) {
        strcpy(cwd,"/");
    } else {
        for (int i = 0; i < npaths; i++) {
            p += sprintf(p, "/%s", paths[i]);
        }
    }
}

/**
 * Obtén el directorio de trabajo actual.
 */
static const char *get_cwd(void)
{
    return cwd;
}

/**
 * Normaliza la matriz de rutas para eliminar nombres vacíos,
 * y los nombres de directorio actual (.) y padre (..).
 *
 * @param names matriz de elementos de nombre
 * @param nnames el número de elementos
 * @param el nuevo valor de nnames después de la normalización
 */
static int normalize_paths(char *names[], int nnames) {
    int dest = 0;
    for (int src = 0; src < nnames; src++) {
        if (strcmp(names[src],".") == 0) {
            // ignorar el elemento de ruta del directorio actual
        } else if (strcmp(names[src], "..") == 0) {
            if (dest > 0) dest--;  // retroceder un elemento de directorio
        } else {  // agregar elemento de directorio
            if (dest < src) {
                strcpy(names[dest], names[src]);
            }
            dest++;
        }
    }
    return dest;
}

/**
 * Copia el nombre de ruta completa de la ruta al búfer de ruta. Si la ruta
 * comienza con '/', copia la ruta al búfer de ruta. De lo contrario,
 * agrega cwd a la ruta separado por '/'.
 *
 * @param path una ruta absoluta o relativa
 * @param pathbuf búfer de ruta de resultado debe tener una longitud de MAX_PATH
 * @return puntero al búfer de ruta
 */
static char *full_path(const char *path, char *pathbuf) {
    // dividir en elementos de ruta y normalizar la ruta
    char tmp_path[MAX_PATH];
    if (*path == '/') {
        strcpy(tmp_path, path);
    } else {
        sprintf(tmp_path, "%s/%s", get_cwd(), path);
    }

    // dividir la ruta en nombres de ruta
    int npaths = split(tmp_path, NULL, 0, "/");  // obtener el recuento de rutas
    char **names = malloc(npaths*sizeof(char*));
    split(tmp_path, names, npaths, "/");	// dividir en nombres de ruta

    npaths = normalize_paths(names, npaths);
    if (npaths == 0) {
        strcpy(pathbuf, "/");
    } else {
        // agregar elementos de ruta en pathbuf
        char *bufp = pathbuf;
        for (int i = 0; i < npaths; i++) {
            bufp += sprintf(bufp, "/%s", names[i]);
        }
    }
    free(names);

    return pathbuf;
}

/**
 * Cambiar directorio.
 *
 * @param argv arg[0] es el nuevo directorio
 */
static int do_cd1(char *argv[])
{
    struct stat sb;
    char pathbuf[MAX_PATH];
    int value = fs_ops.getattr(full_path(argv[0], pathbuf), &sb);
    if (value == 0) {
        if (S_ISDIR(sb.st_mode)) {
            int nnames = split(pathbuf, NULL, 0, "/");  // obtener el recuento
            char **names = malloc(nnames * sizeof(char*));

            split(pathbuf, names, nnames, "/");
            update_cwd(names, nnames);

            free(names);
            return 0;
        } else {
            return -ENOTDIR;
        }
    } else {
        return -ENOENT;
    }
}

/**
 * Cambiar al directorio raíz.
 */
static int do_cd0(char *argv[])
{
    char *args[] = {"/"};
    return do_cd1(args);
}

/**
 * Imprimir directorio de trabajo actual
 */
static int do_pwd(char *argv[])
{
    printf("%s\n", get_cwd());
    return 0;
}

static char lsbuf[DIRENTS_PER_BLK][MAX_PATH]; /** búfer para listar entradas de directorio */
static int  lsi;  /* índice actual de ls */

static void init_ls(void)
{
    lsi = 0;
}

static int filler(void *buf, const char *name, const struct stat *sb, off_t off)
{
    sprintf(lsbuf[lsi++], "%s\n", name);
    return 0;
}

/**
 * Ordenar e imprimir listados de directorios.
 */
static void print_ls(void)
{
    int i;
    qsort(lsbuf, lsi, MAX_PATH, (void*)strcmp);
    for (i = 0; i < lsi; i++) {
        printf("%s", lsbuf[i]);
    }
}

/**
 * Listar directorio relativo al directorio de trabajo actual.
 *
 * @param argv argv[0] es un nombre de directorio
 */
int do_ls1(char *argv[])
{
    char pathbuf[MAX_PATH];
    full_path(argv[0], pathbuf);
    struct fuse_file_info info;
    memset(&info, 0, sizeof(struct fuse_file_info));
    int value;
    if ((value = fs_ops.opendir(pathbuf, &info)) == 0) {
        init_ls();
        value = fs_ops.readdir(pathbuf, NULL, filler, 0, &info);
        print_ls();
        fs_ops.releasedir(pathbuf, &info);
    }
    return value;
}

/**
 * Listar directorio de trabajo actual
 */
static int do_ls0(char *argv[])
{
    char *cwd = strdup(get_cwd());
    int status = do_ls1(&cwd);
    free(cwd);
    return status;
}

/**
 * Callback agrega la lista de entradas de directorio al búfer ls.
 * La forma de la función se especifica por Fuse readdir API.
 */
static int dashl_filler(void *buf, const char *name, const struct stat *sb, off_t off)
{
    char mode[16], time[26], *lasts;
    sprintf(lsbuf[lsi++], "%5lld %s %2d %4d %4d %8lld %s %s\n",
            sb->st_blocks, strmode(mode, sb->st_mode),
            sb->st_nlink, sb->st_uid, sb->st_gid, sb->st_size,
            strtok_r(ctime_r(&sb->st_mtime,time),"\n",&lasts), name);
    return 0;
}


/**
 * Lista larga del directorio especificado.
 *
 * @path ruta del directorio a listar
 */
static int _lsdashl(const char *path)
{
    struct stat sb;
    char pathbuf[MAX_PATH];
    full_path(path, pathbuf);
    int value = fs_ops.getattr(pathbuf, &sb);
    if (value == 0) {
        init_ls();
        struct fuse_file_info info;
        memset(&info, 0, sizeof(struct fuse_file_info));
        if (S_ISDIR(sb.st_mode)) {
            if ((value = fs_ops.opendir(pathbuf, &info)) == 0) {
                /* leer información del directorio */
                value = fs_ops.readdir(pathbuf, NULL, dashl_filler, 0, &info);
            }
            fs_ops.releasedir(pathbuf, &info);
        } else {
            /* leer información del archivo */
            value = dashl_filler(NULL, pathbuf, &sb, 0);
        }
        /* imprimir información del archivo o directorio */
        print_ls();
    }
    return value;
}

/**
 * Lista larga del directorio especificado
 * relativo al directorio actual.
 *
 * @param argv argv[0] es el nombre del directorio
 */
static int do_lsdashl1(char *argv[])
{
    char pathbuf[MAX_PATH];
    full_path(argv[0], pathbuf);
    return _lsdashl(pathbuf);
}

/**
 * Lista larga del directorio de trabajo actual.
 *
 * @param argv sin usar
 */
static int do_lsdashl0(char *argv[])
{
    char *cwd = strdup(get_cwd());
    int status = do_lsdashl1(&cwd);
    free(cwd);
    return status;
}

/**
 * Modificar el modo de un archivo o directorio especificado
 * por argv[0] utilizando la cadena de modo especificada por argv[1].
 * El nombre del archivo o directorio se interpretará en relación
 * al directorio de trabajo actual.
 *
 * @param argv argv[0] es el modo, argv[1] es el nombre de un archivo o
 * directorio
 */
static int do_chmod(char *argv[])
{
    char path[MAX_PATH];
    int mode = strtol(argv[0], NULL, 8);
    full_path(argv[1], path);
    return fs_ops.chmod(path, mode);
}

/**
 * Renombrar directorio.
 *
 * @param argv argv[0] es el nombre antiguo, argv[1] es el nuevo nombre
 *   relativo al directorio de trabajo
 */
static int do_rename(char *argv[])
{
    char p1[MAX_PATH], p2[MAX_PATH];
    full_path(argv[0], p1);
    full_path(argv[1], p2);
    return fs_ops.rename(p1, p2);
}

/**
 * Crear directorio.
 *
 * @param argv argv[0] es el nombre del directorio
 */
static int do_mkdir(char *argv[])
{
    char path[MAX_PATH];
    full_path(argv[0], path);
    return fs_ops.mkdir(path, 0777);
}

/**
 * Eliminar un directorio.
 *
 * @param argv argv[0] es el nombre del directorio
 */
static int do_rmdir(char *argv[])
{
    char path[MAX_PATH];
    full_path(argv[0], path);
    return fs_ops.rmdir(path);
}

/**
 * Eliminar un archivo.
 *
 * @param argv argv[0] es el nombre del archivo
 */
static int do_rm(char *argv[])
{
    char path[MAX_PATH];
    full_path(argv[0], path);
    return fs_ops.unlink(path);
}

static int blksiz;		/* tamaño del búfer de bloque */
static char *blkbuf;	/* búfer de bloque para copiar archivos */

/**
 * Copiar un archivo desde el directorio local al sistema de archivos
 *
 * @param argv arg[0] es el archivo local, argv[1] es
 *   el nombre del archivo en el sistema de archivos
 */
static int do_put(char *argv[])
{
    char *outside = argv[0], *inside = argv[1];
    char path[MAX_PATH];
    int len, fd, offset = 0, val;

    if ((fd = open(outside, O_RDONLY, 0)) < 0) {
        return fd;
    }
    full_path(inside, path);
    if ((val = fs_ops.mknod(path, 0777 | S_IFREG, 0)) != 0) {
        return val;
    }

    struct fuse_file_info info;
    memset(&info, 0, sizeof(struct fuse_file_info));
    if ((val = fs_ops.open(path, &info)) != 0) {
        return val;
    }
    while ((len = read(fd, blkbuf, blksiz)) > 0) {
        val = fs_ops.write(path, blkbuf, len, offset, &info);
        if (val != len) {
            break;
        }
        offset += len;
    }
    close(fd);
    fs_ops.release(path, &info);
    return (val >= 0) ? 0 : val;
}

/**
 * Copiar un archivo desde el directorio local al sistema de archivos con
 * el mismo nombre.
 *
 * @param argv arg[0] es el nombre del archivo; nombre del sistema de archivos relativo al directorio actual
 */
static int do_put1(char *argv[])
{
    char *args2[] = {argv[0], argv[0]};
    return do_put(args2);
}

/**
 * Copiar un archivo desde el sistema de archivos al directorio local
 *
 * @param argv arg[0] es el nombre del archivo en el sistema de archivos,
 *              argv[1] es el archivo local
 */
static int do_get(char *argv[])
{
    char *inside = argv[0], *outside = argv[1];
    char path[MAX_PATH];
    int len, fd, offset = 0;

    if ((fd = open(outside, O_WRONLY|O_CREAT|O_TRUNC, 0777)) < 0) {
        return fd;
    }
    full_path(inside, path);
    struct fuse_file_info info;
    memset(&info, 0, sizeof(struct fuse_file_info));
    int val;
    if ((val = fs_ops.open(path, &info)) != 0) {
        return val;
    }
    while (1) {
        len = fs_ops.read(path, blkbuf, blksiz, offset, &info);
        if (len >= 0) {
            len = write(fd, blkbuf, len);
            if (len <= 0) {
                break;
            }
            offset += len;
        }
    }
    close(fd);
    fs_ops.release(path, &info);
    return (len >= 0) ? 0 : len;
}

/**
 * Copiar un archivo desde el sistema de archivos al directorio local con
 * el mismo nombre.
 *
 * @param argv arg[0] es el nombre del archivo; nombre del sistema de archivos
 *   relativo al directorio actual
 */
static int do_get1(char *argv[])
{
    char *args2[] = {argv[0], argv[0]};
    return do_get(args2);
}

/**
 * Recuperar e imprimir un archivo.
 *
 * @param argv argv[0] es el nombre del archivo
 */
static int do_show(char *argv[])
{
    char path[MAX_PATH];
    int len, offset = 0;

    full_path(argv[0], path);
    struct fuse_file_info info;
    memset(&info, 0, sizeof(struct fuse_file_info));
    int val;
    if ((val = fs_ops.open(path, &info)) != 0) {
        return val;
    }
    while ((len = fs_ops.read(path, blkbuf, blksiz, offset, &info)) > 0) {
        fwrite(blkbuf, len, 1, stdout);
        offset += len;
    }
    fs_ops.release(path, &info);
    return (len >= 0) ? 0 : len;
}

/**
 * Imprimir estadísticas del sistema de archivos
 *
 * @argv no utilizado
 */
static int do_statfs(char *argv[])
{
    struct statvfs st;
    int value = fs_ops.statfs("/", &st);
    if (value == 0) {
        printf("tamaño de bloque: %lu\n", st.f_bsize);
        printf("número de bloques: %u\n", st.f_blocks);
        printf("bloques disponibles: %u\n", st.f_bavail);
        printf("longitud máxima de nombre: %lu\n", st.f_namemax);
    }
    return value;
}

/**
 * Imprimir estadísticas de archivos
 *
 * @param argv argv[0] es el nombre del archivo relativo al directorio actual
 */
static int do_stat(char *argv[])
{
    struct stat sb;
    int value = fs_ops.getattr(argv[0], &sb);
    if (value == 0) {
        char mode[16], time[26], *lasts;
        printf("%5lld %s %2d %4d %4d %8lld %s %s\n",
               sb.st_blocks, strmode(mode, sb.st_mode),
               sb.st_nlink, sb.st_uid, sb.st_gid, sb.st_size,
               strtok_r(ctime_r(&sb.st_mtime,time),"\n",&lasts), argv[0]);
    }
    return value;
}

/**
 * Establecer el tamaño de bloque de lectura/escritura
 *
 * @param size tamaño de bloque de lectura/escritura
 */
static void _blksiz(int size)
{
    blksiz = size;	// guardar el nuevo tamaño de bloque
    if (blkbuf) {	// liberar el antiguo búfer de bloque
        free(blkbuf);
    }
    blkbuf = malloc(blksiz);	// crear un nuevo búfer de bloque
    printf("tamaño de bloque de lectura/escritura: %d\n", blksiz);
}

/**
 * Establecer el tamaño de bloque de lectura/escritura
 *
 * @param argv argv[0] es el tamaño de bloque como cadena
 */
static int do_blksiz(char *argv[])
{
    _blksiz(atoi(argv[0]));
    return 0;
}

/**
 * Truncar un archivo.
 *
 * @param argv argv[0] es el nombre del archivo relativo al directorio actual
 */
static int do_truncate(char *argv[])
{
    char path[MAX_PATH];
    full_path(argv[0], path);
    return fs_ops.truncate(path, 0);
}

/**
 * Establecer el tiempo de acceso y modificación.
 *
 * @param argv argv[0] es el nombre del archivo relativo al directorio actual
 */
static int do_utime(char *argv[])
{
    struct utimbuf ut;
    char path[MAX_PATH];
    full_path(argv[0], path);
    ut.actime = ut.modtime = time(NULL);	// establecer el tiempo de acceso a ahora
    return fs_ops.utime(path, &ut);
}

/**
 * Crea un archivo regular o establece el tiempo de acceso y modificación.
 *
 * @param argv argv[0] es el nombre del archivo relativo al directorio actual
 */
static int do_touch(char *argv[])
{
    char path[MAX_PATH];
    full_path(argv[0], path);
    // intentar crear un nuevo archivo
    int status = fs_ops.mknod(path, 0777 | S_IFREG, 0);
    if (status == -EEXIST) {
        // si ya existe, modificar su tiempo de acceso/modificación a ahora
        struct utimbuf ut;
        ut.actime = ut.modtime = time(NULL);
        status =fs_ops.utime(path, &ut);
    }
    return status;
}

/** struct sirve como tabla de despacho para los comandos */
static struct {
    char *name;
    int   nargs;
    int   (*f)(char *args[]);
    char  *help;
} cmds[] = {
        {"cd",      0, do_cd0,          "cd - cambiar al directorio raíz"},
        {"cd",      1, do_cd1,          "cd <dir> - cambiar al directorio"},
        {"pwd",     0, do_pwd,          "pwd - mostrar el directorio actual"},
        {"ls",      0, do_ls0,          "ls - listar archivos en el directorio actual"},
        {"ls",      1, do_ls1,          "ls <dir> - listar directorio especificado"},
        {"ls-l",    0, do_lsdashl0,     "ls-l - mostrar lista detallada de archivos"},
        {"ls-l",    1, do_lsdashl1,     "ls-l <file> - mostrar información detallada del archivo"},
        {"chmod",   2, do_chmod,        "chmod <mode> <file> - cambiar permisos"},
        {"rename",  2, do_rename,       "rename <oldname> <newname> - renombrar archivo"},
        {"mkdir",   1, do_mkdir,        "mkdir <dir> - crear directorio"},
        {"rmdir",   1, do_rmdir,        "rmdir <dir> - eliminar directorio"},
        {"rm",      1, do_rm,           "rm <file> - eliminar archivo"},
        {"put",     2, do_put,          "put <outside> <inside> - copiar un archivo desde el directorio local al sistema de archivos"},
        {"put",     1, do_put1,         "put <name> - igual, pero mantener el mismo nombre"},
        {"get",     2, do_get,          "get <inside> <outside> - recuperar un archivo del sistema de archivos al directorio local"},
        {"get",     1, do_get1,         "get <name> - igual, pero mantener el mismo nombre"},
        {"show",    1, do_show,         "show <file> - recuperar e imprimir un archivo"},
        {"statfs",  0, do_statfs,       "statfs - mostrar información del sistema de archivos"},
        {"blksiz",  1, do_blksiz,       "blksiz - establecer el tamaño de bloque de lectura/escritura"},
        {"truncate",1, do_truncate,     "truncate <file> - truncar a longitud cero"},
        {"utime",   1, do_utime,        "utime <file> - establecer la hora de modificación a la hora actual"},
        {"touch",   1, do_touch,        "touch <file> - crear archivo o establecer la hora de modificación a la hora actual"},
        {"stat",    1, do_stat,         "stat <file> - mostrar información del archivo"},
        {0, 0, 0}
};

/**
 * Bucle de comandos para el intérprete de comandos interactivo.
 */
static int cmdloop(void)
{
    char line[MAX_PATH];

    update_cwd(NULL, 0);

    while (true) {
        printf("cmd> "); fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        if (!isatty(0)) {
            printf("%s", line);
        }

        if (line[0] == '#')	{/* líneas de comentario */
            continue;
        }

        // dividir la entrada en comando y argumentos
        char *args[10];
        int i, nargs = split(line, args, 10, " \t\r\n");

        // continuar si la línea está vacía
        if (nargs == 0) {
            continue;
        }

        // salir si el comando es "quit" o "exit"
        if ((strcmp(args[0], "quit") == 0) || (strcmp(args[0], "exit") == 0)) {
            break;
        }

        // proporcionar ayuda si el comando es "help" o "?"
        if ((strcmp(args[0], "help") == 0) || (strcmp(args[0], "?") == 0)) {
            for (i = 0; cmds[i].name != NULL; i++) {
                printf("%s\n", cmds[i].help);
            }
            continue;
        }

        // validar el comando y los argumentos
        for (i = 0; cmds[i].name != NULL; i++) {
            if ((strcmp(args[0], cmds[i].name) == 0) && (nargs == cmds[i].nargs+1)) {
                break;
            }
        }

        // si el comando no es reconocido o el número de argumentos es incorrecto
        if (cmds[i].name == NULL) {
            if (nargs > 0) {
                printf("comando incorrecto: %s\n", args[0]);
            }
            continue;
        }

        // procesar el comando
        int err = cmds[i].f(&args[1]);
        if (err != 0) {
            printf("error: %s\n", strerror(-err));
        }
    }
    return 0;
}


/**************/

int main(int argc, char **argv)
{
    /* Procesamiento y verificación de argumentos
     */
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    if (fuse_opt_parse(&args, &_data, opts, NULL) == -1){
        fprintf(stderr, "opción incorrecta\n");
        exit(1);
    }

    if (_data.image_name == 0){
        fprintf(stderr, "Debe proporcionar una imagen\n");
        
        exit(1);
    }

    char *file = _data.image_name;
    if (strcmp(file+strlen(file)-4, ".img") != 0) {
        fprintf(stderr, "archivo de imagen incorrecto (debe terminar en .img): %s\n", file);
        
        exit(1);
    }

    if ((disk = image_create(file)) == NULL) {
        fprintf(stderr, "no se puede abrir el archivo de imagen '%s': %s\n", file, strerror(errno));
        exit(1);
    }

    if (_data.cmd_mode) {  /* procesar comandos interactivos */
        fs_ops.init(NULL);
        _blksiz(FS_BLOCK_SIZE);
        cmdloop();
        return 0;
    }

    /** pasar el control a fuse */
    return fuse_main(args.argc, args.argv, &fs_ops, NULL);
}