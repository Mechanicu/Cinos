#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
// #include <linux/unistd.h>

int main(int argc, char **argv)
{
    unsigned long i       = 0;
    // vsyscall static map addr
    time_t (*f)(time_t *) = (time_t(*)(time_t *))0xffffffffff600400UL;

    if (!strcmp(argv[1], "vsyscall")) {
        for (i = 0; i < 1000000; ++i) {
            f(NULL);
        }
    } else if (!strcmp(argv[1], "vdso")) {
        for (i = 0; i < 1000000; ++i) {
            time(NULL);
        }
    } else {
        for (i = 0; i < 1000000; ++i) {
            // _syscall(SYS_time, NULL);
            syscall(__NR_time, NULL);
        }
    }

    return 0;
}