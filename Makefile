FSCD_CFLAGS	:= -g -O2
FSCD_CXXFLAGS	:= $(CFLAGS)

FSCD_INCS	:= -I/usr/include -I/usr/include/dbus-1.0
FSCD_LIBS	:= -lX11 -lXi -lXext -lXtst -lXrandr -ldbus-1 -lhal

all: modules fscd

################################################################################
include $(PWD)/config.mk

ifdef DEBUG
ifneq ($(DEBUG), n)
  FSCD_CFLAGS	+= -DDEBUG
endif
endif

ifdef WACOM
ifneq ($(WACOM), n)
  FSCD_CFLAGS	+= -DENABLE_WACOM
  FSCD_INCS	+= -I.
  FSCD_LIBS	+= -lwacomcfg
endif
endif

ifdef XOSD
ifneq ($(XOSD), n)
  FSCD_CFLAGS	+= -DENABLE_XOSD
  FSCD_LIBS	+= -lxosd
endif
endif

PREFIX		?= /usr/local

################################################################################

obj-m	+= fsc_btns.o

KERNELRELEASE	?= $(shell uname -r)
KERNEL_SOURCE	?= /lib/modules/$(KERNELRELEASE)/build

modules: fsc_btns.ko
fsc_btns.ko: fsc_btns.c
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules

install:: fsc_btns.ko modules_install
modules_install:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules_install

clean::
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) clean
	rm -f Module.symvers

################################################################################

.c.o:
	$(CC) $(FSCD_CFLAGS) $(FSCD_INCS) -o $@ -c $<

fscd: fscd.o
	$(CC) $(FSCD_CFLAGS) $(FSCD_LIBS) -o $@ $^

install:: fscd
	install --mode=0755 --owner=root --group=root fscd $(PREFIX)/bin

clean::
	rm -f *.o fscd


.PHONY: modules modules_install install clean
