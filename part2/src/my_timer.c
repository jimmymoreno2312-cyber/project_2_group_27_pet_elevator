// SPDX-License-Identifier: GPL-2.0
/*
 * my_timer: creates /proc/timer. Each read prints the current wall-clock time
 * and, from the second read on, the time elapsed since the previous read.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>

#define PROC_NAME "timer"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Prints current time and time elapsed since the last read");

static struct timespec64 last_read;	/* time of the previous read */
static bool last_valid;			/* false until the first read happens */

static int timer_show(struct seq_file *m, void *v)
{
	struct timespec64 now, delta;

	ktime_get_real_ts64(&now);
	seq_printf(m, "current time: %lld.%09ld\n", (long long)now.tv_sec, now.tv_nsec);

	if (last_valid) {
		delta = timespec64_sub(now, last_read);
		seq_printf(m, "elapsed time: %lld.%09ld\n",
			   (long long)delta.tv_sec, delta.tv_nsec);
	}

	last_read = now;
	last_valid = true;
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
