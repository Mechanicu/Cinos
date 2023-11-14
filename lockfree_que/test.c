#include "lockfree_queue.h"
#include "log.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define FILE_COPY_PAGE_SIZE     (1 << 10 << 2)
#define FILE_COPY_MAX_CONSUMERS 128

struct file_copy_block {
    size_t   off;
    uint32_t size;
    char     data[0];
};

struct producer_param {
    int      input_fd;
    int      output_fd;
    uint32_t consumers_count;
    uint32_t file_copy_block_size;
    off_t    input_file_size;
};

typedef struct file_copy_block fcp_block_t;
typedef struct producer_param  producer_param_t;

ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);

static lockfree_que_t *g_c2p_que           = NULL;
static lockfree_que_t *g_p2c_que           = NULL;
static atomic_t        remain_blocks_count = {0};

void *consumer_thread(void *arg)
{
    producer_param_t *params    = (producer_param_t *)arg;
    fcp_block_t      *cur_block = NULL;
    while (1) {
        lockfree_obj_t val = {0};
        while (lockfree_deque(g_p2c_que, &val) < 0)
            ;
        cur_block    = (fcp_block_t *)(val.val);
        ssize_t size = 0;
        if ((size = pwrite(params->output_fd, cur_block->data, cur_block->size, cur_block->off)) < 0) {
            perror("Write error");
            exit(1);
        }

        LOCKFREEQUE_MEM_FREE(cur_block);
        int remain = atomic_sub(&remain_blocks_count, 1);
        if (!remain) {
            LOG_DESC(DBG, "Consumer", "Copy finished, exit now");
            exit(0);
        }
    }
    return NULL;
}

void *producer_thread(void *arg)
{
    producer_param_t *params       = (producer_param_t *)arg;
    off_t             blocks_count = (params->input_file_size + params->file_copy_block_size - 1) / params->file_copy_block_size;
    LOG_DESC(DBG, "Producer", "Input fd:%d, file size:0x%lx, output fd:%d, consumers count:%u, block size:0x%x, blocks count:%lu",
             params->input_fd, params->input_file_size, params->output_fd, params->consumers_count, params->file_copy_block_size, blocks_count);
    atomic_set(&remain_blocks_count, blocks_count);

    /*create consumer threads*/
    pthread_t consumers[FILE_COPY_MAX_CONSUMERS];
    for (int i = 0; i < params->consumers_count; i++) {
        pthread_create(&consumers[i], NULL, consumer_thread, (void *)params);
    }

    off_t        roff      = 0;
    fcp_block_t *cur_block = NULL;
    ssize_t      size      = 0;

    while (1) {
        cur_block       = LOCKFREEQUE_MEM_ALLOC(sizeof(fcp_block_t) + params->file_copy_block_size);
        cur_block->off  = roff;
        cur_block->size = params->file_copy_block_size;
        if ((cur_block->size = pread(params->input_fd, cur_block->data, cur_block->size, cur_block->off)) < 0) {
            perror("Read error");
            exit(1);
        }
        lockfree_obj_t val = {.val = cur_block};
        while (lockfree_enque(g_p2c_que, &val) < 0)
            ;
        roff += cur_block->size;
        if (roff >= params->input_file_size) {
            pthread_join(consumers[0], NULL);
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        LOG_DESC(ERR, "Main", "usage: $consumer_count $src_file_path $dest_file_path");
        exit(1);
    }
    pthread_t producer;
    uint32_t  consumers_count = (uint32_t)atoi(argv[1]);
    consumers_count           = consumers_count > FILE_COPY_MAX_CONSUMERS
                                    ? FILE_COPY_MAX_CONSUMERS
                                    : consumers_count;
    char *src_file            = argv[2];
    char *dest_file           = argv[3];

    /**/
    g_c2p_que                 = lockfree_que_create(LOCKFREE_DEFAULT_MAX_QUEOBJ_COUNT);
    g_p2c_que                 = lockfree_que_create(LOCKFREE_DEFAULT_MAX_QUEOBJ_COUNT);
    if (!g_p2c_que || !g_c2p_que) {
        LOG_DESC(ERR, "Main", "Create lockfree queue failed");
        exit(1);
    }
    /**/
    int input_fd  = open(src_file, O_RDWR);
    int output_fd = open(dest_file, O_RDWR | O_CREAT | O_TRUNC, 0777);
    if (input_fd < 0 || output_fd < 0) {
        perror("Open file");
        exit(1);
    }
    off_t input_size = 1 << 10 << 10 << 10;
    // off_t input_size = lseek(input_fd, 0, SEEK_END);
    if (input_size < 0) {
        perror("Get file size");
        exit(1);
    }

    producer_param_t params = {
        .input_fd             = input_fd,
        .output_fd            = output_fd,
        .input_file_size      = input_size,
        .file_copy_block_size = FILE_COPY_PAGE_SIZE,
        .consumers_count      = consumers_count};
    pthread_create(&producer, NULL, producer_thread, &params);
    pthread_join(producer, NULL);
    return 0;
}