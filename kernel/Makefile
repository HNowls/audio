ifneq ($(KERNELRELEASE),)
	obj-m :=all_process.o
else
	PWD :=$(shell pwd)
	KVER ?=$(shell uname -r)
	KDIR := /lib/modules/$(KVER)/build
all:
	$(MAKE) -C $(KDIR) M=$(PWD)
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers  modul*
endif
