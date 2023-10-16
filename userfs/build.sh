SRC_FILE="userfs_init.c test_init.c userfs_block_rw.c bitmap.c disk_ops.c userfs_heap.c"
gcc ${SRC_FILE} -I../include -lpthread
