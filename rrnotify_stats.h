/**
 * @file rrnotify_stats.h
 *
 * @remark Copyright (C) 2006-2015 RotateRight, LLC
 * @remark Copyright 2002 OProfile authors
 * @remark Based on Oprofile's implementation.
 * @remark Read the file COPYING
 */

#ifndef RRNOTIFY_STATS_H
#define RRNOTIFY_STATS_H

#include <asm/atomic.h>
 
// XXX names must match oprofile/rrprofile stats
struct rrnotify_stat_struct {
	atomic_t sample_lost_no_mm;
	atomic_t event_lost_overflow;
	atomic_t event_received;
};

extern struct rrnotify_stat_struct rrnotify_stats;
 
/* reset all stats to zero */
void rrnotify_reset_stats(void);
 
struct super_block;
struct dentry;
 
/* create the stats/ dir */
void rrnotify_create_stats_files(struct super_block * sb, struct dentry * root);

#endif /* RRNOTIFY_STATS_H */
