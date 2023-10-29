#include "userfs_file_ops.h"
#include "atomic.h"
#include "inode.h"
#include "log.h"
#include "userfs_dentry_hash.h"
#include "userfs_file_ctrl.h"
#include "userfs_heap.h"
#include "vnode.h"
#include <string.h>
#include <sys/time.h>

#define USERFS_FILE_HOLE_DATA (0)

userfs_bbuf_t *userfs_file_create(
    const char              *name,
    const uint32_t           name_len,
    const uint32_t           dblock_shard_size,
    userfs_super_block_t    *sb,
    linkhash_t              *dentry_hashtable,
    userfs_bbuf_t           *dentry_table_bbuf,
    userfs_bgd_index_list_t *bgd_idx_list)
{
    /*file name should less than 27B*/
    if (name_len > USERFS_MAX_FILE_NAME_LEN) {
        LOG_DESC(ERR, "USERFS FILE CREATE", "File name too long, name len:%u, max len:%u",
                 name_len, USERFS_MAX_FILE_NAME_LEN);
        return NULL;
    }
    /*get current time as file create time stamp*/
    struct timeval file_create_tp = {0};
    if (gettimeofday(&file_create_tp, NULL) < 0) {
        perror("USERFS FILE CREATE");
        return NULL;
    }
    /*alloc dentry for new inode*/
    userfs_dentry_table_t *dentry_table = USERFS_MBLOCK(dentry_table_bbuf->b_data)->dentry_table;
    uint32_t               dentry_pos   = userfs_alloc_dentry(dentry_table);
    if (dentry_pos == UINT32_MAX) {
        LOG_DESC(ERR, "USERFS FILE CREATE", "Alloc dentry for new file failed");
        return NULL;
    }
    LOG_DESC(DBG, "USERFS FILE CREATE", "New dentry pos:%u, first dentry pos:%u, used dentry count:%u",
             dentry_pos, dentry_table->h.dfd_first_dentry, dentry_table->h.dfd_used_dentry_count);
    /*alloc inode for new file*/
    userfs_bbuf_t *new_inode_bbuf = userfs_get_new_inode(sb, bgd_idx_list, dblock_shard_size, &file_create_tp);
    if (!new_inode_bbuf) {
        LOG_DESC(DBG, "USERFS FILE CREATE", "Alloc new inode failed");
        userfs_free_dentry(dentry_table, dentry_pos);
        return NULL;
    }
    strncpy(dentry_table->dentry[dentry_pos].d_name.name, name, name_len + 1);
    dentry_table->dentry[dentry_pos].d_first_dblock = new_inode_bbuf->b_blocknr;
    /*insert new dentry in dentry hash table*/
    hash_obj_t *new_hash_dentry =
        userfs_dentry_hash_insert(name, name_len, USERFS_NAME2INODEBUF, (unsigned long)new_inode_bbuf, dentry_pos, dentry_hashtable);
    if (!new_hash_dentry) {
        LOG_DESC(DBG, "USERFS FILE CREATE", "Insert in dentry hash table failed");
        userfs_free_dentry(dentry_table, dentry_pos);
        userfs_free_used_inode();
        return NULL;
    }
    /*init inode*/
    userfs_inode_t *new_inode     = USERFS_DBLOCK(new_inode_bbuf->b_data)->inode;
    new_inode->i_ctime            = file_create_tp.tv_sec;
    new_inode->i_v2pnode_table[0] = USERFS_INODEADDRINFO_CAL(USERFS_NAME2INODEBUF, new_inode_bbuf);
    atomic_add(&(new_inode->ref_count), 1);
    LOG_DESC(DBG, "USERFS FILE CREATE", "Create file success, file:%s, first block:%u, dblock buf:%p, create time:0x%lx, refcount:%d",
             name, new_inode_bbuf->b_blocknr, new_inode_bbuf, file_create_tp.tv_sec, atomic_get(&(new_inode->ref_count)));
    return new_inode_bbuf;
}

userfs_bbuf_t *userfs_file_open(
    const char           *name,
    const uint32_t        name_len,
    const uint32_t        dblock_shard_size,
    userfs_super_block_t *sb,
    linkhash_t           *dentry_hashtable)
{
    /*file name should less than 27B*/
    if (name_len > USERFS_MAX_FILE_NAME_LEN) {
        LOG_DESC(ERR, "USERFS FILE OPEN", "File name too long, name len:%u, max len:%u",
                 name_len, USERFS_MAX_FILE_NAME_LEN);
        return NULL;
    }
    /*get current time as file latest access time stamp*/
    struct timeval file_open_tp = {0};
    if (gettimeofday(&file_open_tp, NULL) < 0) {
        perror("USERFS FILE OPEN");
        return NULL;
    }

    uint32_t                   dentry_pos = 0;
    userfs_dhtable_inodeaddr_t inodeaddr  = userfs_dentry_hash_get(name, name_len, &dentry_pos, dentry_hashtable);
    if (inodeaddr == UINT32_MAX) {
        LOG_DESC(ERR, "USERFS FILE OPEN", "Cannot find file:%s", name);
        return NULL;
    }
    LOG_DESC(DBG, "USERFS FILE OPEN", "Get dentry name:%s, dentry pos:%u", name, dentry_pos);

    /*if file doesn't been open yet, then dentry hashtable store inode number, otherwise store block buf
    address*/
    userfs_bbuf_t  *ff_bbuf    = NULL;
    uint32_t        ff_block   = 0;
    userfs_inode_t *open_inode = NULL;
    if (USERFS_INODETYPE_GET(inodeaddr) == USERFS_NAME2INODE) {
        /*haven't been opened yet, read inode*/
        ff_block = USERFS_INODEADDR_GET(inodeaddr);
        ff_bbuf  = userfs_get_used_inode(sb, ff_block, dblock_shard_size);
        if (!ff_bbuf) {
            LOG_DESC(DBG, "USERFS FILE OPEN", "Read inode failed, inode number:%u", ff_block);
            return NULL;
        }

        /*update inode number in dentry hash table to open block buf address in memory*/
        hash_obj_t *new_hash_dentry =
            userfs_dentry_hash_update(name, name_len, USERFS_NAME2INODEBUF, (unsigned long)ff_bbuf, dentry_hashtable);
        if (!new_hash_dentry) {
            LOG_DESC(DBG, "USERFS FILE OPEN", "Update dentry hash failed, file:%s, inode number:%u, dblock buf:%p",
                     name, ff_block, ff_bbuf);
            userfs_free_dbbuf(ff_bbuf);
            return NULL;
        }
        open_inode = USERFS_DBLOCK(ff_bbuf->b_data)->inode;
        atomic_add(&(open_inode->ref_count), 1);
        open_inode->i_v2pnode_table[0] = USERFS_INODEADDRINFO_CAL(USERFS_NAME2INODEBUF, ff_bbuf);
        LOG_DESC(DBG, "USERFS FILE OPEN", "Open file success, file:%s, first block:%u, record first block:%lu, dblock buf:%p, access time:0x%lx, refcount:%d",
                 name, ff_bbuf->b_blocknr, open_inode->i_v2pnode_table[0], ff_bbuf, file_open_tp.tv_sec, atomic_get(&(open_inode->ref_count)));
    } else {
        ff_bbuf    = (userfs_bbuf_t *)USERFS_INODEADDR_GET(inodeaddr);
        ff_block   = ff_bbuf->b_blocknr;
        open_inode = USERFS_DBLOCK(ff_bbuf->b_data)->inode;
        atomic_add(&(open_inode->ref_count), 1);
        LOG_DESC(DBG, "USERFS FILE OPEN", "Refering file success, file:%s, first block:%u, dblock buf:%p, access time:0x%lx, refcount:%d",
                 name, ff_bbuf->b_blocknr, ff_bbuf, file_open_tp.tv_sec, atomic_get(&(open_inode->ref_count)));
    }
    open_inode->i_atime = file_open_tp.tv_sec;
    return ff_bbuf;
}

int userfs_file_close(
    const char           *name,
    const uint32_t        name_len,
    const uint32_t        dblock_shard_size,
    userfs_super_block_t *sb,
    linkhash_t           *dentry_hashtable)
{
    /*file name should less than 27B*/
    if (name_len > USERFS_MAX_FILE_NAME_LEN) {
        LOG_DESC(ERR, "USERFS FILE CLOSE", "File name too long, name len:%u, max len:%u",
                 name_len, USERFS_MAX_FILE_NAME_LEN);
        return -1;
    }

    uint32_t                   dentry_pos = 0;
    userfs_dhtable_inodeaddr_t inodeaddr  = userfs_dentry_hash_get(name, name_len, &dentry_pos, dentry_hashtable);
    if (inodeaddr == UINT32_MAX) {
        LOG_DESC(ERR, "USERFS FILE CLOSE", "Cannot find file:%s", name);
        return -1;
    }

    /*if file hasn't been opened yet, then dentry hashtable store inode number,
    otherwise store block buf address.
    If file hasn't been opened yet, then return*/
    if (USERFS_INODETYPE_GET(inodeaddr) == USERFS_NAME2INODE) {
        /*haven't been opened yet, not need to close*/
        LOG_DESC(ERR, "USERFS FILE CLOSE", "File hasn't been open, file:%s, first block:%lu",
                 name, USERFS_INODEADDR_GET(inodeaddr));
        return -1;
    }

    /*get current time as file latest access time stamp*/
    struct timeval file_close_tp = {0};
    if (gettimeofday(&file_close_tp, NULL) < 0) {
        perror("USERFS FILE CLOSE");
        return -1;
    }

    userfs_bbuf_t  *ff_bbuf     = (userfs_bbuf_t *)USERFS_INODEADDR_GET(inodeaddr);
    uint32_t        ff_block    = ff_bbuf->b_blocknr;
    userfs_inode_t *close_inode = USERFS_DBLOCK(ff_bbuf->b_data)->inode;
    LOG_DESC(DBG, "USERFS FILE CLOSE", "inode nr:%u, file blocks:%u", ff_bbuf->b_blocknr, close_inode->i_blocks);
    int res = atomic_sub(&(close_inode->ref_count), 1);
    /*if file still been opened by others, then doesn't close it*/
    if (res > 0) {
        LOG_DESC(DBG, "USERFS FILE CLOSE", "Dereferring file success, file:%s, first block:%u, dblock buf:%p, close time:0x%lx, refcount:%d",
                 name, ff_bbuf->b_blocknr, ff_bbuf, file_close_tp.tv_sec, res);
        return 0;
    }
    /*if file doesn't be opened by others, then close it*/
    /*update dentry hashtable from block buf address to inode number*/
    if (!userfs_dentry_hash_update(name, name_len, USERFS_NAME2INODE, ff_block, dentry_hashtable)) {
        LOG_DESC(ERR, "USERFS FILE CLOSE", "Update dentry hash table failed, file:%s, first block:%u, dblock buf:%p",
                 name, ff_bbuf->b_blocknr, ff_bbuf);
        res = -1;
    }
    /*rewrite data block number to inode block buf*/
    for (int i = 0; i < close_inode->i_blocks; i++) {
        unsigned long cur_db_addr = close_inode->i_v2pnode_table[i];
        /*non-opened data block, skip*/
        if (USERFS_INODETYPE_GET(cur_db_addr) == USERFS_NAME2INODE) {
            LOG_DESC(DBG, "USERFS FILE CLOSE", "Non-opened dblock, dblock nr:%u", (uint32_t)USERFS_INODETYPE_GET(cur_db_addr));
            continue;
        }

        /*opened data block, flush data to disk*/
        userfs_bbuf_t *cur_dbbuf_head = (userfs_bbuf_t *)USERFS_INODEADDR_GET(cur_db_addr);
        LOG_DESC(DBG, "USERFS FILE CLOSE", "Opened dblock, dblock nr:%u, dblock buf:0x%p, list len:%u",
                 cur_dbbuf_head->b_blocknr, cur_dbbuf_head, cur_dbbuf_head->b_list_len);
        if (userfs_dbbuf_list_flush(sb->s_first_datablock, sb->s_data_block_size, cur_dbbuf_head, cur_dbbuf_head->b_list_len) < 0) {
            LOG_DESC(ERR, "USERFS FILE CLOSE", "Opened dblock list flush FAILED, file:%s, dblock nr:%u, dblock buf:0x%p, list len:%u",
                     name, cur_dbbuf_head->b_blocknr, cur_dbbuf_head, cur_dbbuf_head->b_list_len);
        }
    }

    LOG_DESC(DBG, "USERFS FILE CLOSE", "Close file success, file:%s, first block:%u, dblock buf:%p, close time:0x%lx",
             name, ff_bbuf->b_blocknr, ff_bbuf, file_close_tp.tv_sec);
    return res;
}

int userfs_file_delete(
    const char            *name,
    const uint32_t         name_len,
    userfs_super_block_t  *sb,
    userfs_dentry_table_t *dentry_table,
    linkhash_t            *dentry_hashtable)
{
    /*file name should less than 27B*/
    if (name_len > USERFS_MAX_FILE_NAME_LEN) {
        LOG_DESC(ERR, "USERFS FILE DELETE", "File name too long, name len:%u, max len:%u",
                 name_len, USERFS_MAX_FILE_NAME_LEN);
        return -1;
    }

    /*delete and free dentry, considering dentry free strategy, also need to update dentry pos*/
    uint32_t                   dentry_pos = 0;
    userfs_dhtable_inodeaddr_t inodeaddr  = userfs_dentry_hash_remove(name, name_len, &dentry_pos, dentry_hashtable);
    if (inodeaddr == UINT32_MAX) {
        LOG_DESC(ERR, "USERFS FILE DELETE", "Cannot find file:%s", name);
        return -1;
    }
    LOG_DESC(DBG, "USERFS FILE DELETE", "Delete dentry from dentry hashtable, file:%s, pos:%u", name, dentry_pos);

    /*free dentry and update dentry pos for moved dentry*/
    int res = userfs_free_dentry(dentry_table, dentry_pos);
    if (!res) {
        userfs_dentry_t *moved_dentry = &(dentry_table->dentry[dentry_pos]);
        if (!userfs_dentry_hash_update_dentrypos(
                moved_dentry->d_name.name, strlen(moved_dentry->d_name.name), dentry_pos, dentry_hashtable)) {
            LOG_DESC(ERR, "USERFS FILE DELETE", "Update dentry pos for moved dentry failed, file:%s, new pos:%u", moved_dentry->d_name.name, dentry_pos);
            return -1;
        }
        LOG_DESC(DBG, "USERFS FILE DELETE", "Update dentry pos for moved dentry, file:%s, new pos:%u", moved_dentry->d_name.name, dentry_pos);
    }

    /*if file hasn't been opened yet, then dentry hashtable store inode number,
    otherwise store block buf address.
    If file isn't opened , then open inode and delete it*/
    userfs_bbuf_t *inodebbuf = NULL;
    if (USERFS_INODETYPE_GET(inodeaddr) == USERFS_NAME2INODEBUF) {
        /*haven't been opened yet, not need to close*/
        LOG_DESC(DBG, "USERFS FILE DELETE", "File is opened, file:%s, block buf addr:0x%lx",
                 name, USERFS_INODEADDR_GET(inodeaddr));
        inodebbuf = (userfs_bbuf_t *)USERFS_INODEADDR_GET(inodeaddr);
    } else {
        /*read inode from disk to free data blocks*/
        uint32_t inode_nr = (uint32_t)USERFS_INODEADDR_GET(inodeaddr);
        inodebbuf         = userfs_get_used_inode(sb, inode_nr, USERFS_PAGE_SIZE);
        if (!inodebbuf) {
            LOG_DESC(DBG, "USERFS FILE DELETE", "Read inode from disk failed, file:%s, inode number:%u",
                     name, inode_nr);
            return -1;
        }
    }

    /*get current time as file delete time stamp*/
    struct timeval file_delete_ops = {0};
    if (gettimeofday(&file_delete_ops, NULL) < 0) {
        perror("USERFS FILE CLOSE");
        return -1;
    }

    /*free data blocks alloc for file*/
    userfs_inode_t *delete_inode = USERFS_DBLOCK(inodebbuf->b_data)->inode;
    LOG_DESC(DBG, "USERFS FILE DELETE", "File inode info, inode number:%u, file sizes:%u, blocks count:%u, create time:0x%lx, last access time:0x%lx, delete time:0x%lx",
             inodebbuf->b_blocknr, delete_inode->i_size, delete_inode->i_blocks, delete_inode->i_ctime, delete_inode->i_atime, file_delete_ops.tv_sec);
    for (int i = 0; i < delete_inode->i_blocks; i++) {
        userfs_free_used_dblock();
    }
    userfs_bbuf_t *next = inodebbuf;
    userfs_bbuf_t *prev = NULL;
    while (next != NULL) {
        prev = next;
        next = prev->b_this_page;
        userfs_free_dbbuf(prev);
    }

    return 0;
}

int is_fileops_in_range(
    const uint32_t in_db_off,
    const uint32_t dbbuf_size,
    const uint32_t dbbuf_s_off)
{
    return (in_db_off >= dbbuf_s_off && in_db_off < (dbbuf_s_off + dbbuf_size));
}

userfs_bbuf_t *userfs_file_write_off_get(
    const uint32_t           v_db_nr,
    const unsigned long      p_db_addr,
    const uint32_t           in_db_shard_off,
    const uint32_t           dblock_shard_size,
    userfs_inode_t          *inode,
    userfs_bgd_index_list_t *bgd_idx_list,
    userfs_super_block_t    *sb)
{
    userfs_bbuf_t *target_dbbuf = NULL;
    uint32_t       p_db_nr      = 0;

    /*check dblock type*/
    /*current dblock hasn't been allocted yet, alloc*/
    if (!USERFS_INODEADDR_GET(p_db_addr)) {
        target_dbbuf = userfs_get_new_dblock(sb, bgd_idx_list, dblock_shard_size);
        if (!target_dbbuf) {
            LOG_DESC(ERR, "USERFS FILE WOFF", "Alloc new dblock for file failed, physical dblock addr:0x%lx, dblock shard size:0x%x",
                     p_db_addr, dblock_shard_size)
            return NULL;
        }

        p_db_nr                     = target_dbbuf->b_blocknr;
        target_dbbuf->b_block_s_off = in_db_shard_off;
        LOG_DESC(DBG, "USERFS FILE WOFF", "Alloc new dblock for file, dblock nr:%u, dblock shard size:0x%x, in dblock offset:0x%x",
                 target_dbbuf->b_blocknr, target_dbbuf->b_size, target_dbbuf->b_block_s_off);
        inode->i_blocks += 1;
        return target_dbbuf;
    }

    /*dblock has been allocated, but not been read, read it*/
    if (USERFS_INODETYPE_GET(p_db_addr) == USERFS_NAME2INODE) {
        p_db_nr      = USERFS_INODEADDR_GET(p_db_addr);
        target_dbbuf = userfs_get_used_dblock(sb, p_db_nr, in_db_shard_off, dblock_shard_size);
        if (!target_dbbuf) {
            LOG_DESC(ERR, "USERFS FILE WOFF", "Get used dblock of file failed, dblock nr:%u, dblock shard size:0x%x",
                     p_db_nr, dblock_shard_size);
            return NULL;
        }

        LOG_DESC(DBG, "USERFS FILE WOFF", "Get used dblock of file, dblock nr:%u, dblock shard size:0x%x, in dblock offset:0x%x",
                 p_db_nr, dblock_shard_size, target_dbbuf->b_block_s_off);
        inode->i_v2pnode_table[v_db_nr] = (unsigned long)target_dbbuf;
        return target_dbbuf;
    }

    /*otherwise, this block has been open, but may not be the offset want
    in this case, need to query list to decide whether read or directly referring it*/
    target_dbbuf        = (userfs_bbuf_t *)USERFS_INODEADDR_GET(p_db_addr);
    userfs_bbuf_t *head = target_dbbuf;
    for (int i = 0;
         i < head->b_list_len;
         i++, target_dbbuf = target_dbbuf->b_this_page) {
        LOG_DESC(DBG, "USERFS FILE WOFF", "Query dblock buf list, dblock buf:0x%p, dblock nr:%u, in dblock offset:0x%x",
                 target_dbbuf, target_dbbuf->b_blocknr, target_dbbuf->b_block_s_off);
        if (!is_fileops_in_range(in_db_shard_off, target_dbbuf->b_block_s_off, target_dbbuf->b_size)) {
            break;
        }
    }

    /*the offset current write ops want hasn't been read yet, read it*/
    if (!target_dbbuf) {
        target_dbbuf = userfs_get_used_dblock(sb, p_db_nr, in_db_shard_off, dblock_shard_size);
        if (!target_dbbuf) {
            LOG_DESC(ERR, "USERFS FILE WOFF", "Read used dblock of file failed, dblock nr:%u, dblock shard size:0x%x, in dblock offset:0x%x",
                     p_db_nr, dblock_shard_size, in_db_shard_off);
            return NULL;
        }
        head->b_list_len          += 1;
        target_dbbuf->b_this_page  = head->b_this_page;
        head->b_this_page          = target_dbbuf;
    }

    LOG_DESC(DBG, "USERFS FILE WOFF", "Get used dblock of file, list len:%u dblock buf:0x%p, dblock nr:%u, dblock shard size:0x%x, in dblock offset:0x%x",
             head->b_list_len, target_dbbuf, target_dbbuf->b_blocknr, dblock_shard_size, target_dbbuf->b_block_s_off);
    return target_dbbuf;
}

userfs_bbuf_t *userfs_file_read_off_get(
    const uint32_t        v_db_nr,
    const unsigned long   p_db_addr,
    const uint32_t        in_db_shard_off,
    const uint32_t        dblock_shard_size,
    userfs_inode_t       *inode,
    userfs_super_block_t *sb)
{
    userfs_bbuf_t *target_dbbuf = NULL;
    uint32_t       p_db_nr      = 0;

    /*dblock has been allocated, but not been read, read it*/
    if (USERFS_INODETYPE_GET(p_db_addr) == USERFS_NAME2INODE) {
        p_db_nr      = USERFS_INODEADDR_GET(p_db_addr);
        target_dbbuf = userfs_get_used_dblock(sb, p_db_nr, in_db_shard_off, dblock_shard_size);
        if (!target_dbbuf) {
            LOG_DESC(ERR, "USERFS FILE ROFF", "Get used dblock of file failed, dblock nr:%u, dblock shard size:0x%x",
                     p_db_nr, dblock_shard_size);
            return NULL;
        }

        LOG_DESC(DBG, "USERFS FILE ROFF", "Get used dblock of file, dblock nr:%u, dblock shard size:0x%x, in dblock offset:0x%x",
                 p_db_nr, dblock_shard_size, target_dbbuf->b_block_s_off);
        inode->i_v2pnode_table[v_db_nr] = (unsigned long)target_dbbuf;
        return target_dbbuf;
    }

    /*otherwise, this block has been open, but may not be the offset want
    in this case, need to query list to decide whether read or directly referring it*/
    target_dbbuf        = (userfs_bbuf_t *)USERFS_INODEADDR_GET(p_db_addr);
    userfs_bbuf_t *head = target_dbbuf;
    for (int i = 0;
         i < head->b_list_len;
         i++, target_dbbuf = target_dbbuf->b_this_page) {
        LOG_DESC(DBG, "USERFS FILE ROFF", "Query dblock buf list, dblock buf:0x%p, dblock nr:%u, in dblock offset:0x%x",
                 target_dbbuf, target_dbbuf->b_blocknr, target_dbbuf->b_block_s_off);
        if (!is_fileops_in_range(in_db_shard_off, target_dbbuf->b_block_s_off, target_dbbuf->b_size)) {
            break;
        }
    }

    /*the offset current write ops want hasn't been read yet, read it*/
    if (!target_dbbuf) {
        target_dbbuf = userfs_get_used_dblock(sb, p_db_nr, in_db_shard_off, dblock_shard_size);
        if (!target_dbbuf) {
            LOG_DESC(ERR, "USERFS FILE ROFF", "Read used dblock of file failed, dblock nr:%u, dblock shard size:0x%x, in dblock offset:0x%x",
                     p_db_nr, dblock_shard_size, in_db_shard_off);
            return NULL;
        }
        head->b_list_len          += 1;
        target_dbbuf->b_this_page  = head->b_this_page;
        head->b_this_page          = target_dbbuf;
    }

    LOG_DESC(DBG, "USERFS FILE ROFF", "Get used dblock of file, list len:%u dblock buf:0x%p, dblock nr:%u, dblock shard size:0x%x, in dblock offset:0x%x",
             head->b_list_len, target_dbbuf, target_dbbuf->b_blocknr, dblock_shard_size, target_dbbuf->b_block_s_off);
    return target_dbbuf;
}

uint32_t userfs_file_write(
    const char              *buf,
    const uint32_t           woff,
    const uint32_t           size,
    const uint32_t           dblock_shard_size,
    userfs_bbuf_t           *inodebbuf,
    userfs_super_block_t    *sb,
    userfs_bgd_index_list_t *bgd_idx_list)
{
    /*reserve space for inode*/
    if (woff >= (UINT32_MAX - USERFS_INODE_SIZE)) {
        LOG_DESC(ERR, "USERFS FILE WRITE", "No space to write, woff:0x%x", woff);
        return 0;
    }

    uint32_t real_woff       = woff + USERFS_INODE_SIZE;
    uint32_t real_size       = size;

    uint32_t v_db_nr         = real_woff / sb->s_data_block_size;
    uint32_t in_db_off       = real_woff % sb->s_data_block_size;
    uint32_t in_shard_off    = real_woff % dblock_shard_size;
    uint32_t in_db_shard_off = in_db_off - in_shard_off;
    /*write offset and write size must keep in one data block shard,
    otherwise must cut into series of write ops*/
    if ((in_shard_off + size) > dblock_shard_size) {
        real_size = dblock_shard_size - in_db_off;
        LOG_DESC(WAR, "USERFS FILE WRITE", "Write size must align to data block shard size, in shard offset:0x%x, expect wlen:0x%x, real wlen:0x%x, dblock shard size:0x%x",
                 in_shard_off, size, real_size, dblock_shard_size);
    }

    /*get current time as file modify time stamp*/
    struct timeval file_modity_ts = {0};
    if (gettimeofday(&file_modity_ts, NULL) < 0) {
        perror("USERFS FILE WRITE");
        return 0;
    }

    userfs_inode_t *write_inode = USERFS_DBLOCK(inodebbuf->b_data)->inode;
    unsigned long   p_db_addr   = write_inode->i_v2pnode_table[v_db_nr];
    uint32_t        p_db_nr     = 0;
    LOG_DESC(DBG, "USERFS FILE WRITE", "inode nr:%u, logic dblock nr:%u, in dblock offset:0x%x, in dblock shard offset:0x%x, physical dblock addr:0x%lx",
             inodebbuf->b_blocknr, v_db_nr, in_db_off, in_db_shard_off, p_db_addr);

    /*get dblock buf from target data block with target offset*/
    userfs_bbuf_t *target_dbbuf = userfs_file_write_off_get(v_db_nr, p_db_addr, in_db_shard_off,
                                                            dblock_shard_size, write_inode, bgd_idx_list, sb);
    if (target_dbbuf == NULL) {
        LOG_DESC(ERR, "USERFS FILE WRITE", "Read target dblock buf failed");
        return 0;
    }

    /*write to target data block buf*/
    char *target_buf = USERFS_DBLOCK(target_dbbuf->b_data)->data;

    memcpy(target_buf + in_shard_off, buf, real_size);
    /*update inode v2pdblock table, file dblocks count and file size(if needed)*/
    if (USERFS_INODETYPE_GET(p_db_addr) == USERFS_NAME2INODE) {
        write_inode->i_v2pnode_table[v_db_nr] = (unsigned long)target_dbbuf;
    }
    write_inode->i_size  = write_inode->i_size < (real_woff + real_size - USERFS_INODE_SIZE)
                               ? (real_woff + real_size - USERFS_INODE_SIZE)
                               : write_inode->i_size;

    write_inode->i_mtime = file_modity_ts.tv_sec;
    LOG_DESC(DBG, "USERFS FILE WRITE", "Write to file success, write dblock nr:%u, in dblock shard off:0x%x,\
in shard off:0x%x, shard size:0x%x, real woff:0x%x, real size:%u, file blocks:%u, file size:%u",
             target_dbbuf->b_blocknr, in_db_shard_off, in_shard_off, dblock_shard_size, real_woff, real_size,
             write_inode->i_blocks, write_inode->i_size);

    return real_size;
}

uint32_t userfs_file_read(
    char                 *buf,
    const uint32_t        roff,
    const uint32_t        size,
    const uint32_t        dblock_shard_size,
    userfs_bbuf_t        *inodebbuf,
    userfs_super_block_t *sb)
{
    /*reserve space for inode*/
    if (roff >= (UINT32_MAX - USERFS_INODE_SIZE)) {
        LOG_DESC(ERR, "USERFS FILE READ", "inode reserved space, read off:0x%x", roff);
        return 0;
    }

    uint32_t real_roff       = roff + USERFS_INODE_SIZE;
    uint32_t real_size       = size;
    uint32_t v_db_nr         = real_roff / sb->s_data_block_size;
    uint32_t in_db_off       = real_roff % sb->s_data_block_size;
    uint32_t in_shard_off    = real_roff % dblock_shard_size;
    uint32_t in_db_shard_off = in_db_off - in_shard_off;
    /*read offset and read size must align to data block shard size,
    otherwise must cut into series of write ops*/
    if ((in_shard_off + size) > dblock_shard_size) {
        real_size = dblock_shard_size - in_db_off;
        LOG_DESC(WAR, "USERFS FILE READ", "Read size must align to data block shard size, in shard offset:0x%x, expect rsize:0x%x, real rsize:0x%x, dblock shard size:0x%x",
                 in_shard_off, size, real_size, dblock_shard_size);
    }

    userfs_inode_t *read_inode = USERFS_DBLOCK(inodebbuf->b_data)->inode;
    unsigned long   p_db_addr  = read_inode->i_v2pnode_table[v_db_nr];
    uint32_t        p_db_nr    = 0;
    LOG_DESC(DBG, "USERFS FILE READ", "inode nr:%u, logic dblock nr:%u, in dblock offset:0x%x, in dblock shard offset:0x%x, physical dblock addr:0x%lx",
             inodebbuf->b_blocknr, v_db_nr, in_db_off, in_db_shard_off, p_db_addr);

    /*read offset and size also must less than file size*/
    if (read_inode->i_size < (real_roff + real_size - USERFS_INODE_SIZE)) {
        LOG_DESC(ERR, "USERFS FILE READ", "Read offset reach out file,file size:0x%x, read start offset:0x%x, read end offset:0x%x",
                 read_inode->i_size, real_roff - USERFS_INODE_SIZE, real_roff + real_size - USERFS_INODE_SIZE);
        return 0;
    }
    /*if read file hole, directly return all-zero buf*/
    if (!USERFS_INODEADDR_GET(p_db_addr)) {
        LOG_DESC(DBG, "USERFS FILE READ", "Read file hole, read start offset:0x%x, read end offset:0x%x",
                 real_roff - USERFS_INODE_SIZE, real_roff + real_size - USERFS_INODE_SIZE);
        memset(buf, USERFS_FILE_HOLE_DATA, real_size);
        return real_size;
    }

    /*get current time as file access time stamp*/
    struct timeval file_access_ts = {0};
    if (gettimeofday(&file_access_ts, NULL) < 0) {
        perror("USERFS FILE READ");
        return 0;
    }

    /*get dblock buf from target data block with target offset*/
    userfs_bbuf_t *target_dbbuf = userfs_file_read_off_get(v_db_nr, p_db_addr, in_db_shard_off,
                                                           dblock_shard_size, read_inode, sb);
    if (target_dbbuf == NULL) {
        LOG_DESC(ERR, "USERFS FILE READ", "Read target dblock buf failed");
        return 0;
    }

    /*write to target data block buf*/
    char *target_buf = USERFS_DBLOCK(target_dbbuf->b_data)->data;
    memcpy(buf, target_buf + in_shard_off, real_size);
    /*update inode v2pdblock table, file dblocks count and file size(if needed)*/
    if (USERFS_INODETYPE_GET(p_db_addr) == USERFS_NAME2INODE) {
        read_inode->i_v2pnode_table[v_db_nr] = (unsigned long)target_dbbuf;
    }

    read_inode->i_atime = file_access_ts.tv_sec;
    LOG_DESC(DBG, "USERFS FILE READ", "Write to file success, write dblock nr:%u, in dblock shard off:0x%x,\
in shard off:0x%x, shard size:0x%x, real roff:0x%x, real size:0x%x, file blocks:%u, file size:%u",
             target_dbbuf->b_blocknr, in_db_shard_off, in_shard_off, dblock_shard_size, real_roff, real_size,
             read_inode->i_blocks, read_inode->i_size);

    return real_size;
}