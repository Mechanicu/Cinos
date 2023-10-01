#include "node_kvmap.h"
#include "log.h"

/*
type:
DSDB_HASH_INSERT
DSDB_HASH_UPDATE
DSDB_HASH_ANY
*/
bool dsdb_hash_map::dsdb_map_insert(
    const dsdb_kobj_t     &key,
    const dsdb_vobj_t     &val,
    const dsdb_insert_type type)
{
    if (!&key || !&val) {
        LOG_ERROR("DSDB MAP INSERT, invalid args");
        return 0;
    }
    std::map<dsdb_kobj_t, dsdb_vobj_t>::iterator
        iter = this->dsdb_map.find(key);
    switch (type) {
        case DSDB_HASH_INSERT:
            if (iter != this->dsdb_map.end()) {
                LOG_DEBUG("DSDB MAP INSERT, key already exists, key:%s", key.key.c_str());
                return false;
            }
            this->dsdb_map.insert(std::make_pair(key, val));
            LOG_DEBUG("DSDB MAP INSERT, new key:%s, new val:%s, size:%lu",
                      key.key.c_str(), val.val.c_str(), this->dsdb_map.size());
            break;
        case DSDB_HASH_UPDATE:
            if (iter == this->dsdb_map.end()) {
                LOG_DEBUG("DSDB MAP INSERT, key not exists, key:%s", key.key.c_str());
                return false;
            }
            iter->second = val;
            LOG_DEBUG("DSDB MAP INSERT, update key:%s, update val:%s, size:%lu",
                      key.key.c_str(), val.val.c_str(), this->dsdb_map.size());
            break;
        case DSDB_HASH_ANY:
            if (iter != this->dsdb_map.end()) {
                iter->second = val;
                LOG_DEBUG("DSDB MAP INSERT, update key:%s, update val:%s, size:%lu",
                          key.key.c_str(), val.val.c_str(), this->dsdb_map.size());
            } else {
                this->dsdb_map.insert(std::make_pair(key, val));
                LOG_DEBUG("DSDB MAP INSERT, new key:%s, new val:%s, size:%lu",
                          key.key.c_str(), val.val.c_str(), this->dsdb_map.size());
            }
        default:
            LOG_DEBUG("DSDB MAP INSERT, unknown type:%d", type);
            break;
    }
    return true;
}

bool dsdb_hash_map::dsdb_map_remove(
    const dsdb_kobj_t &key)
{
    return this->dsdb_map.erase(key);
}

dsdb_vobj_t *dsdb_hash_map::dsdb_map_search(
    const dsdb_kobj_t &key)
{
    std::map<dsdb_kobj_t, dsdb_vobj_t>::iterator
        iter = this->dsdb_map.find(key);
    if (iter == this->dsdb_map.end()) {
        LOG_DEBUG("DSDB MAP SEARCH, cannot find data, key:%s", key.key.c_str());
        return NULL;
    }
    return &(iter->second);
}