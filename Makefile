MOD_DIR := /lib/modules/$(shell uname -r)/build
DRV_NAME := char_dev 

obj-m += $(DRV_NAME).o

all:
	make -C $(MOD_DIR) M=$(PWD) modules

clean:
	make -C $(MOD_DIR) M=$(PWD) clean

install:
	sudo insmod $(DRV_NAME).ko

uninstall:
	sudo rmmod $(DRV_NAME)

format:
	clang-format *.c

.PHONY: all clean install uninstall format
