// SPDX-License-Identifier: GPL-2.0
/*
 * Kernel side of the pet elevator system calls.
 *
 * This file is copied into the kernel tree as [kernel_version]/syscalls/syscalls.c.
 * The system calls do no work themselves: they forward to the function pointers
 * (STUBs) that the elevator module installs when it is loaded, and fail with
 * -ENOSYS while no module is loaded.
 */
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/syscalls.h>

int (*STUB_start_elevator)(void) = NULL;
int (*STUB_issue_request)(int, int, int) = NULL;
int (*STUB_stop_elevator)(void) = NULL;
EXPORT_SYMBOL(STUB_start_elevator);
EXPORT_SYMBOL(STUB_issue_request);
EXPORT_SYMBOL(STUB_stop_elevator);

/* System call 548 */
SYSCALL_DEFINE0(start_elevator)
{
	if (STUB_start_elevator)
		return STUB_start_elevator();
	return -ENOSYS;
}

/* System call 549 */
SYSCALL_DEFINE3(issue_request, int, start_floor, int, destination_floor, int, type)
{
	if (STUB_issue_request)
		return STUB_issue_request(start_floor, destination_floor, type);
	return -ENOSYS;
}

/* System call 550 */
SYSCALL_DEFINE0(stop_elevator)
{
	if (STUB_stop_elevator)
		return STUB_stop_elevator();
	return -ENOSYS;
}
