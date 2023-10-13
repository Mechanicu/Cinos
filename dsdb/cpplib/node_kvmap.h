#ifndef NODE_KVMAP_H
#define NODE_KVMAP_H
#include <map>
#include <string>

typedef struct dsdb_val_obj {
    int         data_type;
    int         actions;
    std::string val;

    bool operator==(const struct dsdb_val_obj &_val) const
    {
        return this->val == _val.val;
    }
} dsdb_vobj_t;

typedef struct dsdb_key_obj {
    std::string key;
    bool        operator<(const struct dsdb_key_obj &_key) const
    {
        return this->key < _key.key;
    }
} dsdb_kobj_t;

typedef enum {
    DSDB_HASH_INSERT,
    DSDB_HASH_UPDATE,
    DSDB_HASH_ANY,
} dsdb_insert_type;

class dsdb_hash_map
{
public:
    struct dsdb_hash_func {
        size_t operator()(const dsdb_kobj_t &_key) const
        {
            std::hash<std::string> hash_fn;
            return hash_fn(_key.key);
        }
    };
    dsdb_hash_map()
        : dsdb_map(){};
    ~dsdb_hash_map(){};

    bool dsdb_map_insert(
        const dsdb_kobj_t     &key,
        const dsdb_vobj_t     &val,
        const dsdb_insert_type type);
    bool dsdb_map_remove(
        const dsdb_kobj_t &key);
    dsdb_vobj_t *dsdb_map_search(
        const dsdb_kobj_t &key);
private:
    std::map<dsdb_kobj_t, dsdb_vobj_t>
        dsdb_map;
};
#endif