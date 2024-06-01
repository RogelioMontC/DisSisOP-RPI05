/** Tamaño de bloque del dispositivo de bloques */
enum {BLOCK_SIZE = 1024};

/** Estado de operación del dispositivo de bloques */
enum {SUCCESS = 0, E_BADADDR = -1, E_UNAVAIL = -2, E_SIZE = -3};

/** Definición de un dispositivo de bloques */
struct blkdev {
    struct blkdev_ops *ops;		/* operaciones en el dispositivo de bloques */
    void *private;				/* estado privado del dispositivo de bloques */
};

/** Operaciones en un dispositivo de bloques */
struct blkdev_ops {
    int  (*num_blocks)(struct blkdev *dev);
    int  (*read)(struct blkdev *dev, int first_blk, int num_blks, void *buf);
    int  (*write)(struct blkdev *dev, int first_blk, int num_blks, void *buf);
    int  (*flush)(struct blkdev *dev, int first_blk, int num_blks);
    void (*close)(struct blkdev *dev);
};