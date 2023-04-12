#include "mem_pool.h"
#include "log.h"

// for service control
enum service_ctrl
{
    REGISTER_SERVICE,
    REQUEST_SERVICE,
    UNREGISTER_SERVICE,
};

typedef struct service_info {

};

typedef struct service_identify {
    unsigned long server_id;
    unsigned long service_id;
} srv_id_t;

// for server

