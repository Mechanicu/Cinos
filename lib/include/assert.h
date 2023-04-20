#ifndef _ASSERT_H_
#define _ASSERT_H_
#include "log.h"

#define ALLOC_ASSERT(ptr)                       \
    if (ptr == NULL) {                          \
        LOG_ERROR("Failed to allocate memory"); \
        return;                                 \
    }

#endif