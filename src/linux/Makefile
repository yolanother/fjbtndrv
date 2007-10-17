obj-m		= fsc_btns.o

KERNELRELEASE	= $(shell uname -r)
KERNEL_SOURCE	= /lib/modules/$(KERNELRELEASE)/build
PWD		= $(shell pwd)

all: modules
modules: fsc_btns.ko
modules_install: install

fsc_btns.ko: fsc_btns.c
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules

install: fsc_btns.ko
ifeq ($(DESTDIR),)
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules_install
endif

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
