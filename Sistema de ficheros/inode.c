#include "inodeFuse.h"

int create (struct inode *inode, struct dentry *dentry, int mode)
{
    return 0;
}

int lookup (struct inode *inode, struct dentry *dentry)
{
    return 0;
}

int link (struct dentry *old_dentry, struct inode *dir, struct dentry *dentry)
{
    return 0;
}

int unlink (struct inode *dir, struct dentry *dentry)
{
    return 0;
}

int mkdir (struct inode *dir, struct dentry *dentry, int mode)
{
    return 0;
}

int rmdir (struct inode *dir, struct dentry *dentry)
{
    return 0;
}

int mknod (struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
    return 0;
}

int rename (struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry)
{
    return 0;
}

int permission (struct inode *inode,int mask)
{
    return 0;
}

int getattr (struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
    return 0;
}

void setattr (struct dentry *dentry, struct iattr *attr)
{
    return;
}