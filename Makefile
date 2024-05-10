ifneq ($(KERNELRELEASE),)
	obj-m := abstract_driver.o
else

	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif