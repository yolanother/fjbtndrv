FSCD_CFLAGS	:= -g -O2
FSCD_CXXFLAGS	:= $(CFLAGS)

FSCD_INCS	:= -I/usr/include
FSCD_LIBS	:= -lX11 -lXi -lXext -lXtst -lXrandr

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

################################################################################

obj-m	+= fsc_btns.o

KERNELRELEASE	?= $(shell uname -r)
KERNEL_SOURCE	?= /lib/modules/$(KERNELRELEASE)/build
PWD		:= $(shell pwd)

modules:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules

clean::
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) clean
	rm -f Module.symvers

################################################################################

.c.o:
	$(CC) $(FSCD_CFLAGS) $(FSCD_INCS) -o $@ -c $<

fscd: fscd.o
	$(CC) $(FSCD_CFLAGS) $(FSCD_LIBS) -o $@ $^

clean::
	rm -f *.o fscd

