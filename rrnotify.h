/**
 * @file rrnotify.h
 *
 * @remark Copyright (C) 2006-2015 RotateRight, LLC
 * @remark Copyright 2002 OProfile authors
 * @remark Based on Oprofile's implementation. 
 * @remark Read the file COPYING
 */
 
#ifndef RRNOTIFY_H_
#define RRNOTIFY_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/fs.h>

int rrnotify_setup(void);
void rrnotify_shutdown(void); 

int rrnotifyfs_register(void);
void rrnotifyfs_unregister(void);

int rrnotify_start(void);
void rrnotify_stop(void);

int rrnotify_set_ulong(unsigned long *addr, unsigned long val);

extern unsigned long fs_buffer_size;
extern unsigned long fs_buffer_watershed;
extern unsigned long rrnotify_started;

extern int rrnotify_debug; // RR

/** lock for read/write safety */
extern spinlock_t rrnotifyfs_lock;

struct super_block;
struct dentry;

void rrnotify_create_files(struct super_block * sb, struct dentry * root);

/**
 * Create a file of the given name as a child of the given root, with
 * the specified file operations.
 */
int rrnotifyfs_create_file(struct super_block * sb, struct dentry * root,
	char const * name, const struct file_operations * fops);

int rrnotifyfs_create_file_perm(struct super_block * sb, struct dentry * root,
	char const * name, const struct file_operations * fops, int perm);

/** Create a file for read/write access to an unsigned long. */
int rrnotifyfs_create_ulong(struct super_block * sb, struct dentry * root,
	char const * name, unsigned long * val);

/** Create a file for read-only access to an atomic_t. */
int rrnotifyfs_create_ro_atomic(struct super_block * sb, struct dentry * root,
	char const * name, atomic_t * val);

/** create a directory */
struct dentry * rrnotifyfs_mkdir(struct super_block * sb, struct dentry * root,
	char const * name);

#endif /*RRNOTIFY_H_*/
