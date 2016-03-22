/**
 * @file rrnotifyfs.c
 *
 * @remark Copyright (C) 2006-2015 RotateRight, LLC
 * @remark Copyright 2002 OProfile authors
 * @remark Based on Oprofile's implementation. 
 * @remark Read the file COPYING
 */
 
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/version.h>
#include <asm/uaccess.h>

#include "rrnotify.h"
#include "rrnotify_stats.h"
#include "logging.h"
#include "event_buffer.h"

#define RRNOTIFYFS_MAGIC 0x6022006f

DEFINE_SPINLOCK(rrnotifyfs_lock);

/* fs_buffer_size and fs_buffer_watershed are defined in units of (unsigned long). */
unsigned long fs_buffer_size = (1 * 1024 * 1024) / sizeof(unsigned long); // 1MB
unsigned long fs_buffer_watershed = (256 * 1024) / sizeof(unsigned long); // 256kB (fs_buffer_size/4)

static struct inode * rrnotifyfs_get_inode(struct super_block * sb, int mode)
{
	struct inode *inode = new_inode(sb);

	if (inode) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
		inode->i_ino = get_next_ino();
#endif
		inode->i_mode = mode;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
		inode->i_uid = 0;
		inode->i_gid = 0;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
		inode->i_blksize = PAGE_CACHE_SIZE;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
		inode->i_blocks = 0;
#endif
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	}
	return inode;
}


static const struct super_operations s_ops = {
	.statfs		= simple_statfs,
	.drop_inode 	= generic_delete_inode,
};


ssize_t oprofilefs_str_to_user(char const *str, char __user *buf, size_t count, loff_t *offset)
{
	return simple_read_from_buffer(buf, count, offset, str, strlen(str));
}


#define TMPBUFSIZE 50

ssize_t rrnotifyfs_ulong_to_user(unsigned long val, char __user * buf, size_t count, loff_t * offset)
{
	char tmpbuf[TMPBUFSIZE];
	size_t maxlen = snprintf(tmpbuf, TMPBUFSIZE, "%lu\n", val);
	if (maxlen > TMPBUFSIZE)
		maxlen = TMPBUFSIZE;
	return simple_read_from_buffer(buf, count, offset, tmpbuf, maxlen);
}


int rrnotifyfs_ulong_from_user(unsigned long * val, char const __user * buf, size_t count)
{
	char tmpbuf[TMPBUFSIZE];
#ifndef RRNOTIFY
	unsigned long flags;
#endif // RRNOTIFY

	if (!count)
		return 0;

	if (count > TMPBUFSIZE - 1)
		return -EINVAL;

	memset(tmpbuf, 0x0, TMPBUFSIZE);

	if (copy_from_user(tmpbuf, buf, count))
		return -EFAULT;

#ifdef RRNOTIFY
	spin_lock(&rrnotifyfs_lock);
#else
	spin_lock_irqsave(&oprofilefs_lock, flags);
#endif // RRNOTIFY
	*val = simple_strtoul(tmpbuf, NULL, 0);
#ifdef RRNOTIFY
	spin_unlock(&rrnotifyfs_lock);
#else
	spin_unlock_irqrestore(&oprofilefs_lock, flags);
#endif // RRNOTIFY
	return 0;
}


static ssize_t ulong_read_file(struct file * file, char __user * buf, size_t count, loff_t * offset)
{
	unsigned long * val = file->private_data;
	return rrnotifyfs_ulong_to_user(*val, buf, count, offset);
}


static ssize_t ulong_write_file(struct file * file, char const __user * buf, size_t count, loff_t * offset)
{
	unsigned long value;
	int retval;

	if (*offset)
		return -EINVAL;

	retval = rrnotifyfs_ulong_from_user(&value, buf, count);
	if (retval)
		return retval;

	retval = rrnotify_set_ulong(file->private_data, value);
	if (retval)
		return retval;

	return count;
}


static int default_open(struct inode * inode, struct file * filp)
{
#ifdef HAS_IPRIVATE
	if (inode->i_private)
		filp->private_data = inode->i_private;
#else
	if (inode->u.generic_ip)
		filp->private_data = inode->u.generic_ip;
#endif
	return 0;
}


static const struct file_operations ulong_fops = {
	.read		= ulong_read_file,
	.write		= ulong_write_file,
	.open		= default_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	.llseek		= default_llseek,
#endif
};


static const struct file_operations ulong_ro_fops = {
	.read		= ulong_read_file,
	.open		= default_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	.llseek		= default_llseek,
#endif
};


static int __rrnotifyfs_create_file(struct super_block * sb,
	struct dentry * root, char const * name, const struct file_operations * fops,
	int perm, void *priv)
{
	struct dentry * dentry;
	struct inode * inode;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
	dentry = d_alloc_name(root, name);
#else
	struct qstr qname;
	qname.name = name;
	qname.len = strlen(name);
	qname.hash = full_name_hash(qname.name, qname.len);
	dentry = d_alloc(root, &qname);
#endif
	if (!dentry)
		return -ENOMEM;
	inode = rrnotifyfs_get_inode(sb, S_IFREG | perm);
	if (!inode) {
		dput(dentry);
		return -ENOMEM;
	}
	inode->i_fop = fops;
	d_add(dentry, inode);
#ifdef HAS_IPRIVATE
	dentry->d_inode->i_private = priv;
#else
	dentry->d_inode->u.generic_ip = priv;
#endif
	return 0;
}


int rrnotifyfs_create_ulong(struct super_block * sb, struct dentry * root,
	char const * name, unsigned long * val)
{
	return __rrnotifyfs_create_file(sb, root, name,
					&ulong_fops, 0644, val);
}


int rrnotifyfs_create_ro_ulong(struct super_block *sb, struct dentry *root,
	char const *name, unsigned long *val)
{
	return __rrnotifyfs_create_file(sb, root, name,
					&ulong_ro_fops, 0444, val);
}



static ssize_t atomic_read_file(struct file * file, char __user * buf, size_t count, loff_t * offset)
{
	atomic_t * val = file->private_data;
	return rrnotifyfs_ulong_to_user(atomic_read(val), buf, count, offset);
}


static const struct file_operations atomic_ro_fops = {
	.read		= atomic_read_file,
	.open		= default_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	.llseek		= default_llseek,
#endif // >= 2.6.37
};


int rrnotifyfs_create_ro_atomic(struct super_block * sb, struct dentry * root,
	char const * name, atomic_t * val)
{
	return __rrnotifyfs_create_file(sb, root, name,
					&atomic_ro_fops, 0444, val);
}


int rrnotifyfs_create_file(struct super_block * sb, struct dentry * root,
	char const * name, const struct file_operations * fops)
{
	return __rrnotifyfs_create_file(sb, root, name, fops, 0644, NULL);
}


int rrnotifyfs_create_file_perm(struct super_block * sb, struct dentry * root,
	char const * name, const struct file_operations * fops, int perm)
{
	return __rrnotifyfs_create_file(sb, root, name, fops, perm, NULL);
}


struct dentry * rrnotifyfs_mkdir(struct super_block * sb,
	struct dentry * root, char const * name)
{
	struct dentry * dentry;
	struct inode * inode;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
	dentry = d_alloc_name(root, name);
#else
	struct qstr qname;
	qname.name = name;
	qname.len = strlen(name);
	qname.hash = full_name_hash(qname.name, qname.len);
	dentry = d_alloc(root, &qname);
#endif
	if (!dentry)
		return NULL;
	inode = rrnotifyfs_get_inode(sb, S_IFDIR | 0755);
	if (!inode) {
		dput(dentry);
		return NULL;
	}
	inode->i_op = &simple_dir_inode_operations;
	inode->i_fop = &simple_dir_operations;
	d_add(dentry, inode);
	return dentry;
}


static ssize_t pointer_size_read(struct file * file, char __user * buf, size_t count, loff_t * offset)
{
	return rrnotifyfs_ulong_to_user(sizeof(void *), buf, count, offset);
}


static struct file_operations pointer_size_fops = {
	.read		= pointer_size_read,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	.llseek		= default_llseek,
#endif // >= 2.6.37
};

static ssize_t enable_read(struct file * file, char __user * buf, size_t count, loff_t * offset)
{
	return rrnotifyfs_ulong_to_user(rrnotify_started, buf, count, offset);
}


static ssize_t enable_write(struct file * file, char const __user * buf, size_t count, loff_t * offset)
{
	unsigned long val;
	int retval;

	if (*offset) {
		return -EINVAL;
	}

	retval = rrnotifyfs_ulong_from_user(&val, buf, count);
	if (retval) {
		return retval;
	}
 
	if (val) {
		retval = rrnotify_start();
		if (retval) {
			return retval;
		}
	} else {
		rrnotify_stop();
	}

	return count;
}

 
static struct file_operations enable_fops = {
	.read		= enable_read,
	.write		= enable_write,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	.llseek		= default_llseek,
#endif // >= 2.6.37
};


static ssize_t debug_read(struct file * file, char __user * buf, size_t count, loff_t * offset)
{
	return rrnotifyfs_ulong_to_user(rrnotify_debug, buf, count, offset);
}


static ssize_t debug_write(struct file * file, char const __user * buf, size_t count, loff_t * offset)
{
	int retval;
	unsigned long val;

	if(*offset) {
		return -EINVAL;
	}

	retval = rrnotifyfs_ulong_from_user(&val, buf, count);
	if(retval) {
		return retval;
	}

	if(rrnotify_debug && !val) {
		LOG_DEBUG("debug output disabled");
	}

	rrnotify_debug = val != 0 ? 1 : 0;
	if(rrnotify_debug) {
		LOG_DEBUG("debug output enabled")
	}

	return count;
}


static struct file_operations debug_fops = {
	.read           = debug_read,
	.write          = debug_write,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	.llseek		= default_llseek,
#endif // >= 2.6.37
};

static int rrnotifyfs_fill_super(struct super_block * sb, void * data, int silent)
{
	struct inode * root_inode;
	struct dentry * root_dentry;

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = RRNOTIFYFS_MAGIC;
	sb->s_op = &s_ops;
	sb->s_time_gran = 1;

	root_inode = rrnotifyfs_get_inode(sb, S_IFDIR | 0755);
	if (!root_inode)
		return -ENOMEM;
	root_inode->i_op = &simple_dir_inode_operations;
	root_inode->i_fop = &simple_dir_operations;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	root_dentry = d_make_root(root_inode);
#else
	root_dentry = d_alloc_root(root_inode);
#endif // >= 3.4.0
	if (!root_dentry) {
		iput(root_inode);
		return -ENOMEM;
	}

	sb->s_root = root_dentry;

	rrnotifyfs_create_file_perm(sb, root_dentry, "debug", &debug_fops, 0666);
	rrnotifyfs_create_file_perm(sb, root_dentry, "enable", &enable_fops, 0666);
	rrnotifyfs_create_file_perm(sb, root_dentry, "buffer", &event_buffer_fops, 0666);
	rrnotifyfs_create_ulong(sb, root_dentry, "buffer_size", &fs_buffer_size);
	rrnotifyfs_create_ulong(sb, root_dentry, "buffer_watershed", &fs_buffer_watershed);
	rrnotifyfs_create_file(sb, root_dentry, "pointer_size", &pointer_size_fops);

	rrnotify_create_stats_files(sb, root_dentry);

	// FIXME: verify kill_litter_super removes our dentries
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
static struct dentry *rrnotifyfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_single(fs_type, flags, data, rrnotifyfs_fill_super);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
static int rrnotifyfs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_single(fs_type, flags, data, rrnotifyfs_fill_super, mnt);
}
#else
static struct super_block *rrnotifyfs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return get_sb_single(fs_type, flags, data, rrnotifyfs_fill_super);
}
#endif

static struct file_system_type rrnotifyfs_type = {
	.owner		= THIS_MODULE,
#ifdef RRNOTIFY
	.name		= "rrnotifyfs",
#else
	.name		= "oprofilefs",
#endif // RRNOTIFY
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	.mount		= rrnotifyfs_mount,
#else
	.get_sb		= rrnotifyfs_get_sb,
#endif // >= 2.6.37
	.kill_sb	= kill_litter_super,
};


int __init rrnotifyfs_register(void)
{
	return register_filesystem(&rrnotifyfs_type);
}


void __exit rrnotifyfs_unregister(void)
{
	unregister_filesystem(&rrnotifyfs_type);
}
