montaje= punto_montaje
fichero= proverbiosycantares.txt

fuse_flags= -D_FILE_OFFSET_BITS=64 -lfuse -pthread

basicFUSE : basicFUSE.o basicFUSE_lib.o
	gcc -g -o $@  $^ ${fuse_flags}
	mkdir -p $(montaje)
	touch ${montaje}/flag
	
basicFUSE.o : basicFUSE.c basicFUSE_lib.h
	gcc -g -c -o $@  $< ${fuse_flags}

basicFUSE_lib.o : basicFUSE_lib.c basicFUSE_lib.h
	gcc -g -c -o $@  $<


mount: basicFUSE
	./basicFUSE -o nonempty $(fichero) $(montaje)

debug: basicFUSE
	./basicFUSE -d -o nonempty $(fichero) $(montaje)

umount:
	fusermount -u $(montaje)
