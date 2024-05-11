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

#ifndef __BASIC_FUSE__
#define __BASIC_FUSE__

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/***********************************
 * macros y estructura
 * */

#define MAX_NOMBRE 1024        	/* tamaño máximo de nombre de fichero */
#define MAX_FICHEROS 128        /* número máximo de capítulos */
#define MAX_CONTENIDOS 4096*10 	/* tamaño máximo de un capítulo 40KB*/

struct structura_mis_datos
{
	char *fichero_inicial;  				/* fichero que contiene los capítulos */
	int numero_ficheros;     				/* número de capitulos encontrados */
	char *nombre_ficheros[MAX_FICHEROS];	/* nombre de los capítulos */
	char *contenido_ficheros[MAX_FICHEROS];	/* contenido de los capítulos */
	struct timespec st_atim;  				/* fechas del fichero */
    struct timespec st_mtim; 
    struct timespec st_ctim;  
    uid_t     st_uid;        				/* El usuario y grupo */
    gid_t     st_gid;  
    
};


/***********************************
 * Busca un fichero path en mi estructura de datos
 * */
int buscar_fichero(const char *path, struct structura_mis_datos *mis_datos);

/***********************************
 * error en parametros de entrada
 * */
void error_parametros();

/***********************************
 * lee fichero con los capítulos y rellena estructura mis_datos
 * */
void leer_fichero(struct structura_mis_datos *mis_datos);

#endif
