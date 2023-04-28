#ifndef _LOG_H
#define _LOG_H
#include <stdio.h>

#define LOG_LEVEL_ERROR   0
#define LOG_LEVEL_WARNING 1
#define LOG_LEVEL_DEBUG   2
#define LOG_LEVEL_INFO    3

#define LOG_LEVEL         (LOG_LEVEL_DEBUG + 1)

#if LOG_LEVEL > LOG_LEVEL_ERROR
#define LOG_ERROR(fmt, ...) printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...)
#endif

#if LOG_LEVEL > LOG_LEVEL_WARNING
#define LOG_WARING(fmt, ...) printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_WARING(fmt, ...)
#endif

#if LOG_LEVEL > LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...) printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

#if LOG_LEVEL > LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#endif