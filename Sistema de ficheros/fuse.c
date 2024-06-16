#include "fuse.h"

#include <stdlib.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    // Implement your logic here
    
    return 0;

}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    // Implement your logic here
    
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
    // Implement your logic here
    
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    // Implement your logic here
    
    return 0;
}

static struct fuse_operations fs_oper = {
    .getattr    = fs_getattr,
    .readdir    = fs_readdir,
    .open       = fs_open,
    .read       = fs_read,
};

int main(int argc, char *argv[])
{
    struct super_block    *sb  = (struct super_block *)malloc(sizeof(struct super_block));

    return fuse_main(argc, argv, &fs_oper, NULL);
}