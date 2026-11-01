obj-m := elevator.o
elevator-y := src/elevator.o

KDIR := /lib/modules/$(shell uname -r)/build

.PHONY: default clean

default:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
