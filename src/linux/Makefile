KERNELRELEASE	:= $(shell uname -r)
KERNEL_SOURCE	:= /lib/modules/$(KERNELRELEASE)/build
PWD		:= $(shell pwd)

ifdef DEFS
EXTRA_CFLAGS	+= $(DEFS)
endif


obj-m		+= fsc_btns.o

all: fsc_btns.ko
modules: fsc_btns.ko
modules_install: install

fsc_btns.ko: fsc_btns.c
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules

install: fsc_btns.ko
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules_install
	if test -x /sbin/depmod; then /sbin/depmod -a $(KERNELRELEASE); fi

uninstall:
	rm -f /lib/modules/$(KERNELRELEASE)/extra/fsc_btns.ko

clean:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) clean

distclean: clean
	rm -f Module.symvers

distdir: Makefile fsc_btns.c
	cp -p $^ $(distdir)

%:
	@echo; \
	 echo " *** unknown target $@ *** "; \
	 echo

.PHONY: all install modules modules_install clean
