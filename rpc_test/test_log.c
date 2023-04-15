#include <stdio.h>

#define LOG(fmt, ...) printf("[%s:%d] " fmt "", __FILE__, __LINE__, ##__VA_ARGS__)

int main(int argc, char **argv)
{
    return 0;
}