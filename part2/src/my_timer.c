// SPDX-License-Identifier: GPL-2.0
/*
 * my_timer: creates /proc/timer. Each read prints the current wall-clock time
 * and, from the second read on, the time elapsed since the previous read.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>

#define PROC_NAME "timer"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group 27");
MODULE_DESCRIPTION("Prints current time and time elapsed since the last read");

/* Protects last_read and last_valid against concurrent reads of /proc/timer. */
static DEFINE_MUTEX(timer_lock);
static struct timespec64 last_read;	/* time of the previous read */
static bool last_valid;			/* false until the first read happens */

static int timer_show(struct seq_file *m, void *v)
{
	struct timespec64 now, delta;

	mutex_lock(&timer_lock);
	ktime_get_real_ts64(&now);
	seq_printf(m, "current time: %lld.%09ld\n", (long long)now.tv_sec, now.tv_nsec);

	if (last_valid) {
		delta = timespec64_sub(now, last_read);
		seq_printf(m, "elapsed time: %lld.%09ld\n",
			   (long long)delta.tv_sec, delta.tv_nsec);
	}

	last_read = now;
	last_valid = true;
	mutex_unlock(&timer_lock);
	return 0;
}

static int __init my_timer_init(void)
{
	last_valid = false;
	if (!proc_create_single(PROC_NAME, 0444, NULL, timer_show))
		return -ENOMEM;
	return 0;
}

static void __exit my_timer_exit(void)
{
	remove_proc_entry(PROC_NAME, NULL);
}

module_init(my_timer_init);
module_exit(my_timer_exit);
