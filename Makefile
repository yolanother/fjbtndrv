obj-m	+= fsc_btns.o

KERNELRELEASE	?= $(shell uname -r)
KERNEL_SOURCE	?= /lib/modules/$(KERNELRELEASE)/build
PWD		:= $(shell pwd)

all: modules

modules:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) clean

