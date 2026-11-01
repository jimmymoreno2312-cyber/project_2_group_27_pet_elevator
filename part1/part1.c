/*
 * Part 1: identical to empty.c plus exactly five system calls.
 * Each libc wrapper below maps to one real syscall (no stdio, so no extra write calls).
 */
#include <unistd.h>

int main(void)
{
	getpid();	/* getpid  */
	getppid();	/* getppid */
	getuid();	/* getuid  */
	getgid();	/* getgid  */
	geteuid();	/* geteuid */
	return 0;
}
