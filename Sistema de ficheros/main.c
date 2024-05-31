//#include "inodeFuse.h"
//#include "fuse.c"
#include <stdlib.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

/*
    *./main 
    *funcion:(mount, umount, 
                createFile, createDir, 
                readFile, listDir, 
                writeFile, deleteFile, deleteDir, moveFile) 
    *nombreDirectorio
    *nombreFichero
    *(nombreDirectorioNuevo [opcional])
    *(nombreFicheroNuevo [opcional]) 
*/

void error_parametros() {
    fprintf(stderr, "Uso: ./main funcion:(mount, umount, createFile, createDir, readFile, listDir, writeFile, deleteFile, deleteDir, moveFile) nombreFicheo|Directorio, (nombreFichero [opcional]) (nombreDirectorio [opcional])\n");
    exit(1);
}

int main(int argc, char *argv[]) {
//    struct inode    *inode  = (struct inode *)malloc(sizeof(struct inode));
//    struct dentry   *dentry = (struct dentry *)malloc(sizeof(struct dentry));
//    struct file     *file   = (struct file *)malloc(sizeof(struct file));
    
    if((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-')) error_parametros();
    
    /**
     * argc == 3
     * argv[argc-2][0] == ./main
     * argv[argc-1][0] == "funcion"
     * argv[argc][0] == nombre
    */
    if((argc == 3) && (strcmp(argv[argc-2], "createFile") == 0)){
        printf("Creando fichero: %s\n", argv[argc-1]);
        return 0;
    }else if((argc == 3) && (strcmp(argv[argc-2], "createDir") == 0)){
        printf("Creando directorio: %s\n", argv[argc-1]);
        return 0;
    }else if ((argc == 3) && (strcmp(argv[argc-2], "deleteFile") == 0)){
        printf("Borrando fichero: %s\n", argv[argc-1]);
        return 0;
    }else if((argc == 3) && (strcmp(argv[argc-2], "deleteDir") == 0)){
        printf("Borrando directorio: %s\n", argv[argc-1]);
        return 0;
    }else if((argc == 3) && (strcmp(argv[argc-2], "readFile") == 0)){
        printf("Leyendo fichero: %s\n", argv[argc-1]);
        return 0;
    }else if((argc == 3) && (strcmp(argv[argc-2], "listDir") == 0)){
        printf("Leyendo directorio: %s\n", argv[argc-1]);
        return 0;
    }else if((argc == 3) && (strcmp(argv[argc-2], "writeFile") == 0)){
        printf("Escribiendo fichero: %s\n", argv[argc-1]);
        return 0;
    } else
        /*
            * argc == 6
            * argv[argc-6][0] == ./main
            * argv[argc-5][0] == "funcion"
            * argv[argc-4][0] == nombreFichero
            * argv[argc-3][0] == nombreDirectorio
            * argv[argc-2][0] == nombreFicheroNuevo
            * argv[argc-1][0] == nombreDirectorioNuevo
            * 
        */ 
        if ((argc == 6) && (strcmp(argv[argc-5], "moveFile") == 0)) {
        if (strcmp(argv[argc-1], argv[argc-3]) != 0) {
            printf("Moviendo fichero: %s a %s\n", argv[argc-3], argv[argc-1]);
        } else {
            printf("Renombrando fichero: %s como %s\n", argv[argc-4], argv[argc-2]);
        }
        return 0;
    } else if ((argc == 3) && (strcmp(argv[argc-2], "mount") == 0)) {
        printf("Montando: %s\n", argv[argc-1]);
        return 0;
    }else if((argc == 3) && (strcmp(argv[argc-2], "umount") == 0)){
        printf("Desmontando: %s\n", argv[argc-1]);
        return 0;
    }else error_parametros();
    return 0;

    
    //return fuse_main(argc, argv, &fs_oper, inode);
}