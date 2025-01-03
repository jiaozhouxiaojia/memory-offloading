/*
 * Copyright (C) 2025, LiXiang. All rights reserved
 * Author: JiaHao
 * Create: Thu Jan 02 09:58:20 2025
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

// Define the log level enumeration type
typedef enum {
    LOG_LEVEL_DEBUG,  // Debug information
    LOG_LEVEL_INFO,   // General information
    LOG_LEVEL_WARN,   // Warning information
    LOG_LEVEL_ERROR   // Error information
} LogLevel;

// Define global variables to control the log level and whether logging is enabled
extern LogLevel g_log_level;
extern int g_log_enabled;

// Log function declaration
void log_message(LogLevel level, const char* file, int line, const char* func, const char* format, ...);

// Simplified macros for convenient use of the log function
#define LOG_DEBUG(fmt, ...) \
    log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    log_message(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
