CONFIG_MODULE_SIG=n

obj-m += abs.o
obj-m += abs_test_devices.o

all: build

build:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

debug:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) EXTRA_CFLAGS="-DDEBUG" modules

