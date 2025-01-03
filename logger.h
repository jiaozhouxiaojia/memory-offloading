/*
 * Copyright (C) 2025, LiXiang. All rights reserved
 * Author: JiaHao
 * Create: Thu Jan 02 09:58:20 2025
 */
#ifndef LOGGER_H
#define LOGGER_H

// Define the log level enumeration type
enum log_level {
	LOG_LEVEL_DEBUG,  // Debug information
	LOG_LEVEL_INFO,   // General information
	LOG_LEVEL_WARN,   // Warning information
	LOG_LEVEL_ERROR   // Error information
};

void enable_log_level(void);
enum log_level get_log_level(void);
void set_log_level(enum log_level level);

// Log function declaration
void log_message(enum log_level level, const char* file, int line, const char* func, const char* format, ...);

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
