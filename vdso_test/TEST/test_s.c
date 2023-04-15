#include <stdlib.h>
#include "../../log.h"

void *__vdsosym(const char *vername, const char *name, const void *vaddr_vdso_start);

const char* vdso_syms[] = {
    "_kernel_get_info",
};

extern char vdso_start[];
extern char vdso_end[];

int main(int argc, char *argv[])
{
    LOG_DEBUG("vdso_start:0x%lx, vdso_end:0x%lx", vdso_start, vdso_end);
    // __vdsosym(0, vdso_syms[1], vdso_start);
    return 0;
}