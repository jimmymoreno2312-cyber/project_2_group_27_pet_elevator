obj-m := my_timer.o
my_timer-y := src/my_timer.o

KDIR := /lib/modules/$(shell uname -r)/build

.PHONY: default clean

default:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
