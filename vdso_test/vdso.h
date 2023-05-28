#ifndef _VDSO_H
#define _VDSO_H

#include "../log.h"
#include "config.h"
#include <elf.h>
#include <limits.h>
#include <link.h>

#ifndef ELF_BITS
#if ULONG_MAX > 0xffffffffUL
#define ELF_BITS 64
#else
#define ELF_BITS 32
#endif
#endif

#define PAGE_SIZE                4096

#define ELF_BITS_XFORM2(bits, x) Elf##bits##_##x
#define ELF_BITS_XFORM(bits, x)  ELF_BITS_XFORM2(bits, x)
#define ELF(x)                   ELF_BITS_XFORM(ELF_BITS, x)

// maybe need to check version

#define OK_TYPES                 (1 << STT_NOTYPE | 1 << STT_OBJECT | 1 << STT_FUNC | 1 << STT_COMMON)
#define OK_BINDS                 (1 << STB_GLOBAL | 1 << STB_WEAK | 1 << STB_GNU_UNIQUE)

typedef struct vdso_info {
    int valid;

    /* Load information */
    unsigned long load_addr;
    unsigned long load_offset; /* load_addr - recorded vaddr */

    /* Symbol table */
    ELF(Sym) * symtab;
    const char *symstrings;
    ELF(Word) * bucket;
    ELF(Word) * chain;
    ELF(Word)
    nbucket, nchain;

    /* Version table */
    ELF(Versym) * versym;
    ELF(Verdef) * verdef;
} vdso_info_t;

typedef struct _vdso_addr {
    /* data */
    void *vdso_code_vaddr;
    void *vdso_data_vaddr;
} vdso_addr_t;

//
void *vdso_sym(const void *vdso_base, const char *version, const char *name, vdso_info_t *vdso_info);

#endif
