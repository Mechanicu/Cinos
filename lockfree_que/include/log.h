#ifndef _LOG_H
#define _LOG_H
#include <stdio.h>
#include <unistd.h>

#define LOG_LEVEL_ERROR   0
#define LOG_LEVEL_WARNING 1
#define LOG_LEVEL_DEBUG   2
#define LOG_LEVEL_INFO    3
#define ERR               LOG_LEVEL_ERROR
#define WAR               LOG_LEVEL_WARNING
#define DBG               LOG_LEVEL_DEBUG
#define INF               LOG_LEVEL_INFO

#define LOG_LEVEL         (LOG_LEVEL_DEBUG + 1)

#define LOG(level, fmt, ...)                                                         \
    if (LOG_LEVEL > level) {                                                         \
        printf("[%s:%d:%u] " fmt "\n", __FILE__, __LINE__, getpid(), ##__VA_ARGS__); \
    }

#define _LOG_DESC(level, desc, fmt, ...)                                                      \
    if (LOG_LEVEL > level) {                                                                  \
        printf("[%s:%d:%u] " desc ":" fmt "\n", __FILE__, __LINE__, getpid(), ##__VA_ARGS__); \
    }
#define LOG_DESC(level, desc, fmt, ...) _LOG_DESC(level, desc, fmt, ##__VA_ARGS__)

#if LOG_LEVEL > LOG_LEVEL_ERROR
#define LOG_ERROR(fmt, ...) printf("[%s:%d:%u] " fmt "\n", __FILE__, __LINE__, getpid(), ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...)
#endif

#if LOG_LEVEL > LOG_LEVEL_WARNING
#define LOG_WARING(fmt, ...) printf("[%s:%d:%u] " fmt "\n", __FILE__, __LINE__, getpid(), ##__VA_ARGS__)
#else
#define LOG_WARING(fmt, ...)
#endif

#if LOG_LEVEL > LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...) printf("[%s:%d:%u] " fmt "\n", __FILE__, __LINE__, getpid(), ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

#if LOG_LEVEL > LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) printf("[%s:%d:%u] " fmt "\n", __FILE__, __LINE__, getpid(), ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#endif