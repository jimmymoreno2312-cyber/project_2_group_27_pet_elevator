CC     = gcc
CFLAGS = -Wall -Wextra

.PHONY: default clean

default: empty.trace part1.trace

empty: empty.c
	$(CC) $(CFLAGS) -o $@ $<

part1: part1.c
	$(CC) $(CFLAGS) -o $@ $<

empty.trace: empty
	strace -o $@ ./empty

part1.trace: part1
	strace -o $@ ./part1

# Removes the executables only; the .trace files are deliverables and are kept.
clean:
	rm -f empty part1
