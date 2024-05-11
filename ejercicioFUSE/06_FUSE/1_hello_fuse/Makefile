# ejemplo de uso FUSE para DSO 2020
# hemos instalado el paquete de desarrollo de FUSE con:
# sudo apt-get install fuse-dev

fuse_flags= -D_FILE_OFFSET_BITS=64 -lfuse -pthread
EXEC=hello_fuse
DIR=mount_point

.PHONY : mount umount test debug clean

$(EXEC): hello_fuse.c
	gcc -o $@ $^ ${fuse_flags}
	mkdir -p $(DIR)
mount: 
	./hello_fuse $(DIR)
debug:
	./hello_fuse -d $(DIR)
umount:
	fusermount -u $(DIR)
test: 
	ls $(DIR)
	cat $(DIR)/hello

clean:
	rm $(EXEC)
	rmdir $(DIR)
