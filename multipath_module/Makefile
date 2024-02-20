CONFIG_MODULE_SIG=n

obj-m += multipath_module.o

KERNEL_SRC := /lib/modules/$(shell uname -r)/build

# VPATH := $(PWD)

# ccflags-y := -I$(src) -I$(KERNEL_SRC)/include

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean 