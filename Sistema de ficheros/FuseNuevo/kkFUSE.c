#define FUSE_USE_VERSION 30

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_FILES 128

// Estructura para inodos
typedef struct {
    char name[256];
    char content[1028];
    mode_t mode;
    nlink_t nlink;
    off_t size;
    int is_dir;
} inode_t;

// Estructura para entradas de directorio (dentry)
typedef struct {
    inode_t *inode;
} dentry_t;

// Estructura para nuestro sistema de archivos
typedef struct {
    inode_t root;
    inode_t files[MAX_FILES];
    int file_count;
} superBlock;

superBlock sb;

// Función para obtener atributos de un archivo
static int do_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    
    for (int i = 0; i < sb.file_count; i++) {
        if (strcmp(path + 1, sb.files[i].name) == 0) {
            stbuf->st_mode = sb.files[i].mode;
            stbuf->st_nlink = sb.files[i].nlink;
            stbuf->st_size = sb.files[i].size;
            return 0;
        }
    }
    
    return -ENOENT;
}

// Función para leer un directorio
static int do_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;
    
    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }
    
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    
    for (int i = 0; i < sb.file_count; i++) {
        filler(buf, sb.files[i].name, NULL, 0, 0);
    }
    
    return 0;
}

// Función para abrir un archivo
static int do_open(const char *path, struct fuse_file_info *fi) {
    for (int i = 0; i < sb.file_count; i++) {
        if (strcmp(path + 1, sb.files[i].name) == 0) {
            return 0;
        }
    }
    return -ENOENT;
}

// Función para leer un archivo
static int do_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    size_t len;
    (void) fi;
    
    for (int i = 0; i < sb.file_count; i++) {
        if (strcmp(path + 1, sb.files[i].name) == 0) {
            len = sb.files[i].size;
            if (offset < len) {
                if (offset + size > len) {
                    size = len - offset;
                }
                memcpy(buf, sb.files[i].content + offset, size);
            } else {
                size = 0;
            }
            return size;
        }
    }
    return -ENOENT;
}

// Función para escribir en un archivo
static int do_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    
    for (int i = 0; i < sb.file_count; i++) {
        if (strcmp(path + 1, sb.files[i].name) == 0) {
            if (offset + size > sizeof(sb.files[i].content)) {
                size = sizeof(sb.files[i].content) - offset;
            }
            memcpy(sb.files[i].content + offset, buf, size);
            sb.files[i].size = offset + size;
            return size;
        }
    }
    return -ENOENT;
}

// Función para crear un archivo
static int do_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    if (sb.file_count >= MAX_FILES) {
        return -ENOSPC;
    }
    
    for (int i = 0; i < sb.file_count; i++) {
        if (strcmp(path + 1, sb.files[i].name) == 0) {
            return -EEXIST;
        }
    }

    inode_t *new_file = &sb.files[sb.file_count++];
    strcpy(new_file->name, path + 1);
    new_file->mode = S_IFREG | mode;
    new_file->nlink = 1;
    new_file->size = 0;
    new_file->is_dir = 0;
    
    return 0;
}

// Función para borrar un archivo
static int do_unlink(const char *path) {
    for (int i = 0; i < sb.file_count; i++) {
        if (strcmp(path + 1, sb.files[i].name) == 0) {
            for (int j = i; j < sb.file_count - 1; j++) {
                sb.files[j] = sb.files[j + 1];
            }
            sb.file_count--;
            return 0;
        }
    }
    return -ENOENT;
}
 // Función para renombrar un archivo o directorio
static int do_rename(const char *from, const char *to, unsigned int flags) {
    (void) flags;
    for (int i = 0; i < sb.file_count; i++) {
        if (strcmp(from + 1, sb.files[i].name) == 0) {
            strcpy(sb.files[i].name, to + 1);
            return 0;
        }
    }
    return -ENOENT;
}
 
// Función para crear un directorio
static int do_mkdir(const char *path, mode_t mode) {
    if (sb.file_count >= MAX_FILES) {
        return -ENOSPC;
    }
    
    for (int i = 0; i < sb.file_count; i++) {
        if (strcmp(path + 1, sb.files[i].name) == 0) {
            return -EEXIST;
        }
    }

    inode_t *new_dir = &sb.files[sb.file_count++];
    strcpy(new_dir->name, path + 1);
    new_dir->mode = S_IFDIR | mode;
    new_dir->nlink = 2;
    new_dir->size = 0;
    new_dir->is_dir = 1;
    
    return 0;
}

// Función para borrar un directorio
static int do_rmdir(const char *path) {
    for (int i = 0; i < sb.file_count; i++) {
        if (strcmp(path + 1, sb.files[i].name) == 0 && sb.files[i].is_dir) {
            for (int j = i; j < sb.file_count - 1; j++) {
                sb.files[j] = sb.files[j + 1];
            }
            sb.file_count--;
            return 0;
        }
    }
    return -ENOENT;
}

static struct fuse_operations operations = {
    .getattr    = do_getattr,
    .readdir    = do_readdir,
    .open       = do_open,
    .read       = do_read,
    .write      = do_write,
    .create     = do_create,
    .unlink     = do_unlink,
    .rename     = do_rename,
    .mkdir      = do_mkdir,
    .rmdir      = do_rmdir,
};

int main(int argc, char *argv[]) {
    sb.file_count = 0;
    sb.root = (inode_t) { "/", "", S_IFDIR | 0755, 2, 0, 1 };
    
    return fuse_main(argc, argv, &operations, NULL);
}