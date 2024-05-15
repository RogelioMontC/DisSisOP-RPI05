MODULE=driver

KERNEL_SRC=/lib/modules/`uname -r`/build

obj-m += $(MODULE).o

time_waiting=70

compile:
	make -C $(KERNEL_SRC) M=${CURDIR} modules

install:
	sudo insmod $(MODULE).ko time_waiting=$(time_waiting) 
	sudo dmesg | tail

uninstall:
	sudo rmmod $(MODULE)
	sudo dmesg | tail
