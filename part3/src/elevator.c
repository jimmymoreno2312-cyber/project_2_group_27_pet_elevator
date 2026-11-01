// SPDX-License-Identifier: GPL-2.0
/*
 * elevator: a kernel module that runs a pet elevator.
 *
 * Pets are requested through the issue_request() system call and wait in a
 * per-floor FIFO list. A kthread moves the elevator, unloading and loading pets
 * with a LOOK strategy (keep going in one direction while anything is ahead,
 * otherwise reverse). All state is protected by one mutex and is reported in
 * /proc/elevator.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/wait.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Pet elevator scheduler");

#define PROC_NAME	"elevator"
#define MIN_FLOOR	1
#define MAX_FLOOR	5
#define MAX_PETS	5
#define MAX_WEIGHT	50	/* lbs */
#define MOVE_SECS	2	/* time to travel one floor */
#define LOAD_SECS	1	/* time to load/unload pets */

enum pet_type { CHIHUAHUA, PUG, PUGHUAHUA, DACHSHUND, NUM_PET_TYPES };
enum elevator_state { OFFLINE, IDLE, LOADING, UP, DOWN };

static const int pet_weight[NUM_PET_TYPES] = { 3, 14, 10, 16 };
static const char pet_letter[NUM_PET_TYPES] = { 'C', 'P', 'H', 'D' };
static const char *const state_name[] = {
	[OFFLINE] = "OFFLINE", [IDLE] = "IDLE", [LOADING] = "LOADING",
	[UP] = "UP", [DOWN] = "DOWN",
};

struct pet {
	int type;
	int start_floor;
	int dest_floor;
	struct list_head node;
};

/* Everything below is protected by elev_lock (except where READ_ONCE is used). */
static DEFINE_MUTEX(elev_lock);
static DECLARE_WAIT_QUEUE_HEAD(elev_wq);

static struct list_head waiting[MAX_FLOOR + 1];	/* indexed by floor */
static LIST_HEAD(passengers);

static enum elevator_state state = OFFLINE;
static int cur_floor = MIN_FLOOR;
static int cur_load;		/* lbs */
static int num_passengers;
static int num_waiting;
static int num_serviced;
static bool stopping;		/* stop requested, finishing current passengers */
static bool wake_pending;	/* new work for the thread, avoids lost wake-ups */
static struct task_struct *elevator_task;

/* ---- helpers (caller must hold elev_lock) ---- */

static void notify_thread(void)
{
	WRITE_ONCE(wake_pending, true);
	wake_up_interruptible(&elev_wq);
}

/* Drop every passenger whose destination is the current floor. */
static int unload_pets(void)
{
	struct pet *p, *tmp;
	int count = 0;

	list_for_each_entry_safe(p, tmp, &passengers, node) {
		if (p->dest_floor != cur_floor)
			continue;
		list_del(&p->node);
		cur_load -= pet_weight[p->type];
		num_passengers--;
		num_serviced++;
		count++;
		kfree(p);
	}
	return count;
}

/* Board waiting pets of the current floor in FIFO order while they fit. */
static int load_pets(void)
{
	struct list_head *queue = &waiting[cur_floor];
	struct pet *p;
	int count = 0;

	while (!list_empty(queue)) {
		p = list_first_entry(queue, struct pet, node);
		if (num_passengers >= MAX_PETS ||
		    cur_load + pet_weight[p->type] > MAX_WEIGHT)
			break;
		list_move_tail(&p->node, &passengers);
		cur_load += pet_weight[p->type];
		num_passengers++;
		num_waiting--;
		count++;
	}
	return count;
}

/* Is there something to do in direction dir (+1 up, -1 down)? */
static bool has_target(int dir, bool include_waiting)
{
	struct pet *p;
	int floor;

	list_for_each_entry(p, &passengers, node) {
		if ((dir > 0 && p->dest_floor > cur_floor) ||
		    (dir < 0 && p->dest_floor < cur_floor))
			return true;
	}
	if (!include_waiting)
		return false;
	for (floor = cur_floor + dir; floor >= MIN_FLOOR && floor <= MAX_FLOOR; floor += dir) {
		if (!list_empty(&waiting[floor]))
			return true;
	}
	return false;
}

/* LOOK: keep the current direction while possible, else reverse, else 0. */
static int pick_direction(int last_dir)
{
	bool include_waiting = !stopping;

	if (has_target(last_dir, include_waiting))
		return last_dir;
	if (has_target(-last_dir, include_waiting))
		return -last_dir;
	return 0;
}

/* ---- elevator thread ---- */

/* Sleep up to secs seconds; returns early if the thread is asked to stop. */
static void elevator_sleep(int secs)
{
	wait_event_interruptible_timeout(elev_wq, kthread_should_stop(), secs * HZ);
}

static int elevator_run(void *data)
{
	int last_dir = 1;
	int dir;

	while (!kthread_should_stop()) {
		mutex_lock(&elev_lock);
		WRITE_ONCE(wake_pending, false);

		if (state == OFFLINE) {
			mutex_unlock(&elev_lock);
			wait_event_interruptible(elev_wq,
				kthread_should_stop() || READ_ONCE(wake_pending));
			continue;
		}

		/* Unload (and load, unless stopping) at the current floor. */
		if (unload_pets() + (stopping ? 0 : load_pets()) > 0) {
			state = LOADING;
			mutex_unlock(&elev_lock);
			elevator_sleep(LOAD_SECS);
			continue;
		}

		dir = pick_direction(last_dir);
		if (dir == 0) {
			/* Nothing left to do: finish stopping or wait for pets. */
			state = stopping ? OFFLINE : IDLE;
			stopping = false;
			mutex_unlock(&elev_lock);
			wait_event_interruptible(elev_wq,
				kthread_should_stop() || READ_ONCE(wake_pending));
			continue;
		}

		last_dir = dir;
		state = dir > 0 ? UP : DOWN;
		mutex_unlock(&elev_lock);

		elevator_sleep(MOVE_SECS);
		if (kthread_should_stop())
			break;

		mutex_lock(&elev_lock);
		cur_floor += dir;
		mutex_unlock(&elev_lock);
	}
	return 0;
}

/* ---- system call implementations ---- */

static int my_start_elevator(void)
{
	struct task_struct *task;
	int ret = 0;

	mutex_lock(&elev_lock);
	if (state != OFFLINE) {
		ret = 1;	/* already active (or still stopping) */
		goto out;
	}

	state = IDLE;
	cur_floor = MIN_FLOOR;
	stopping = false;

	if (!elevator_task) {
		task = kthread_run(elevator_run, NULL, "elevator");
		if (IS_ERR(task)) {
			state = OFFLINE;
			ret = PTR_ERR(task);
			goto out;
		}
		elevator_task = task;
	}
	notify_thread();
out:
	mutex_unlock(&elev_lock);
	return ret;
}

static int my_issue_request(int start_floor, int destination_floor, int type)
{
	struct pet *p;

	if (start_floor < MIN_FLOOR || start_floor > MAX_FLOOR ||
	    destination_floor < MIN_FLOOR || destination_floor > MAX_FLOOR ||
	    start_floor == destination_floor ||
	    type < 0 || type >= NUM_PET_TYPES)
		return 1;

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	p->type = type;
	p->start_floor = start_floor;
	p->dest_floor = destination_floor;

	mutex_lock(&elev_lock);
	list_add_tail(&p->node, &waiting[start_floor]);
	num_waiting++;
	notify_thread();
	mutex_unlock(&elev_lock);
	return 0;
}

static int my_stop_elevator(void)
{
	int ret = 0;

	mutex_lock(&elev_lock);
	if (stopping) {
		ret = 1;	/* already deactivating */
	} else if (state != OFFLINE) {
		stopping = true;
		notify_thread();
	}
	mutex_unlock(&elev_lock);
	return ret;
}

/* Provided by the modified kernel (syscalls.c). */
extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int, int, int);
extern int (*STUB_stop_elevator)(void);

/* ---- /proc/elevator ---- */

static int elevator_show(struct seq_file *m, void *v)
{
	struct pet *p;
	int floor, count;

	mutex_lock(&elev_lock);
	seq_printf(m, "Elevator state: %s\n", state_name[state]);
	seq_printf(m, "Current floor: %d\n", cur_floor);
	seq_printf(m, "Current load: %d lbs\n", cur_load);

	seq_puts(m, "Elevator status:");
	list_for_each_entry(p, &passengers, node)
		seq_printf(m, " %c%d", pet_letter[p->type], p->dest_floor);
	seq_putc(m, '\n');

	for (floor = MAX_FLOOR; floor >= MIN_FLOOR; floor--) {
		count = 0;
		list_for_each_entry(p, &waiting[floor], node)
			count++;
		seq_printf(m, "[%c] Floor %d: %d", floor == cur_floor ? '*' : ' ', floor, count);
		list_for_each_entry(p, &waiting[floor], node)
			seq_printf(m, " %c%d", pet_letter[p->type], p->dest_floor);
		seq_putc(m, '\n');
	}

	seq_printf(m, "Number of pets: %d\n", num_passengers);
	seq_printf(m, "Number of pets waiting: %d\n", num_waiting);
	seq_printf(m, "Number of pets serviced: %d\n", num_serviced);
	mutex_unlock(&elev_lock);
	return 0;
}

/* ---- module init / exit ---- */

static void free_all_pets(void)
{
	struct pet *p, *tmp;
	int floor;

	list_for_each_entry_safe(p, tmp, &passengers, node) {
		list_del(&p->node);
		kfree(p);
	}
	for (floor = MIN_FLOOR; floor <= MAX_FLOOR; floor++) {
		list_for_each_entry_safe(p, tmp, &waiting[floor], node) {
			list_del(&p->node);
			kfree(p);
		}
	}
}

static int __init elevator_init(void)
{
	int floor;

	for (floor = MIN_FLOOR; floor <= MAX_FLOOR; floor++)
		INIT_LIST_HEAD(&waiting[floor]);

	if (!proc_create_single(PROC_NAME, 0444, NULL, elevator_show))
		return -ENOMEM;

	STUB_start_elevator = my_start_elevator;
	STUB_issue_request = my_issue_request;
	STUB_stop_elevator = my_stop_elevator;
	return 0;
}

static void __exit elevator_exit(void)
{
	struct task_struct *task;

	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;
	remove_proc_entry(PROC_NAME, NULL);

	mutex_lock(&elev_lock);
	task = elevator_task;
	elevator_task = NULL;
	mutex_unlock(&elev_lock);

	/* Must not hold elev_lock here: the thread may need it to reach its stop check. */
	if (task)
		kthread_stop(task);

	mutex_lock(&elev_lock);
	free_all_pets();
	mutex_unlock(&elev_lock);
}

module_init(elevator_init);
module_exit(elevator_exit);
