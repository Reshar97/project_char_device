KDIR := /lib/modules/$(shell uname -r)/build
DRV_NAME := char_dev
PWD := $(shell pwd)

obj-m := $(DRV_NAME).o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.sumvers *.ko *.mod *.mod*

install:
	insmod $(DRV_NAME).ko

uninstall:
	rmmod $(DRV_NAME)

format:
	clang-format *.c

.PHONY: all clean install uninstall format
