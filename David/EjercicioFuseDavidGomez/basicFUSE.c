/*
 * FUSE: Filesystem in Userspace
 * DSO 2014
 * Ejemplo para montar un libro de poesía como sistema de ficheros
 * Cada capítulo del libro será un fichero diferente
 * 
 * Compilarlo con make
 *  
 * uso:  basicFUSE [opciones FUSE] fichero_inicial punto_de_montaje
 * 
 *  ./basicFUSE proverbiosycatares.txt punto_montaje
 * 
*/
#define FUSE_USE_VERSION 26

#include "basicFUSE_lib.h"

#include <stdlib.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>


static const char *big_path = "BIG";
static const char *little_path = "little";
/*
 *  Para usar los datos pasados a FUSE usar en las funciones:
 * 
	struct structura_mis_datos *mis_datos= (struct structura_mis_datos *) fuse_get_context()->private_data;
 * 
 * */

/***********************************
 * */
static int mi_getattr(const char *path, struct stat *stbuf)
{
	/* completar */
	printf("Inicia getattr\n");
	struct structura_mis_datos *mis_datos= (struct structura_mis_datos *) fuse_get_context()->private_data;
	
	int i;
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 4;
		stbuf->st_uid = mis_datos->st_uid;
		stbuf->st_gid = mis_datos->st_gid;
		
		stbuf->st_atime = mis_datos->st_atime;
		stbuf->st_mtime = mis_datos->st_mtime;
		stbuf->st_ctime = mis_datos->st_ctime;
		stbuf->st_size = 1024;
		stbuf->st_blocks = 2;
	}else if ((i= buscar_fichero(path, mis_datos)) >= 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		
		stbuf->st_uid = mis_datos->st_uid;
		stbuf->st_gid = mis_datos->st_gid;
		
		stbuf->st_atime = mis_datos->st_atime;
		stbuf->st_mtime = mis_datos->st_mtime;
		stbuf->st_ctime = mis_datos->st_ctime;
		
		stbuf->st_size = strlen(mis_datos->contenido_ficheros[i]);
		stbuf->st_blocks = stbuf->st_size/512 + (stbuf->st_size%512)? 1 : 0;
		printf("datos guardados del capitulo %s en /\n", mis_datos->nombre_ficheros[i]);
	}else if(strcmp(path+1, big_path) == 0){
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		stbuf->st_uid = mis_datos->st_uid;
		stbuf->st_gid = mis_datos->st_gid;
		
		stbuf->st_atime = mis_datos->st_atime;
		stbuf->st_mtime = mis_datos->st_mtime;
		stbuf->st_ctime = mis_datos->st_ctime;
		stbuf->st_size = 1024;
		stbuf->st_blocks = 2;
	}else if ((i= buscar_fichero(path+4, mis_datos)) >= 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		
		stbuf->st_uid = mis_datos->st_uid;
		stbuf->st_gid = mis_datos->st_gid;
		
		stbuf->st_atime = mis_datos->st_atime;
		stbuf->st_mtime = mis_datos->st_mtime;
		stbuf->st_ctime = mis_datos->st_ctime;
		
		stbuf->st_size = strlen(mis_datos->contenido_ficheros[i]);
		stbuf->st_blocks = stbuf->st_size/512 + (stbuf->st_size%512)? 1 : 0;
		printf("datos guardados del capitulo %s en BIG\n", mis_datos->nombre_ficheros[i]);
	}else if(strcmp(path+1, little_path) == 0){
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		stbuf->st_uid = mis_datos->st_uid;
		stbuf->st_gid = mis_datos->st_gid;
		
		stbuf->st_atime = mis_datos->st_atime;
		stbuf->st_mtime = mis_datos->st_mtime;
		stbuf->st_ctime = mis_datos->st_ctime;
		stbuf->st_size = 1024;
		stbuf->st_blocks = 2;

	}else if ((i= buscar_fichero(path+7, mis_datos)) >= 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		
		stbuf->st_uid = mis_datos->st_uid;
		stbuf->st_gid = mis_datos->st_gid;
		
		stbuf->st_atime = mis_datos->st_atime;
		stbuf->st_mtime = mis_datos->st_mtime;
		stbuf->st_ctime = mis_datos->st_ctime;
		
		stbuf->st_size = strlen(mis_datos->contenido_ficheros[i]);
		stbuf->st_blocks = stbuf->st_size/512 + (stbuf->st_size%512)? 1 : 0;
		printf("datos guardados del capitulo %s en little\n", mis_datos->nombre_ficheros[i]);
	}else{
		res = -ENOENT;
	}
	return res;
	
}

/***********************************
 * */

static int mi_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	printf("Inicia readdir\n");
	//soy consciente de que no estoy usando estos parámetros 
	(void) offset;
	(void) fi;
	/* completar*/
	int i;
	
	struct structura_mis_datos *mis_datos= (struct structura_mis_datos *) fuse_get_context()->private_data;
	if (strcmp(path, "/") == 0){
		if(filler(buf, "." , NULL, 0)!=0) return -ENOMEM;
		if(filler(buf, "..", NULL, 0)!=0) return -ENOMEM;
		if(filler(buf, big_path,NULL,0)!=0) return -ENOMEM;
		if(filler(buf,little_path,NULL,0) != 0) return -ENOMEM;
		for (i=0; i< mis_datos->numero_ficheros; i++)
		{
			printf("nombre: %s está en /\n", mis_datos->nombre_ficheros[i]);
			if (filler(buf,mis_datos->nombre_ficheros[i], NULL, 0) != 0) return -ENOMEM;
		}
	}else if(strncmp(path+1,big_path,3) == 0){
		if(filler(buf, "." , NULL, 0)!=0) return -ENOMEM;
		if(filler(buf, "..", NULL, 0)!=0) return -ENOMEM;
		for (i=0; i< mis_datos->numero_ficheros; i++)
		{
			if(strlen(mis_datos->contenido_ficheros[i]) >= 256){
				printf("nombre: %s está en BIG\n", mis_datos->nombre_ficheros[i]);
				if (filler(buf,mis_datos->nombre_ficheros[i], NULL, 0) != 0) return -ENOMEM;
			}
		}
	}else if(strncmp(path+1, little_path,6) == 0){
		if(filler(buf, "." , NULL, 0)!=0) return -ENOMEM;
		if(filler(buf, "..", NULL, 0)!=0) return -ENOMEM;
		for (i=0; i< mis_datos->numero_ficheros; i++)
		{
			if(strlen(mis_datos->contenido_ficheros[i]) < 256){
				printf("nombre: %s está en little\n", mis_datos->nombre_ficheros[i]);
				if (filler(buf,mis_datos->nombre_ficheros[i], NULL, 0) != 0) return -ENOMEM;
			}
		}
	}else{
		return -ENOENT;
	}
		
	
	return 0;
}

/***********************************
 * */
static int mi_open(const char *path, struct fuse_file_info *fi)
{
	printf("Inicia mi_open\n");
	
	int pos_capitulo = -1;
	struct structura_mis_datos *mis_datos= (struct structura_mis_datos *) fuse_get_context()->private_data;
	if(strcmp(path, "/") == 0){
		pos_capitulo = buscar_fichero(path, mis_datos);
		if((fi->flags & 3) != O_RDONLY) return -EACCES;
	
		if(pos_capitulo >= 0){
			fi->fh = pos_capitulo;
			printf("mi open: nombre: %s en /\n", mis_datos->nombre_ficheros[pos_capitulo]);
		}
	}else if(strncmp(path+1, big_path,3)==0){
		pos_capitulo = buscar_fichero(path+4, mis_datos);
		if((fi->flags & 3) != O_RDONLY) return -EACCES;
	
		if(pos_capitulo >= 0){
			fi->fh = pos_capitulo;
			printf("mi open: nombre: %s en BIG\n", mis_datos->nombre_ficheros[pos_capitulo]);
		}
	}else if(strncmp(path+1, little_path,6)==0){
		pos_capitulo = buscar_fichero(path+7, mis_datos);
		if((fi->flags & 3) != O_RDONLY) return -EACCES;
	
		if(pos_capitulo >= 0){
			fi->fh = pos_capitulo;
			printf("mi open: nombre: %s en little\n", mis_datos->nombre_ficheros[pos_capitulo]);
		}
	}
	
	
	
	
	
	return 0;
}


/***********************************
 * */
static int mi_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{	
	printf("inicia mi_read\n");
	int pos = fi->fh;
	struct structura_mis_datos *mis_datos= (struct structura_mis_datos *) fuse_get_context()->private_data;
	size_t len;
	if(strcmp(path, "/") == 0){
		len = strlen(mis_datos->contenido_ficheros[pos]);
		if(pos > 0){
			if(offset < len){
				if (offset +size > len) size = len -offset;
				memcpy(buf, mis_datos->contenido_ficheros[pos] + offset, size);
			}else size = 0; 
		}
	}else if(strncmp(path+1, big_path,3) == 0){
		len = strlen(mis_datos->contenido_ficheros[pos]);
		if(pos > 0){
			if(offset < len){
				if (offset +size > len) size = len -offset;
				memcpy(buf, mis_datos->contenido_ficheros[pos] + offset, size);
			}else size = 0; 
		}
	}else if(strncmp(path+1, little_path,6) == 0){
		len = strlen(mis_datos->contenido_ficheros[pos]);
		if(pos > 0){
			if(offset < len){
				if (offset +size > len) size = len -offset;
				memcpy(buf, mis_datos->contenido_ficheros[pos] + offset, size);
			}else size = 0; 
		}
	}
	
	return size;
	/* completar */
	
}


/***********************************
 * operaciones FUSE
 * */
static struct fuse_operations basic_oper = {
	.getattr	= mi_getattr,
	.readdir	= mi_readdir,
	.open		= mi_open,
	.read		= mi_read,
};


/***********************************
 * */
int main(int argc, char *argv[])
{
	printf("Inicia el programa\n");
	//argc = numero de palabras separada por separadores, ¿Dónde están las palabras?
	//argv = va de argv[0] hasta argv[argc-1] cada contenido es una zona de memoria donde tengo caracteres
	struct structura_mis_datos *mis_datos;
	
	mis_datos=malloc(sizeof(struct structura_mis_datos));
	 printf("parametros de entrada\n");
	// análisis parámetros de entrada
	if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-')) error_parametros();
	mis_datos->fichero_inicial = strdup(argv[argc-2]); // fichero donde están los capítulos
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    //accedemos al nombre del fichero que está en la posición de memoria argv[argc-2]
    //Los dos últimos se suelen reservar para cosas del fuse así que decrementamos en uno argc y movemos la posición de memoria de los dos último a -1 espacio
    leer_fichero(mis_datos);
    
    /*int i;
    for(i=0; i<mis_datos->numero_ficheros; i++)
    {
		printf("----->  %s\n", mis_datos->nombre_ficheros[i]);
		printf("%s",mis_datos->contenido_ficheros[i]);
	}
	exit(0);*/

	printf("Vamos a comenzar\n");
	return fuse_main(argc, argv, &basic_oper, mis_datos);
}
