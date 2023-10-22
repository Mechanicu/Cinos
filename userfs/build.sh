SRC_FILE=" userfs_init.c"
SRC_FILE+=" test_init.c"
SRC_FILE+=" userfs_block_rw.c"
SRC_FILE+=" bitmap.c"
SRC_FILE+=" disk_ops.c"
SRC_FILE+=" userfs_heap.c"
SRC_FILE+=" userfs_file_ctrl.c"
SRC_FILE+=" userfs_dentry_hash.c"
gcc -O2 ${SRC_FILE} -I../include -lpthread
