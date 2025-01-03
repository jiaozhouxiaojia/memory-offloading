/*
 * Copyright (C) 2025, LiXiang. All rights reserved
 * Author: JiaHao
 * Create: Fri Jan 10 10:49:43 2025
 */
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "logger.h"

// Define global variables, initial value is INFO level, logging enabled
static enum log_level log_level = LOG_LEVEL_INFO;
static int g_log_enabled = 1;

enum log_level get_log_level(void)
{
	return log_level;
}

void set_log_level(enum log_level level)
{
	log_level = level;
}

void enable_log_level(void)
{
	g_log_enabled = 1;
}

// Get the string representation of the current time
static inline const char* get_current_time_str(void)
{
	static char buffer[20];

	time_t now = time(NULL);
	struct tm* tm_info = localtime(&now);
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
	return buffer;
}

// Convert log level to string
static inline const char* log_level_to_str(enum log_level level)
{
	switch (level) {
	case LOG_LEVEL_DEBUG:
		return "DEBUG";
	case LOG_LEVEL_INFO:
		return "INFO ";
	case LOG_LEVEL_WARN:
		return "WARN ";
	case LOG_LEVEL_ERROR:
		return "ERROR";
	default:
		return "UNKNOWN";
	}
}

// Log function implementation
void log_message(enum log_level level, const char* file, int line, const char* func, const char* format, ...)
{
	if (!g_log_enabled || level < get_log_level()) {
		return;
	}

	// Print log time, level, file, line number, function
	fprintf(stdout, "[%s] [%s] [%s:%d:%s] ", get_current_time_str(),
		log_level_to_str(level), file, line, func);

	// 打印日志内容
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);

//	fprintf(stdout, "\n");
}
