#include "inodeFuse.h"

off_t llseek (struct file *file, off_t offset, int whence)
 {
    return 0;
 }
int open (struct inode *inode, struct file *file)
{
    return 0;
}
int release (struct inode *inode, struct file *file)
{
    return 0;
}
int read (struct file *file, char *buf, size_t count, off_t *offset)
{
    return 0;
}
int write (struct file *file, const char *buf, size_t count, off_t *offset)
{
    return 0;
}
int flush (struct file *file)
{
    return 0;
}
int fsync (struct file *file, int datasync)
{
    return 0;
}
int readdir (struct file *file, void *dirent, filldir_t filldir)
{
    return 0;
}