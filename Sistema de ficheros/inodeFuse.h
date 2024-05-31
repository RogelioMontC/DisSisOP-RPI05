/**
 * @file inodeFuse.h
 * @brief This file contains the structures and function declarations related to the inode and file system operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/types.h>
#include <sys/stat.h>
#include <time.h>
#include <linux/fs.h>


#define NAME_SIZE 40    /* 192 bytes */

/**
 * @struct inode
 * @brief Represents an inode in the file system.
 */
struct inode {
    long id;                                /**< Inode ID */
    mode_t mode;                            /**< File mode */
    nlink_t nlink;                          /**< Link count */
    uid_t uid;                              /**< User ID */
    gid_t gid;                              /**< Group ID */
    off_t size;                            /**< File size */
    struct timespec i_atime;                /**< Time of last access */
    struct timespec i_mtime;                /**< Time of last modification */
    struct timespec i_ctime;                /**< Time of last status change */
    const struct inode_operations *i_op;    /**< Inode operations */
    const struct file_operations *i_fop;    /**< File operations */
    const struct address_space_operations *i_mapping;   /**< Address space operations */
    const struct address_space *i_data;                 /**< Address space */
    uint32_t blocks[15];                     /**< Pointers to data blocks */
};

/**
 * @struct dentry
 * @brief Represents a directory entry in the file system.
 */
struct dentry {
    struct super_block *d_sb;               /**< Superblock object */
    struct dentry *d_parent;                /**< Parent dentry */
    struct list_head *d_subdirs;            /**< Pointer to list of subdirectories */
    struct dentry_operations *d_op;         /**< Dentry operations */
    unsigned char d_iname[NAME_SIZE];       /**< Directory name */
    struct inode *d_inode;                  /**< Inode object */
};

/**
 * @struct file
 * @brief Represents a file in the file system.
 */
struct file {
    struct path *f_path;                    /**< File path */
    struct dentry (f_path->dentry);          /**< Dentry object */
    unsigned int f_flags;                   /**< File flags */
    mode_t f_mode;                          /**< File mode */
    struct file_operations *f_op;           /**< File operations */
    off_t f_pos;                           /**< File position */
};

/**
 * @struct super_block
 * @brief Represents a superblock in the file system.
 */
struct super_block {
    struct list_head s_list;                /**< Pointers for superblock list */
    struct file_system_type *s_type;        /**< File system type */
    const struct super_operations *s_op;    /**< Superblock methods */
    struct dentry *s_root;                  /**< Dentry object of the FileSystem's root directory */
    struct block_device *s_bdev;            /**< Pointer to the Block device driver descriptor */
    struct list_head s_inodes;              /**< List of inodes */
    struct list_head s_dirty;               /**< List of modified objects */
    struct list_head s_io;                  /**< List of inodes waiting to be written in disk */
    struct list_head s_files;               /**< List of file objects */
};

/**
 * @struct inode_operations
 * @brief Represents the operations that can be performed on an inode.
 */
struct inode_operations {
    int (*create) (struct inode *inode, struct dentry *dentry, int mode);
    int (*lookup) (struct inode *inode, struct dentry *dentry);
    int (*link) (struct dentry *old_dentry, struct inode *dir, struct dentry *dentry);
    int (*unlink) (struct inode *dir, struct dentry *dentry);
    int (*mkdir) (struct inode *dir, struct dentry *dentry, int mode);
    int (*rmdir) (struct inode *dir, struct dentry *dentry);
    int (*mknod) (struct inode *dir, struct dentry *dentry, int mode, dev_t dev);
    int (*rename) (struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry);
    int (*permission) (struct inode *inode,int mask);
    void(*setattr) (struct dentry *dentry, struct iattr *attr);
    int (*getattr) (struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat);
};

/**
 * @struct dentry_operations
 * @brief Represents the operations that can be performed on a directory entry.
 */
struct dentry_operations {
    int (*d_revalidate) (struct dentry *dentry, struct nameidata *nd);
    int (*d_hash) (const struct dentry *dentry, const struct qstr *name);
    int (*d_compare) (const struct dentry *dentry, const struct dentry *dentry2, unsigned int len, const char *str, const struct qstr *name);
    int (*d_delete) (struct dentry *dentry);
    int (*d_release) (struct dentry *dentry);
    int (*d_iput) (struct dentry *dentry, struct inode *inode);
    int (*d_dname) (const struct dentry *dentry, char *buffer, int buflen);
};

/**
 * @struct file_operations
 * @brief Represents the operations that can be performed on a file.
 */
struct file_operations {
    struct module *owner;
    off_t (*llseek) (struct file *file, off_t offset, int whence);
    int (*open) (struct inode *inode, struct file *file);
    int (*release) (struct inode *inode, struct file *file);
    int (*read) (struct file *file, char *buf, size_t count, off_t *offset);
    int (*write) (struct file *file, const char *buf, size_t count, off_t *offset);
    int (*flush) (struct file *file);
    int (*fsync) (struct file *file, int datasync);
    int (*readdir) (struct file *file, void *dirent, filldir_t filldir);
};

struct super_operations {
    int (*alloc_inode) (struct super_block *sb);
    void (*write_inode) (struct inode *inode, int sync);
    void (*read_inode) (struct inode *inode);    
    void (*dirty_inode) (struct inode *inode);
    void (*destroy_inode) (struct inode *inode);
    void (*put_inode) (struct inode *inode);
    void (*drop_inode) (struct inode *inode);
    void (*delete_inode) (struct inode *inode);
    void (*put_super) (struct super_block *sb);
    void (*write_super) (struct super_block *sb);
    int (*sync_fs) (struct super_block *sb, int wait);
    int (*statfs) (struct dentry *dentry, struct kstatfs *buf);
    int (*remount_fs) (struct super_block *sb, int *flags, char *data);
    void (*clear_inode) (struct inode *inode);
    void (*umount_begin) (struct super_block *sb);
};

void error_parametros();