# Elevator

A pet elevator for the Linux kernel (COP4610 Project 2, Group 27). The project has three parts:
tracing system calls with `strace`, a `my_timer` kernel module that exposes `/proc/timer`, and an
`elevator` kernel module that schedules pets (Chihuahua, Pug, Pughuahua, Dachshund) between floors
1-5 using three new system calls (548 `start_elevator`, 549 `issue_request`, 550 `stop_elevator`).
The elevator holds at most 5 pets / 50 lbs and reports its state through `/proc/elevator`.

## Group Members
- **Pedro**: pd23b@fsu.edu
- **Jimmy**: jam23ba@fsu.edu
- **Sarah**: sef21a@fsu.edu

## Division of Labor

### Part 1: System Call Tracing
- **Responsibilities**: Write an empty C program and a copy (`part1.c`) that makes exactly five
  additional system calls, then capture both `strace` outputs to verify the difference.
- **Assigned to**: Pedro, Jimmy

### Part 2: Timer Kernel Module
- **Responsibilities**: Write the `my_timer` module that creates `/proc/timer` on load, removes it on
  unload, and prints the current time and time elapsed since the last read.
- **Assigned to**: Jimmy, Sarah

### Part 3a: Adding System Calls
- **Responsibilities**: Implement `start_elevator`, `issue_request` and `stop_elevator` in the kernel
  (`syscalls.c`, `syscall_64.tbl`, `syscalls.h`, kernel `Makefile`).
- **Assigned to**: Sarah, Pedro

### Part 3b: Kernel Compilation
- **Responsibilities**: Download Linux 6.16.x, configure, compile and install it with the new system
  calls, and verify the install with `uname -r`.
- **Assigned to**: Pedro, Jimmy

### Part 3c: Threads
- **Responsibilities**: Create the kthread that controls elevator movement and its start/stop lifecycle.
- **Assigned to**: Jimmy, Sarah

### Part 3d: Linked List
- **Responsibilities**: Store the pets waiting on each floor and the pets on the elevator in linked
  lists (`linux/list.h`), with pets allocated using `kmalloc`.
- **Assigned to**: Sarah, Pedro

### Part 3e: Mutexes
- **Responsibilities**: Protect data shared between the system calls, the kthread and the proc file
  with a mutex.
- **Assigned to**: Pedro, Jimmy

### Part 3f: Scheduling Algorithm
- **Responsibilities**: Decide which direction to move and which pets to load/unload, respecting the
  5 pet / 50 lb limit, FIFO boarding and the non-circular floors.
- **Assigned to**: Jimmy, Sarah

## File Listing
```
project_2_group_27_pet_elevator/   (repo root)
├── Makefile              # builds part1, part2 and part3
├── README.md
├── part1/
│   ├── empty.c           # empty program (baseline)
│   ├── empty.trace       # strace output of empty
│   ├── part1.c           # empty + exactly five system calls
│   ├── part1.trace       # strace output of part1
│   └── Makefile
├── part2/
│   ├── src/
│   │   └── my_timer.c    # /proc/timer kernel module
│   └── Makefile          # produces my_timer.ko
└── part3/
    ├── src/
    │   └── elevator.c    # elevator kernel module (kthread, mutex, linked lists, /proc/elevator)
    ├── syscalls.c        # kernel-side system calls 548-550 (copied into the kernel tree)
    ├── patch_kernel.sh   # applies syscalls.c and the kernel edits (Part 3a)
    ├── tests/            # provided producer/consumer and syscall tests
    └── Makefile          # produces elevator.ko
```

# How to Compile & Execute

### Requirements
- **Compiler**: `gcc` and `make`
- **Tools**: `strace` (Part 1)
- **Kernel**: Part 2 and Part 3 need the headers of the running kernel
  (`/lib/modules/$(uname -r)/build`). Part 3 also needs the custom Linux 6.16.x kernel with the new
  system calls installed.

Running `make` in the repo root builds every part; `make clean` removes build output.

## Part 1

### Compilation
```bash
cd part1
make
```
This builds `empty` and `part1` in `part1/` and runs both under `strace`, writing `empty.trace` and
`part1.trace`.

### Execution
```bash
wc -l empty.trace part1.trace
```
`part1.trace` must have exactly five more lines than `empty.trace` (the calls are `getpid`,
`getppid`, `getuid`, `getgid`, `geteuid`). `make clean` deletes the executables and keeps the traces.

## Part 2

### Compilation
```bash
cd part2
make
```
This builds the kernel module `part2/my_timer.ko`. Note that kernel builds fail if the path to the
project contains spaces.

### Execution
```bash
sudo insmod my_timer.ko
lsmod | grep my_timer
cat /proc/timer        # current time
sleep 1; cat /proc/timer   # current time and elapsed time since the previous read
sudo rmmod my_timer
```
`/proc/timer` exists only while the module is loaded.

## Part 3

### Adding the system calls to the kernel (done once, on the 6.16.x source)
`part3/syscalls.c` is the kernel side of the system calls. They forward to function pointers
(`STUB_*`) that `elevator.ko` fills in when it is loaded. This needs an **x86-64** Linux VM, because
548-550 are added to the x86 system call table.

Download the source to `/usr/src`, link it into the home directory and apply the patch script (it is
safe to run twice and changes nothing if a file does not look as expected):
```bash
cd /usr/src
sudo wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.16.12.tar.xz
sudo tar -xf linux-6.16.12.tar.xz
sudo chown -R "$USER" linux-6.16.12
ln -s /usr/src/linux-6.16.12 ~/linux-6.16.12

cd <repo>/part3
./patch_kernel.sh ~/linux-6.16.12
```
The script performs these steps, which can also be done by hand in the kernel tree:
1. Create `syscalls/` and copy `part3/syscalls.c` into it, plus a `syscalls/Makefile` containing
   `obj-y := syscalls.o`.
2. Add `obj-y += syscalls/` to the top-level `Kbuild` file. The project description lists the
   top-level `Makefile`, but in 6.16 the build directories are listed in `Kbuild`.
3. Add to `arch/x86/entry/syscalls/syscall_64.tbl`:
   `548 common start_elevator sys_start_elevator`, `549 common issue_request sys_issue_request`,
   `550 common stop_elevator sys_stop_elevator`.
4. Add the three `asmlinkage long sys_...` prototypes to `include/linux/syscalls.h`.

### Compiling and installing the kernel
```bash
sudo apt install build-essential libncurses-dev bison flex libssl-dev libelf-dev dwarves bc
cd ~/linux-6.16.12
cp /boot/config-$(uname -r) .config
make olddefconfig
scripts/config --disable SYSTEM_TRUSTED_KEYS --disable SYSTEM_REVOCATION_KEYS
make menuconfig                    # save and exit
make -j$(nproc)
sudo make modules_install
sudo make install
sudo reboot
uname -r                           # after the reboot: 6.16.12
```
Secure Boot must be off in the VM, otherwise the unsigned modules will not load.

### Checking the system calls
With the custom kernel booted and `elevator.ko` not loaded:
```bash
cd part3/tests/system-calls-test
make
sudo insmod syscheck.ko
./test-syscalls
sudo rmmod syscheck
```

### Compilation
Boot the custom kernel, then:
```bash
cd part3
make
```
This builds `part3/elevator.ko`. It only loads on a kernel that contains the system calls above.

### Execution
```bash
sudo insmod elevator.ko
cd tests/elevator-test && make     # builds producer and consumer
./consumer --start                 # start the elevator
./producer 10                      # create 10 random pets
watch -n 1 cat /proc/elevator
./consumer --stop                  # offload current pets, then go OFFLINE
cd ../.. && sudo rmmod elevator
```

### Design
- Pets are `kmalloc`ed in `issue_request` and freed when they reach their destination. Each floor
  has a FIFO `list_head` queue and boarding moves a pet from the front of that queue to the
  passenger list, so a pet that does not fit blocks those behind it.
- One mutex (`elev_lock`) protects every shared value: floor lists, passenger list, state and
  counters. It is taken by the system calls, the kthread and the `/proc/elevator` read. The kthread
  releases it before every sleep so the system calls and the proc file are never blocked for the
  2 s / 1 s delays, and `kmalloc` happens before it is taken.
- A single kthread moves the elevator: unload, load (FIFO, 5 pets / 50 lbs), then pick a direction
  (keep going while pets are ahead, otherwise reverse). It waits 2 s per floor and 1 s per load.
- `stop_elevator` stops new boardings; the elevator delivers its passengers and then goes OFFLINE.
- `issue_request` returns 1 for an invalid request (floor outside 1-5, same start and destination,
  or unknown type) and 0 otherwise.

## Development Log
Each member records their contributions here.

### Pedro

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-11-01 | Part 1: confirmed `part1.trace` has exactly the 5 added calls over `empty.trace`; removed redundant comments from `part1.c`. |
| 2026-11-01 | Part 3a: ran `patch_kernel.sh` against the Linux 6.16 `Kbuild`, `syscall_64.tbl` and `syscalls.h`; fixed its next-steps order and line widths. |
| 2026-11-01 | Part 3b: wrote the kernel download/build/install steps in this README. The kernel build itself is still to be run on an x86-64 VM. |
| 2026-11-01 | Part 3d/3e: tested `elevator.c` in user space with a pthread stand-in for the kernel API (FIFO, limits, stop/restart, leaks); took the mutex for `elevator_task` in module exit; changed `/proc/elevator` floor lines to the format in the project description. |

### Jimmy

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-11-01 | Part 1: wrote `empty.c`, `part1.c` and Makefile; verified `part1.trace` has 5 more syscalls than `empty.trace`. |
| 2026-11-01 | Part 2: scaffolded `my_timer` module and Makefile; compiles against the installed kernel headers. |
| 2026-11-01 | Part 3: drafted `part3/syscalls.c` and the `elevator` module (`part3/src/elevator.c`); compiles, not yet run on the custom kernel. |
| 2026-11-01 | Updated this README with group members and division of labor. |

### Sarah

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-11-01 | Part 2: added a mutex to my_timer.c so concurrent reads of /proc/timer can't corrupt the last-read time;
| 2026-11-02 | Part 3a: reviewed syscalls.c and the patch script's table/prototype edits for 6.16; ran test-syscalls on the custom kernel.
| 2026-11-02 | Part 3f:  reviewed the LOOK scheduling: unload, then FIFO load within 5 pets / 50 lbs, checked the floor range against the provided producer.


## Meetings
Document in-person meetings, their purpose, and what was discussed.

| Date       | Attendees            | Topics Discussed | Outcomes / Decisions |
|------------|----------------------|------------------|-----------------------|
| YYYY-MM-DD | [Names]              | [Agenda items]   | [Actions/Next steps]  |

## Bugs
- Part 2 and Part 3 have not yet been loaded and tested on the 6.16.x kernel.
- A system call that is in progress while `rmmod elevator` runs could still execute module code
  being unloaded. Stop the elevator and let system calls finish before removing the module.

## Considerations
- Build the kernel in a VM and take a snapshot first, so a broken kernel can be rolled back.
- Kernel modules cannot be built from a path containing spaces, so the project must live in a
  directory without spaces when running `make` for Part 2 and Part 3.
