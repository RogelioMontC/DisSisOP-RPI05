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

#include "basicFUSE_lib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/***********************************
 * Busca un fichero path en mi estructura de datos
 * */
int buscar_fichero(const char *path, struct structura_mis_datos *mis_datos)
{
	int i;
	
	for(i=0; i< mis_datos->numero_ficheros;i++)
	{
		if(strcmp(mis_datos->nombre_ficheros[i],path+1)==0)
		{
			return i;
		}
	}
	return -1;
}



/***********************************
 * */
void error_parametros()
{
    fprintf(stderr, "uso:  basicFUSE [opciones FUSE] fichero_inicial punto_de_montaje\n");
    exit(-1);
}

void leer_fichero(struct structura_mis_datos *mis_datos)
{
	FILE *f;
	int i=0;
	char nombre[MAX_NOMBRE];
	char contenido[MAX_CONTENIDOS];
	struct stat fileStat;
	
	f=fopen(mis_datos->fichero_inicial,"rc");
	if (f==NULL) error_parametros();
	
	nombre[0]='X';
	
	while (!feof(f))
	{
		if(fscanf(f," X%[LXVI]s ",nombre+1))
		{
			if(strcmp(nombre,"XX")==0) 
				mis_datos->nombre_ficheros[i]=strdup("largo");
			else
				mis_datos->nombre_ficheros[i]=strdup(nombre);
			fscanf(f," %[^X]s ",contenido);
			mis_datos->contenido_ficheros[i]=strdup(contenido);
			i++;
		}
	}
	
	mis_datos->numero_ficheros=i;
	
	fstat(fileno(f), &fileStat);
	mis_datos->st_uid= fileStat.st_uid;
	mis_datos->st_gid= fileStat.st_gid;
	mis_datos->st_atime = fileStat.st_atime;
	mis_datos->st_ctime = fileStat.st_ctime;
	mis_datos->st_mtime = fileStat.st_mtime;
		      
	fclose(f);
}
