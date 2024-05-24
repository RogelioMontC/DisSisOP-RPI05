#include "inodeFuse.h"

int  alloc_inode (struct super_block *sb)
{
    return 0;
}
void  write_inode (struct inode *inode, int sync)
{
    return;
}
void  read_inode (struct inode *inode)
{
    return;
}
void  dirty_inode (struct inode *inode)
{
    return;
}
void  destroy_inode (struct inode *inode)    
{
    return;
}
void  put_inode (struct inode *inode)
{
    return;
}
void  drop_inode (struct inode *inode)
{
    return;
}
void  delete_inode (struct inode *inode)
{
    return;
}
void  put_super (struct super_block *sb)
{
    return;
}
void  write_super (struct super_block *sb)
{
    return;
}
int  sync_fs (struct super_block *sb, int wait)
{
    return 0;
}
int  statfs (struct dentry *dentry, struct kstatfs *buf)
{
    return 0;
}
int  remount_fs (struct super_block *sb, int *flags, char *data)
{
    return 0;
}
void  clear_inode (struct inode *inode)
{
    return;
}
void  umount_begin (struct super_block *sb)
{
    return;
}