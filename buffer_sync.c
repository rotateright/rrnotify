/**
 * @file buffer_sync.c
 *
 * @remark Copyright (C) 2006-2015 RotateRight, LLC
 * @remark Copyright 2002 OProfile authors
 * @remark Based on Oprofile's implementation. 
 * @remark Read the file COPYING
 */

#include <linux/mm.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/dcookies.h>
#include <linux/profile.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/version.h>
 
#include "rrnotify.h" 
#include "rrnotify_stats.h"
#include "event_buffer.h"
#include "buffer_sync.h"

/* The task is on its way out. A sync of the buffer means we can catch
 * any remaining samples for this task.
 */
static int task_exit_notify(struct notifier_block * self, unsigned long val, void * data)
{
	struct task_struct * task = data;
	
	sync_buffer(task);
  	
  	return 0;
}

static struct notifier_block task_exit_nb = {
	.notifier_call	= task_exit_notify,
};

int sync_start(void)
{
	int err;

	err = profile_event_register(PROFILE_TASK_EXIT, &task_exit_nb);
	return err;
}

void sync_stop(void)
{
	profile_event_unregister(PROFILE_TASK_EXIT, &task_exit_nb);
}

/* Optimisation. We can manage without taking the dcookie sem
 * because we cannot reach this code without at least one
 * dcookie user still being registered (namely, the reader
 * of the event buffer). */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
static inline unsigned long fast_get_dcookie(struct path * path)
{
	unsigned long cookie;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
	if (path->dentry->d_flags & DCACHE_COOKIE)
#else
	if (path->dentry->d_cookie)
#endif
		return (unsigned long)path->dentry;
	get_dcookie(path, &cookie);
	return cookie;
}
#else
static inline unsigned long fast_get_dcookie(struct dentry * dentry,
	struct vfsmount * vfsmnt)
{
	unsigned long cookie;
 
	if (dentry->d_cookie)
		return (unsigned long)dentry;
	get_dcookie(dentry, vfsmnt, &cookie);
	return cookie;
}
#endif


static void release_mm(struct mm_struct * mm)
{
	if (!mm)
		return;
	up_read(&mm->mmap_sem);
	mmput(mm);
}

static struct mm_struct * take_tasks_mm(struct task_struct * task)
{
	struct mm_struct * mm = get_task_mm(task);
	if (mm)
		down_read(&mm->mmap_sem);
	return mm;
}

static void add_escape_code(int code)
{
	add_event_entry(RR_ESCAPE_CODE);
	add_event_entry(code);
}

static void add_task_thread_info(struct task_struct * task)
{
	unsigned long utime, stime;
	struct timespec end_time;

	add_escape_code(RRNOTIFY_THREAD_INFO_BEGIN);

	// Write the task group id and the thread id
	add_event_entry(task->tgid);
	add_event_entry(task->pid);

	// Write out the user time and the system time
	utime = jiffies_to_usecs(task->utime);
	stime = jiffies_to_usecs(task->stime);
	add_event_entry(utime);
	add_event_entry(stime); 
	
	// Write out the start time 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
	add_event_entry(task->real_start_time/1000000000);
	add_event_entry(task->real_start_time);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
	add_event_entry(task->real_start_time.tv_sec);
	add_event_entry(task->real_start_time.tv_nsec);
#else
	add_event_entry(task->start_time.tv_sec);
	add_event_entry(task->start_time.tv_nsec);
#endif
	
	// Write out the end time
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
	do_posix_clock_monotonic_gettime(&end_time);
#else
	end_time = current_kernel_time();
#endif
	add_event_entry(end_time.tv_sec);
	add_event_entry(end_time.tv_nsec);

	add_escape_code(RRNOTIFY_THREAD_INFO_END);
}

static void add_task_module_info(struct task_struct * task)
{
	struct mm_struct *mm = take_tasks_mm(task);

	// module info is variable-length - calculate total length in entries first
	unsigned long moduleCount = 0;
	if(mm) {
		struct vm_area_struct * vma;
		for (vma = mm->mmap; vma; vma = vma->vm_next) {
			if (vma->vm_file && (vma->vm_flags & VM_EXEC)) {
				moduleCount++;
			} else {
				continue;
			}
		}
	} else {
		atomic_inc(&rrnotify_stats.sample_lost_no_mm);
	}

	add_escape_code(RRNOTIFY_MODULE_LIST_BEGIN);
	add_event_entry(moduleCount); // number of module entries

	if(mm) {
		unsigned long cookie = RR_NO_COOKIE;
		off_t offset = 0;
		struct vm_area_struct * vma;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
		// fixup removal of VM_EXECUTABLE flag in linux 3.7.0 and later
		// https://lkml.org/lkml/2012/3/31/42
#define VM_EXECUTABLE   0x00001000
		unsigned long vm_flags = 0;
		unsigned long app_cookie = RR_NO_COOKIE;
		if (mm->exe_file) {
			app_cookie = fast_get_dcookie(&mm->exe_file->f_path);
		}
#endif // >= 3.7.0

		for (vma = mm->mmap; vma; vma = vma->vm_next) {
			if (vma->vm_file && (vma->vm_flags & VM_EXEC)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
				cookie = fast_get_dcookie(&vma->vm_file->f_path);
#else
				cookie = fast_get_dcookie(vma->vm_file->f_dentry,
								vma->vm_file->f_vfsmnt);
#endif
				offset = vma->vm_pgoff << PAGE_SHIFT;

				add_event_entry(vma->vm_start);
				add_event_entry(vma->vm_end);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
				vm_flags = vma->vm_flags;
				if(cookie == app_cookie) {
					vm_flags |= VM_EXECUTABLE;
				}
				add_event_entry(vm_flags);
#else
				add_event_entry(vma->vm_flags);
#endif // >= 3.7.0
				add_event_entry(cookie);
				add_event_entry(offset);
			} else {
				continue;
			}
		}
	}

	release_mm(mm);

	add_escape_code(RRNOTIFY_MODULE_LIST_END);
}

void sync_buffer(struct task_struct * task)
{
	down(&buffer_sem);
	atomic_inc(&rrnotify_stats.event_received);
	add_escape_code(RRNOTIFY_RECORD_BEGIN);
	add_task_thread_info(task);
	add_task_module_info(task);
	add_escape_code(RRNOTIFY_RECORD_END);
	up(&buffer_sem);
}
