#!/usr/bin/env bash
#
# Part 3a: add the pet elevator system calls (548-550) to a Linux kernel source tree.
#
# Usage: ./patch_kernel.sh /path/to/linux-6.16.x
#
# What it does (safe to run twice; each step is skipped if already applied):
#   1. creates <kernel>/syscalls/ with syscalls.c (from this directory) and a Makefile
#   2. adds syscalls/ to the kernel's list of build directories (the Kbuild file in 6.16,
#      the core-y line of the top-level Makefile in older kernels)
#   3. appends entries 548-550 to arch/x86/entry/syscalls/syscall_64.tbl
#   4. adds the sys_* prototypes to include/linux/syscalls.h
# Nothing is modified unless every file it needs to edit looks as expected.
# Originals are saved once as <file>.pre-elevator.

set -euo pipefail

if [ $# -ne 1 ]; then
	echo "usage: $0 /path/to/linux-kernel-source" >&2
	exit 2
fi

HERE=$(cd "$(dirname "$0")" && pwd)
KDIR=$(cd "$1" && pwd)
MAKEFILE=$KDIR/Makefile
KBUILD=$KDIR/Kbuild
TBL=$KDIR/arch/x86/entry/syscalls/syscall_64.tbl
HDR=$KDIR/include/linux/syscalls.h
SYSDIR=$KDIR/syscalls

die() { echo "error: $*" >&2; exit 1; }

# ---- checks (nothing is modified until all of these pass) ----
[ -f "$HERE/syscalls.c" ] || die "$HERE/syscalls.c not found"
for f in "$MAKEFILE" "$TBL" "$HDR"; do
	[ -f "$f" ] || die "$f not found - is $KDIR a kernel source tree?"
done

# Where the kernel lists its build directories: Kbuild (obj-y += kernel/) in newer
# kernels, the top-level Makefile (core-y += kernel/ ...) in older ones.
DIR_FILE=
DIR_LINE=
DIR_STYLE=
if grep -Eqs '^(obj-y|core-y)[[:space:]]*\+=.*(^|[[:space:]])syscalls/' "$KBUILD" "$MAKEFILE"; then
	:	# already listed
elif DIR_LINE=$(grep -n -E -m1 '^obj-y[[:space:]]*\+=[[:space:]]*kernel/[[:space:]]*$' \
		"$KBUILD" 2>/dev/null | cut -d: -f1) && [ -n "$DIR_LINE" ]; then
	DIR_FILE=$KBUILD
	DIR_STYLE=obj-y
else
	DIR_LINE=$(grep -n -E -m1 '^core-y[[:space:]]*\+=.*[[:space:]]kernel/' "$MAKEFILE" |
		cut -d: -f1 || true)
	[ -n "$DIR_LINE" ] || die "no 'obj-y += kernel/' in Kbuild or 'core-y += kernel/' in" \
		"Makefile; add syscalls/ to the kernel's directory list by hand"
	if sed -n "${DIR_LINE}p" "$MAKEFILE" | grep -q '\\$'; then
		die "the core-y line in Makefile continues on the next line; add syscalls/ by hand"
	fi
	DIR_FILE=$MAKEFILE
	DIR_STYLE=core-y
fi

HDR_END=
if ! grep -q 'sys_start_elevator' "$HDR"; then
	HDR_END=$(grep -n '^#endif' "$HDR" | tail -1 | cut -d: -f1 || true)
	[ -n "$HDR_END" ] || die "no closing #endif found in $HDR"
fi

VERSION=$(sed -n -e 's/^VERSION = //p' -e 's/^PATCHLEVEL = //p' -e 's/^SUBLEVEL = //p' \
	"$MAKEFILE" | paste -sd. -)
echo "Kernel tree: $KDIR (version ${VERSION:-unknown})"

backup() { [ -e "$1.pre-elevator" ] || cp -p "$1" "$1.pre-elevator"; }

# ---- 1. syscalls/ directory ----
mkdir -p "$SYSDIR"
cp "$HERE/syscalls.c" "$SYSDIR/syscalls.c"
printf 'obj-y := syscalls.o\n' > "$SYSDIR/Makefile"
echo "[1/4] syscalls/syscalls.c and syscalls/Makefile installed"

# ---- 2. kernel directory list ----
if [ -z "$DIR_FILE" ]; then
	echo "[2/4] syscalls/ is already in the kernel build - skipped"
elif [ "$DIR_STYLE" = obj-y ]; then
	backup "$DIR_FILE"
	sed -i "${DIR_LINE}a obj-y\t\t\t+= syscalls/" "$DIR_FILE"
	echo "[2/4] added 'obj-y += syscalls/' to Kbuild"
else
	backup "$DIR_FILE"
	sed -i "${DIR_LINE}s|\$| syscalls/|" "$DIR_FILE"
	echo "[2/4] added syscalls/ to core-y in Makefile"
fi

# ---- 3. syscall table ----
if grep -q 'start_elevator' "$TBL"; then
	echo "[3/4] syscall_64.tbl already has the elevator calls - skipped"
else
	backup "$TBL"
	[ -z "$(tail -c1 "$TBL")" ] || echo >> "$TBL"	# make sure the file ends in a newline
	printf '548\tcommon\tstart_elevator\tsys_start_elevator\n' >> "$TBL"
	printf '549\tcommon\tissue_request\tsys_issue_request\n' >> "$TBL"
	printf '550\tcommon\tstop_elevator\tsys_stop_elevator\n' >> "$TBL"
	echo "[3/4] appended 548-550 to syscall_64.tbl"
fi

# ---- 4. prototypes ----
if [ -n "$HDR_END" ]; then
	backup "$HDR"
	TMP=$(mktemp)
	trap 'rm -f "$TMP"' EXIT
	{
		head -n $((HDR_END - 1)) "$HDR"
		cat <<'PROTOS'

/* Pet elevator system calls (see syscalls/syscalls.c) */
asmlinkage long sys_start_elevator(void);
asmlinkage long sys_issue_request(int start_floor, int destination_floor, int type);
asmlinkage long sys_stop_elevator(void);

PROTOS
		tail -n +"$HDR_END" "$HDR"
	} > "$TMP"
	cat "$TMP" > "$HDR"
	echo "[4/4] added prototypes to include/linux/syscalls.h"
else
	echo "[4/4] syscalls.h already has the prototypes - skipped"
fi

cat <<NEXT_STEPS

Done. Next steps (see README, Part 3):
  cd $KDIR
  cp /boot/config-\$(uname -r) .config && make olddefconfig
  scripts/config --disable SYSTEM_TRUSTED_KEYS --disable SYSTEM_REVOCATION_KEYS
  make menuconfig && make -j\$(nproc)
  sudo make modules_install && sudo make install && sudo reboot
NEXT_STEPS
