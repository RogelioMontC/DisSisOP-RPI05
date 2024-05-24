#include "inodeFuse.h"

int  d_revalidate (struct dentry *dentry, struct nameidata *nd)
{
    return 0;
}
int  d_hash (const struct dentry *dentry, const struct qstr *name)
{
    return 0;
}
int  d_compare (const struct dentry *dentry, const struct dentry *dentry2, 
                unsigned int len, const char *str, const struct qstr *name)
{
    return 0;
}
int  d_delete (struct dentry *dentry)
{
    return 0;
}
int  d_release (struct dentry *dentry)
{
    return 0;
}
int  d_iput (struct dentry *dentry, struct inode *inode)
{
    return 0;
}
int  d_dname (const struct dentry *dentry, char *buffer, int buflen)
{
    return 0;
}