# Virtual Parking Sensor - top level Makefile
#
#   make                 build driver, application and tests
#   make load            insert the kernel module   (START=<cm> optional)
#   make run             start the dashboard application
#   make test            run unit + integration tests (module must be loaded)
#   make unload          remove the kernel module
#   make logs            show the driver's kernel log messages
#   make clean           remove all build artefacts

MODULE := parking_sensor

all: driver app tests

driver:
	$(MAKE) -C driver

app:
	$(MAKE) -C app

tests: app
	$(MAKE) -C tests

load: driver
	sudo insmod driver/$(MODULE).ko $(if $(START),start_distance=$(START))
	@udevadm settle 2>/dev/null || sleep 1
	@ls -l /dev/parksensor

unload:
	sudo rmmod $(MODULE)

reload:
	-sudo rmmod $(MODULE)
	$(MAKE) load

run: app
	./app/parking_monitor

unit: tests
	$(MAKE) -C tests unit

integration: tests
	$(MAKE) -C tests integration

test: unit integration

logs:
	sudo dmesg | grep parksensor | tail -n 30

clean:
	$(MAKE) -C driver clean
	$(MAKE) -C app clean
	$(MAKE) -C tests clean

.PHONY: all driver app tests load unload reload run unit integration test logs clean
