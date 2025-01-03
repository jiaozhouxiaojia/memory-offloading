/*
 * Copyright (C) 2022, Bytedance. All rights reserved
 * Author: JiaHao
 * Create: Thu Jan 02 09:58:20 2025
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

// 定义日志级别枚举类型
typedef enum {
    LOG_LEVEL_DEBUG,  // 调试信息
    LOG_LEVEL_INFO,   // 普通信息
    LOG_LEVEL_WARN,   // 警告信息
    LOG_LEVEL_ERROR   // 错误信息
} LogLevel;

// 定义全局变量，控制日志级别和是否启用日志
extern LogLevel g_log_level;
extern int g_log_enabled;

// 日志函数声明
void log_message(LogLevel level, const char* file, int line, const char* func, const char* format, ...);

// 简化的宏，方便调用日志函数
#define LOG_DEBUG(fmt, ...) \
    log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    log_message(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
