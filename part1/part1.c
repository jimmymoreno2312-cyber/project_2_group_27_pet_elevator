/*
 * Part 1: identical to empty.c plus exactly five system calls.
 * Each libc wrapper below maps to one real syscall (no stdio, so no extra write calls).
 */
#include <unistd.h>

int main(void)
{
	getpid();
	getppid();
	getuid();
	getgid();
	geteuid();
	return 0;
}
