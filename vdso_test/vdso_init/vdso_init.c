#include "../../log.h"
#include "../config.h"
#include "../vdso.h"
#include <string.h>

/* Straight from the ELF specification. */
static inline unsigned long elf_hash(const unsigned char *name)
{
    unsigned long h = 0;
    unsigned long g = 0;
    while (*name) {
        h = (h << 4) + *name++;
        if ((g = h & 0xf0000000) != 0) {
            h ^= g >> 24;
        }
        h &= ~g;
    }
    return h;
}

static inline void vdso_init_from_sysinfo_ehdr(const unsigned long base, vdso_info_t *vdso_info)
{
    size_t i;
    int    found_vaddr   = 0;
    vdso_info->valid     = 0;
    vdso_info->load_addr = base;
    ELF(Ehdr) *hdr       = (ELF(Ehdr) *)base;

    if (hdr->e_ident[EI_CLASS] !=
        (ELF_BITS == 32 ? ELFCLASS32 : ELFCLASS64)) {
        return; /* Wrong ELF class -- check ELF_BITS */
    }

    ELF(Phdr) *pt = (ELF(Phdr) *)(vdso_info->load_addr + hdr->e_phoff);
    ELF(Dyn) *dyn = 0;
    LOG_DEBUG("VDSO start:0x0x%lx, ph_tab:0x%p", vdso_info->load_addr, pt);
    /*
     * We need two things from the segment table: the load offset
     * and the dynamic table.
     */
    for (i = 0; i < hdr->e_phnum; i++) {
        if (pt[i].p_type == PT_LOAD && !found_vaddr) {
            found_vaddr            = 1;
            vdso_info->load_offset = base + (unsigned long)pt[i].p_offset - (unsigned long)pt[i].p_vaddr;
            LOG_DEBUG("VDSO load_offset:0x%lx, p_offset:0x%lx, p_vaddr:0x%lx", vdso_info->load_offset, (unsigned long)pt[i].p_offset, (unsigned long)pt[i].p_vaddr);

        } else if (pt[i].p_type == PT_DYNAMIC) {
            dyn = (ELF(Dyn) *)(base + pt[i].p_offset);
            LOG_DEBUG("VDSO dyn:0x%p", dyn);
        }
    }

    if (!found_vaddr || !dyn) {
        return; /* Failed */
    }

    /*
     * Fish out the useful bits of the dynamic table.
     */
    ELF(Word) *sysv_hash  = 0;
    vdso_info->symstrings = 0;
    vdso_info->symtab     = 0;
    vdso_info->versym     = 0;
    vdso_info->verdef     = 0;
    for (i = 0; dyn[i].d_tag != DT_NULL; i++) {
        switch (dyn[i].d_tag) {
            case DT_STRTAB:
                vdso_info->symstrings = (const char *)((unsigned long)dyn[i].d_un.d_ptr + vdso_info->load_offset);
                break;
            case DT_SYMTAB:
                vdso_info->symtab = (ELF(Sym) *)((unsigned long)dyn[i].d_un.d_ptr + vdso_info->load_offset);
                break;
            case DT_HASH:
                sysv_hash = (ELF(Word) *)((unsigned long)dyn[i].d_un.d_ptr + vdso_info->load_offset);
                break;
            case DT_VERSYM:
                vdso_info->versym = (ELF(Versym) *)((unsigned long)dyn[i].d_un.d_ptr + vdso_info->load_offset);
                break;
            case DT_VERDEF:
                vdso_info->verdef = (ELF(Verdef) *)((unsigned long)dyn[i].d_un.d_ptr + vdso_info->load_offset);
                break;
        }
    }

    LOG_DEBUG("VDSO symstrings:0x%p, symtab:0x%p, versym:0x%p, sysv_hash:0x%p, verdef:0x%p",
              vdso_info->symstrings,
              vdso_info->symtab,
              vdso_info->versym,
              sysv_hash,
              vdso_info->verdef);
    if (!vdso_info->symstrings || !vdso_info->symtab || !sysv_hash) {
        return; /* Failed */
    }
    if (!vdso_info->verdef) {
        vdso_info->versym = 0;
    }

    /* Parse the sysv_hash table header. */
    vdso_info->nbucket = sysv_hash[0];
    vdso_info->nchain  = sysv_hash[1];
    vdso_info->bucket  = &sysv_hash[2];
    vdso_info->chain   = &sysv_hash[vdso_info->nbucket + 2];

    LOG_DEBUG("VDSO sysv_hash: nbucket=%u, nchain=%u, bucket=0x%p, chain=0x%p",
              vdso_info->nbucket,
              vdso_info->nchain,
              vdso_info->bucket,
              vdso_info->chain);
    /* That's all we need. */
    vdso_info->valid = 1;
}

void *vdso_sym(const void *vdso_base, const char *version, const char *name, vdso_info_t *vdso_info)
{
    //
    if ((!vdso_base && !vdso_info) || !name) {
        LOG_ERROR("VDSO vdso_sym: invalid arguments");
        return NULL;
    }
    vdso_info_t vdso_tmp_info = {0};
    if (!vdso_info) {
        LOG_WARING("VDSO vdso_info is null, use tmp one");
        vdso_info = &vdso_tmp_info;
    }
    if (vdso_base) {
        LOG_DEBUG("VDSO init vdso_info by base:%p", vdso_base);
        vdso_init_from_sysinfo_ehdr((const unsigned long)vdso_base, vdso_info);
    }

    if (!vdso_info->valid) {
        return 0;
    }
    // unsigned long ver_hash;
    // ver_hash = elf_hash(version);
    LOG_DEBUG("VDSO sym_name:%s, sym_version:%s", name, version);
    ELF(Word)
    chain = vdso_info->bucket[elf_hash((const unsigned char *)name) % vdso_info->nbucket];
    LOG_DEBUG("VDSO sym_bucket:%lu", elf_hash((const unsigned char *)name) % vdso_info->nbucket);

    for (; chain != STN_UNDEF; chain = vdso_info->chain[chain]) {
        ELF(Sym) *sym = &vdso_info->symtab[chain];
        LOG_DEBUG("VDSO sym_chain:%u", chain);

        /* Check for a defined global or weak function w/ right name. */
        if (ELF64_ST_TYPE(sym->st_info) != STT_FUNC) {
            continue;
        }
        if (ELF64_ST_BIND(sym->st_info) != STB_GLOBAL &&
            ELF64_ST_BIND(sym->st_info) != STB_WEAK) {
            continue;
        }
        if (sym->st_shndx == SHN_UNDEF) {
            continue;
        }
        if (strcmp(name, vdso_info->symstrings + sym->st_name)) {
            continue;
        }

        // /* Check symbol version. */
        // if (vdso_info.versym && !vdso_match_version(vdso_info.versym[chain],
        //                                             version, ver_hash)) {
        //     continue;
        // }
        LOG_DEBUG("VDSO sym_name:%s, sym_addr:%p", name, (void *)(vdso_info->load_offset + sym->st_value));

        return (void *)(vdso_info->load_offset + sym->st_value);
    }

    return 0;
}