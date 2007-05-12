FSCD_CFLAGS	:= -g -O2
FSCD_CXXFLAGS	:= $(CFLAGS)

FSCD_INCLUDES	:= -I/usr/include
FSCD_LIBS	:= -lX11 -lXi -lXext -lXtst -lXrandr

all: modules fscd


################################################################################

ifdef DEBUG
ifneq ($(DEBUG), n)
  FSCD_CFLAGS	+= -DDEBUG
endif
endif

ifdef WACOM
ifneq ($(WACOM), n)
  FSCD_CFLAGS	+= -DENABLE_WACOM
  FSCD_INCLUDES	+= -I.
  FSCD_LIBS	+= -lwacomcfg
endif
endif

ifdef OSD
ifneq ($(OSD), n)
  FSCD_CFLAGS	+= -DENABLE_XOSD
  FSCD_LIBS	+= -lxosd
  ifdef OSD_VERBOSE # 1..3
    FSCD_CFLAGS	+= -DXOSD_VERBOSE=$(OSD_VERBOSE)
  else
    FSCD_CFLAGS	+= -DXOSD_VERBOSE=2
  endif
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
	$(CC) $(FSCD_CFLAGS) $(FSCD_INCLUDES) -o $@ -c $<

fscd: fscd.o
	$(CC) $(FSCD_CFLAGS) $(FSCD_LIBS) -o $@ $^

clean::
	rm -f *.o fscd

