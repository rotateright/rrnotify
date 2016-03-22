/**
 * @file event_buffer.h
 *
 * @remark Copyright (C) 2006-2015 RotateRight, LLC
 * @remark Copyright 2002 OProfile authors
 * @remark Based on Oprofile's implementation. 
 * @remark Read the file COPYING
 */

#ifndef EVENT_BUFFER_H
#define EVENT_BUFFER_H
#include <linux/version.h>
#include <linux/types.h> 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif

void init_event_buffer(void);

int alloc_event_buffer(void);

void free_event_buffer(void);

/* wake up the process sleeping on the event file */
void wake_up_buffer_waiter(void);

/* Each escaped entry is prefixed by ESCAPE_CODE
 * then one of the following codes, then the
 * relevant data.
 */
#define RR_ESCAPE_CODE			~0UL

typedef enum {
	RRNOTIFY_RECORD_BEGIN		=1,
	RRNOTIFY_THREAD_INFO_BEGIN	=2,
	RRNOTIFY_THREAD_INFO_END	=3,
	RRNOTIFY_MODULE_LIST_BEGIN	=4,
	RRNOTIFY_MODULE_LIST_END	=5,
	RRNOTIFY_RECORD_END			=6
} RRNotifyLinuxCode;

#define RR_INVALID_COOKIE	~0UL
#define RR_NO_COOKIE		0UL

/* add data to the event buffer */
void add_event_entry(unsigned long data);

extern struct file_operations event_buffer_fops;

/* mutex between sync_cpu_buffers() and the
 * file reading code.
 */
extern struct semaphore buffer_sem;
extern atomic_t buffer_dump;

#endif /* EVENT_BUFFER_H */
