KERNEL:= $(shell uname -r)
KERNEL_SRC:=/lib/modules/${KERNEL}/build

ifndef MODULE
$(error MODULE not defined, use: MODULE=<name> make)
endif

obj-m += ${MODULE}.o

.PHONY : compile install uninstall clean
 
install: uninstall compile
	@tput setaf 2
	@echo "instaling module"
	@tput sgr0
	sudo insmod ${MODULE}.ko 
	dmesg | tail 
	@echo "done"

compile:
	@tput setaf 2
	@echo "compiling module"
	@tput sgr0
	make -C ${KERNEL_SRC} M=${CURDIR} modules
	@echo "done"
	
uninstall:
	@if lsmod | grep ${MODULE} ; then\
	  tput setaf 1;\
	  echo "removing module"; \
	  tput sgr0;\
	  sudo rmmod ${MODULE}; \
	  dmesg | tail -4 ;\
	fi
	@echo "done"

clean:
	-rm Module.symvers modules.order ${MODULE}.o ${MODULE}.mod* ${MODULE}.ko
	-rm .Module.symvers.cmd .modules.order.cmd .${MODULE}.*.cmd
