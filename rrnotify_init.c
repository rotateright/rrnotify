/**
 * @file rrnotify_init.c
 *
 * @remark Copyright (C) 2006-2015 RotateRight, LLC
 * @remark Copyright 2002 OProfile authors
 * @remark Based on Oprofile's implementation. 
 * @remark Read the file COPYING
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include "rrnotify.h"
#include "rrnotify_stats.h"
#include "event_buffer.h"
#include "buffer_sync.h"

unsigned long rrnotify_started;
static unsigned long is_setup;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
static DEFINE_SEMAPHORE(start_sem);
#else
static DECLARE_MUTEX(start_sem);
#endif

/* Debug
   0 - don't print debug statements.
   1 - print debug statements.
 */
int rrnotify_debug = 0;

int rrnotify_set_ulong(unsigned long *addr, unsigned long val)
{
	int err = -EBUSY;

	down(&start_sem);

	if (!rrnotify_started) {
		*addr = val;
		err = 0;
	}

	up(&start_sem);

	return err;
}

int rrnotify_setup(void)
{
	int err;
 
	down(&start_sem);

	if ((err = alloc_event_buffer())) {
		goto out1;
	}
 
	/* Note even though this starts part of the
	 * profiling overhead, it's necessary to prevent
	 * us missing task deaths and eventually oopsing
	 * when trying to process the event buffer.
	 */
	if ((err = sync_start())) {
		goto out2;
	}

	is_setup = 1;
	atomic_set(&buffer_dump, 0);
	
	up(&start_sem);
	return 0;
 
out2:
	free_event_buffer();
out1:
	up(&start_sem);
	return err;
}

/* Actually start profiling (echo 1>/dev/rrnotify/enable) */
int rrnotify_start(void)
{
	int err = -EINVAL;
 
	down(&start_sem);
 
	if (!is_setup) {
		goto out;
	}

	err = 0; 
 
	if (rrnotify_started) {
		goto out;
	}

	rrnotify_reset_stats();

	rrnotify_started = 1;
	atomic_set(&buffer_dump, 0);
	
out:
	up(&start_sem); 
	return err;
}

/* echo 0>/dev/rrnotify/enable */
void rrnotify_stop(void)
{
	down(&start_sem);
	if (!rrnotify_started) {
		goto out;
	}
	rrnotify_started = 0;

	/* wake up the daemon to read what remains */
	wake_up_buffer_waiter();
out:
	up(&start_sem);
}

void rrnotify_shutdown(void)
{
	down(&start_sem);
	sync_stop();
	is_setup = 0;
	free_event_buffer();
	up(&start_sem);
}

static int __init rrnotify_init(void)
{
	int err;
	
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	sema_init(&start_sem, 1);
#endif
	init_event_buffer();

	printk(KERN_INFO "rrnotify: init\n");
	err = rrnotifyfs_register();
	
	return err;
}

static void __exit rrnotify_exit(void)
{
	rrnotifyfs_unregister();
	printk(KERN_INFO "rrnotify: exit\n");
}

module_init(rrnotify_init);
module_exit(rrnotify_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("RotateRight, LLC");
MODULE_DESCRIPTION("rotateright task exit notifier");
