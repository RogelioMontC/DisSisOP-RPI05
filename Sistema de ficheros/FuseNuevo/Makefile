EXEC=kkFUSE
DIR=mount_point

fuse_flags=`pkg-config fuse3 --cflags --libs`

.PHONY: mount umount clean

$(EXEC): kkFUSE.o
	gcc -g -o $@  $^ ${fuse_flags}
	mkdir -p $(DIR)
	
kkFUSE.o: kkFUSE.c
	gcc -g -c -o $@  $< ${fuse_flags}

mount: kkFUSE
	./$(EXEC) $(DIR)

umount:
	fusermount -u $(DIR)

clean:
	rm $(EXEC) *.o
	rmdir $(DIR)
