#include "log.h"
#include "node_kvmap.h"

int main(int argc, char *argv[])
{
    dsdb_hash_map hmap;
    dsdb_kobj_t   key;
    dsdb_vobj_t   val;
    dsdb_vobj_t   upval;
    dsdb_vobj_t   upval1;
    key.key    = "name";
    val.val    = "insert";
    upval.val  = "update";
    upval1.val = "update1";

    hmap.dsdb_map_insert(key, val, DSDB_HASH_INSERT);
    hmap.dsdb_map_insert(key, val, DSDB_HASH_INSERT);
    hmap.dsdb_map_insert(key, upval, DSDB_HASH_UPDATE);
    hmap.dsdb_map_insert(key, upval1, DSDB_HASH_UPDATE);

    dsdb_vobj_t *search = hmap.dsdb_map_search(key);
    LOG_DEBUG("find object, key:%s, val:%s", key.key.c_str(), search->val.c_str());
    hmap.dsdb_map_remove(key);
    search = hmap.dsdb_map_search(key);
    if (search != NULL) {
        LOG_DEBUG("find object, key:%s, val:%s", key.key.c_str(), search->val.c_str());
    }

    return 0;
}