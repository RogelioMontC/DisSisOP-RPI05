enum {
    FS_BLOCK_SIZE = 1024,		/* tamaño del bloque en bytes */
    FS_MAGIC = 0x37363030		/* número mágico para el superbloque */
};

/**
 *  Entrada en un directorio
 */
enum {FS_FILENAME_SIZE = 28 };
struct fs_dirent {
    uint32_t valid : 1;			/* indicador de entrada válida */
    uint32_t isDir : 1;			/* indicador de directorio */
    uint32_t inode : 30;		/* número de inodo */
    char name[FS_FILENAME_SIZE];/* con el carácter NUL al final */
};								/* total de 32 bytes */

/**
 * Superbloque - contiene los parámetros del sistema de archivos.
 */
struct fs_super {
    uint32_t magic;				/* número mágico */
    uint32_t inode_map_sz;		/* tamaño del mapa de inodos en bloques */
    uint32_t inode_region_sz;	/* tamaño de la región de inodos en bloques */
    uint32_t block_map_sz;		/* tamaño del mapa de bloques en bloques */
    uint32_t num_blocks;		/* número total de bloques, incluyendo el superbloque, mapas de bits, inodos */
    uint32_t root_inode;		/* siempre inodo 1 */

    /* relleno hasta un bloque completo */
    char pad[FS_BLOCK_SIZE - 6 * sizeof(uint32_t)]; 
};								/* total de FS_BLOCK_SIZE bytes */

/**
 * Inodo - contiene información de entrada de archivo
 */
enum {N_DIRECT = 6 };			/* número de entradas directas */
struct fs_inode {
    uint16_t uid;				/* ID de usuario del propietario del archivo */
    uint16_t gid;				/* ID de grupo del propietario del archivo */
    uint32_t mode;				/* permisos | tipo: archivo, directorio, ... */
    uint32_t ctime;				/* hora de creación */
    uint32_t mtime;				/* hora de última modificación */
    int32_t size;				/* tamaño en bytes */
    uint32_t direct[N_DIRECT];	/* punteros a bloques directos */
    uint32_t indir_1;			/* puntero a bloque indirecto simple */
    uint32_t indir_2;			/* puntero a bloque indirecto doble */
    uint32_t pad[3];            /* 64 bytes por inodo */
};								/* total de 64 bytes */

/**
 * Constantes para bloques
 *   DIRENTS_PER_BLK   - número de entradas de directorio por bloque
 *   INODES_PER_BLOCK  - número de inodos por bloque
 *   PTRS_PER_BLOCK    - número de punteros a inodos por bloque
 *   BITS_PER_BLOCK    - número de bits por bloque
 */
enum {
    DIRENTS_PER_BLK = FS_BLOCK_SIZE / sizeof(struct fs_dirent),
    INODES_PER_BLK = FS_BLOCK_SIZE / sizeof(struct fs_inode),
    PTRS_PER_BLK = FS_BLOCK_SIZE / sizeof(uint32_t),
    BITS_PER_BLK = FS_BLOCK_SIZE * 8
};