# Kbuild Makefile for the Virtual Parking Sensor kernel module.
#
#   make            build parking_sensor.ko against the running kernel
#   make clean      remove build artefacts

obj-m := parking_sensor.o
ccflags-y := -I$(src)/../include

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
