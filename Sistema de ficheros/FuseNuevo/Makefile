EXEC=fuse
DIR=mount_point

fuse_flags=`pkg-config fuse3 --cflags --libs`

.PHONY: mount umount clean

$(EXEC): fuse.o
	gcc -g -o $@  $^ ${fuse_flags}
	mkdir -p $(DIR)
	
fuse.o: fuse.c
	gcc -g -c -o $@  $< ${fuse_flags}

mount: fuse
	./$(EXEC) $(DIR)

umount:
	fusermount -u $(DIR)

clean:
	rm $(EXEC) *.o
	rmdir $(DIR)
