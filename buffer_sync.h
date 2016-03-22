/**
 * @file buffer_sync.c
 *
 * @remark Copyright (C) 2006-2015 RotateRight, LLC
 * @remark Copyright 2002 OProfile authors
 * @remark Based on Oprofile's implementation. 
 * @remark Read the file COPYING
 */

#ifndef RRNOTIFY_BUFFER_SYNC_H_
#define RRNOTIFY_BUFFER_SYNC_H_

struct task_struct;

/* add the necessary profiling hooks */
int sync_start(void);

/* remove the hooks */
void sync_stop(void);

/* sync the tgid buffer */
void sync_buffer(struct task_struct * task);

#endif /*RRNOTIFY_BUFFER_SYNC_H_*/
