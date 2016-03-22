/**
 * @file logging.h
 *
 * @remark Copyright (C) 2008-2015 RotateRight, LLC
 */

#ifndef RRNOTIFY_LOGGING_H
#define RRNOTIFY_LOGGING_H

#include <linux/module.h>

#define MODULE_NAME "rrnotify"

extern int rrnotify_debug;

#define LOG_CALL() if(rrnotify_debug) { printk(KERN_INFO "[" MODULE_NAME "] %s()\n", __PRETTY_FUNCTION__); }

#define LOG_DEBUG(format, ...) if(rrnotify_debug) { printk(KERN_INFO "[" MODULE_NAME "] DEBUG: " format "\n", ## __VA_ARGS__); }

#define LOG_WARNING(format, ...) { printk(KERN_ALERT "[" MODULE_NAME "] WARNING: " format "\n", ## __VA_ARGS__); }

#define LOG_ERROR(format, ...) { printk(KERN_ALERT "[" MODULE_NAME "] ERROR: " format "\n", ## __VA_ARGS__); }

#endif // RRNOTIFY_LOGGING_H
