#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <pthread.h>

static inline pid_t gettid(void)
{
    return syscall(SYS_gettid);
}

static void* get_user_tid_pid(void* arg)
{
    printf("tid:%u, pid:%u\n", gettid(), getpid());
    return NULL;
}

int main(int argc, char **argv)
{
    get_user_tid_pid(NULL);
    for(int i = 0; i < 16; i++) {
        pthread_t tmp = 0;
        pthread_create(&tmp, NULL, get_user_tid_pid, NULL);
    }
    sleep(1);
    return 0;
}
