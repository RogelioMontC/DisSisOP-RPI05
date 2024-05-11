EXEC=basicFUSE
DIR=mount_point
FILE= proverbiosycantares.txt

fuse_flags= -D_FILE_OFFSET_BITS=64 -lfuse -pthread

.PHONY: mount umount debug clean

$(EXEC): basicFUSE.o basicFUSE_lib.o
	gcc -g -o $@  $^ ${fuse_flags}
	mkdir -p $(DIR)
	
basicFUSE.o: basicFUSE.c basicFUSE_lib.h
	gcc -g -c -o $@  $< ${fuse_flags}

basicFUSE_lib.o: basicFUSE_lib.c basicFUSE_lib.h
	gcc -g -c -o $@  $<

mount: basicFUSE
	./basicFUSE $(FILE) $(DIR)

debug: basicFUSE
	./basicFUSE -d $(FILE) $(DIR)

umount:
	fusermount -u $(DIR)

clean:
	rm $(EXEC) *.o
	rmdir $(DIR)
