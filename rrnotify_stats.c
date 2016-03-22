/**
 * @file rrnotify_stats.c
 *
 * @remark Copyright (C) 2006-2015 RotateRight, LLC
 * @remark Copyright 2002 OProfile authors
 * @remark Based on Oprofile's implementation.
 * @remark Read the file COPYING
 */


#include "rrnotify.h"

#include <linux/version.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/threads.h>
 
#include "rrnotify_stats.h"
 
struct rrnotify_stat_struct rrnotify_stats;
 
void rrnotify_reset_stats(void)
{
	atomic_set(&rrnotify_stats.sample_lost_no_mm, 0);
	atomic_set(&rrnotify_stats.event_lost_overflow, 0);
	atomic_set(&rrnotify_stats.event_received, 0);
}


void rrnotify_create_stats_files(struct super_block * sb, struct dentry * root)
{
	struct dentry * dir;

	dir = rrnotifyfs_mkdir(sb, root, "stats");
	if (!dir)
		return;
 
	rrnotifyfs_create_ro_atomic(sb, dir, "sample_lost_no_mm",
		&rrnotify_stats.sample_lost_no_mm);
	rrnotifyfs_create_ro_atomic(sb, dir, "event_lost_overflow",
		&rrnotify_stats.event_lost_overflow);
	rrnotifyfs_create_ro_atomic(sb, dir, "event_received",
		&rrnotify_stats.event_received);

}
