KERNEL= $(shell uname -r)
KERNEL_SRC=/lib/modules/${KERNEL}/build

MODULE=with_params

obj-m += ${MODULE}.o

.PHONY : compile clean
 
compile:
	make -C ${KERNEL_SRC} M=${CURDIR} modules

clean:
	rm -rf *.o *.mod.c *.symvers *.order .*.cmd

vclean: clean
	rm *.ko *.mod

# Try:
# % make
# % sudo insmod with_params.ko # using default parameters
# % sudo dmesg | tail         
# [423327.549328] Loading module 'with_params'
# [423327.549331] with_params: (string parameter) one:'default string one'
# [423327.549332] with_params: (interger parameter) two= 2
# % sudo rmmod with_params    
# % sudo insmod with_params.ko one=hola two=4
# % sudo dmesg | tail                        
# [423327.549328] Loading module 'with_params'
# [423327.549331] with_params: (string parameter) one:'default string one'
# [423327.549332] with_params: (interger parameter) two= 2
# [423342.893329] with_params: Cleaning up module
# [423342.893332] with_params: Done
# [423351.453332] Loading module 'with_params'
# [423351.453335] with_params: (string parameter) one:'hola'
# [423351.453335] with_params: (interger parameter) two= 4
# % sudo rmmod with_params                   
# 
