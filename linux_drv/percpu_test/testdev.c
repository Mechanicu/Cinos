#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#define __USE_GNU
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ACCESS_TIMES 16

uint32_t   current_chld_affinity = 0;
extern int sched_setaffinity(__pid_t __pid, size_t __cpusetsize,
                             const cpu_set_t *__cpuset) __THROW;
int        setproc_affinity(const uint32_t affinity_core)
{
    pid_t     selfpid = getpid();
    cpu_set_t mask    = {0};
    CPU_SET(affinity_core, &mask);
    return sched_setaffinity(selfpid, sizeof(mask), &mask);
}

int main(int argc, char const *argv[])
{
    int fd = open("/dev/countdev", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    /*create child with affinity*/
    uint32_t core_count = sysconf(_SC_NPROCESSORS_CONF);
    printf("Current system core count:%u\n", core_count);
    pid_t pid = 1;
    for (int i = 0; i < core_count && (pid != 0); i++ && current_chld_affinity++) {
        pid = fork();
    }
    int      res   = 0;
    uint32_t count = 0;
    /*parent proc*/
    for (int i = 0; i < core_count && (pid != 0); i++) {
        int32_t status;
        int32_t exit_cpid;
        /*summary*/
        exit_cpid  = waitpid(-1, &status, 0);
        count     += WEXITSTATUS(status);
    }
    if (pid != 0) {
        printf("All chld exit, total access times:%d\n", count);
        goto exit;
    }

    /*chld proc*/
    if (setproc_affinity(current_chld_affinity) == -1) {
        perror("Set chld affinity failed");
        return 1;
    }

    struct timeval tv;
    res = gettimeofday(&tv, NULL);
    srand((time_t)(tv.tv_sec) + (time_t)(tv.tv_usec));
    uint32_t access_times = (rand() % MAX_ACCESS_TIMES) + 1;
    printf("Current chld affinity:%x, test times:%d\n", current_chld_affinity, access_times);
    /*start access driver*/
    for (int i = 0; i < access_times; i++) {
        res = ioctl(fd, _IO((uint8_t)0xFF, (uint8_t)0xFF));
        if (res == -1) {
            perror("Chld IO write failed\n");
        }
    }
    printf("Current chld total access times:%d\n", res);
exit:
    close(fd);
    return res;
}