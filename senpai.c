/*
 * Copyright (C) 2022, Bytedance. All rights reserved
 * Author: JiaHao
 * Create: Thu Dec 26 17:32:34 2024
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include "logger.h"

#define CONFIG_FILE_DEFAULT "senpai.conf"

#define FULL_PATH_SIZE  512
#define RECLAMIN_ONCE_MIN  (1 << 20)		 // 1M
#define PAGE_ALIGN_MASK  (~4095)		// 4k page size align


// 定义配置参数结构体
typedef struct {
	char *cgroup_path;
	double psi_threshold;       // PSI 阈值
	double reclaim_ratio;     // 回收比例，0 到 1 之间
	long min_size;            // 最小内存限制，字节
	long max_size;            // 最大内存限制，字节
	long interval;            // 采样间隔，秒
	long pressure_limit;      // 目标压力，微秒
} Config;

static char fullpath[FULL_PATH_SIZE] = {0};
// 全局变量，用于信号处理
volatile sig_atomic_t stop = 0;

// 信号处理函数
void handle_sigint(int sig)
{
	stop = 1;
}

// 定义全局变量，初始值为 INFO 级别，日志启用
LogLevel g_log_level = LOG_LEVEL_DEBUG;
int g_log_enabled = 1;

// 获取当前时间的字符串表示
static const char* get_current_time_str(void)
{
	static char buffer[20];

	time_t now = time(NULL);
	struct tm* tm_info = localtime(&now);
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
	return buffer;
}

// 将日志级别转换为字符串
static const char* log_level_to_str(LogLevel level)
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

// 日志函数实现
void log_message(LogLevel level, const char* file, int line, const char* func, const char* format, ...)
{
	if (!g_log_enabled || level < g_log_level) {
		return;
	}

	// 打印日志时间、级别、文件、行号、函数
	fprintf(stdout, "[%s] [%s] [%s:%d:%s] ", get_current_time_str(), log_level_to_str(level), file, line, func);

	// 打印日志内容
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);

//	fprintf(stdout, "\n");
}

// 去除字符串首尾的空白字符
char *trim_whitespace(char *str)
{
	char *end;

	// 去除字符串前面的空白字符
	while (isspace((unsigned char)*str)) str++;

	if (*str == 0) // 全部都是空白字符
		return str;

	// 去除字符串后面的空白字符
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) end--;

	// 设置新的字符串结尾
	end[1] = '\0';

	return str;
}

// 读取配置文件
int read_config(const char *filename, Config *config)
{
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		LOG_ERROR("Unable to open the configuration file\n");
		return -1;
	}

	char line[256];
	while (fgets(line, sizeof(line), fp)) {
		char *trimmed_line = trim_whitespace(line);

		// 跳过空行和注释
		if (trimmed_line[0] == '\0' || trimmed_line[0] == '#')
			continue;

		char key[128], value[128];
		if (sscanf(trimmed_line, "%127[^=]=%127s", key, value) == 2) {
			char *trimmed_key = trim_whitespace(key);
			char *trimmed_value = trim_whitespace(value);

			if (strcmp(trimmed_key, "CGROUP") == 0) {
				config->cgroup_path = strdup(trimmed_value);
			} else if (strcmp(trimmed_key, "PSI_THRESHOLD") == 0) {
				config->psi_threshold = atof(trimmed_value);
			} else if (strcmp(trimmed_key, "RECLAIM_RATIO") == 0) {
				config->reclaim_ratio = atof(trimmed_value);
			} else if (strcmp(trimmed_key, "MIN_SIZE") == 0) {
				config->min_size = atol(trimmed_value);
			} else if (strcmp(trimmed_key, "MAX_SIZE") == 0) {
				config->max_size = atol(trimmed_value);
			} else if (strcmp(trimmed_key, "INTERVAL") == 0) {
				config->interval = atol(trimmed_value);
			} else if (strcmp(trimmed_key, "PRESSURE") == 0) {
				config->pressure_limit = atol(trimmed_value);
			} else {
				LOG_DEBUG("未知的配置项: %s\n", trimmed_key);
			}
		} else {
			LOG_DEBUG("配置文件格式错误: %s\n", trimmed_line);
		}
	}

	fclose(fp);
	return 0;
}

// 读取 cgroup 下的文件内容，返回字符串
char *read_cgroup_file(const char *path, const char *filename)
{
	memset(fullpath, 0, FULL_PATH_SIZE * sizeof(char));
	snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);

	FILE *fp = fopen(fullpath, "r");
	if (!fp) {
		LOG_ERROR("Unable to open cgroup file %s\n", fullpath);
		return NULL;
	}

	char *buffer = NULL;
	size_t size = 0;
	ssize_t len = getline(&buffer, &size, fp);
	if (len == -1) {
		LOG_ERROR("Failed to read the cgroup file\n");
		fclose(fp);
		return NULL;
	}

	fclose(fp);
	return buffer;
}

// 写入字符串到 cgroup 文件
int write_cgroup_file(const char *path, const char *filename,
		      const char *content)
{
	int ret;

	memset(fullpath, 0, FULL_PATH_SIZE * sizeof(char));
	snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);

	FILE *fp = fopen(fullpath, "w");
	if (!fp) {
		LOG_ERROR("Unable to open the cgroup file for writing\n");
		return -1;
	}

	ret = fputs(content, fp);
//	LOG_ERROR("ret %d ferror(fp) %d\n", ret, ferror(fp));
	if (ret == EOF) {
		LOG_ERROR("Failed to write the cgroup file\n");
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}

// 获取当前内存使用量
long get_current_memory(const char *cgroup_path)
{
	char *buffer = read_cgroup_file(cgroup_path, "memory.current");
	if (!buffer) {
		return -1;
	}

	long current_memory = atol(buffer);
	free(buffer);
	return current_memory;
}

// Get PSI some information
float get_psi_some(const char *cgroup_path, long interval)
{
	char line[256];
	float some_total = -1.0;

	memset(fullpath, 0, FULL_PATH_SIZE * sizeof(char));
	snprintf(fullpath, sizeof(fullpath), "%s/%s", cgroup_path,
		 "memory.pressure");

	//	LOG_DEBUG("fullpath %s\n", fullpath);
	FILE *fp = fopen(fullpath, "r");
	if (!fp) {
		LOG_ERROR("Unable to open PSI file\n");
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		char *ptr = strstr(line, "some avg");
		if (ptr) {
			float avg10, avg60, avg300, total;
			if (sscanf(line, "some avg10=%f avg60=%f avg300=%f total=%f", &avg10, &avg60,
				   &avg300, &total) == 4) {
				some_total = avg10;
				LOG_DEBUG("some avg10=%f avg60=%f avg300=%f total=%f\n", avg10, avg60, avg300,
				       total);
				break;
			}
		}
	}

	fclose(fp);
	return some_total;
}

// Adjust memory
int adjust_memory(const char *cgroup_path, long reclaim_mem)
{
	char value[64] = {0};
	long ret;

	snprintf(value, sizeof(value), "%ld", reclaim_mem);

	ret = write_cgroup_file(cgroup_path, "memory.reclaim", value);

//	LOG_DEBUG("real reclaim size %ld ret %ld\n", reclaim_mem, ret);

	return ret;
}

#define EPSILON 0.00001

float compare_floats(float a, float b)
{
	if (fabs(a - b) < EPSILON) {
		return 0.0; // a and b are "equal"
	} else if (a < b) {
		return b; // a is less than b
	} else {
		return a; // a is greater than b
	}
}

unsigned long psiPolicyFunc(float psi_some, double reclaim_ratio,
			    long current_mem, float psi_threshold)
{
	unsigned long ret;
	double result;

	result = compare_floats(0.0, 1.0 - (psi_some / psi_threshold)) * reclaim_ratio * current_mem;

	LOG_DEBUG("psi %.2f psi_threshold %.2f reclaim_ratio %.2f current_mem %ld result %.2f\n",
	       psi_some, psi_threshold, reclaim_ratio, current_mem, result);

	return (unsigned long)result;
}

// 主函数
int main(int argc, char *argv[])
{
	Config config;
	memset(&config, 0, sizeof(Config));

	const char *config_file = CONFIG_FILE_DEFAULT;
	long reclaim_mem = 0;
	long current_mem = 0;
	float psi_some;

	// Parse command line arguments
	int opt;
	while ((opt = getopt(argc, argv, "c:")) != -1) {
		switch (opt) {
		case 'c':
			config_file = optarg;
			break;
		default:
			LOG_ERROR("用法: %s [-c 配置文件]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	// 读取配置文件
	if (read_config(config_file, &config) == -1) {
		exit(EXIT_FAILURE);
	}

	// 检查必需的配置项
	if (!config.cgroup_path) {
		LOG_ERROR( "配置文件中缺少 CGROUP 项\n");
		exit(EXIT_FAILURE);
	}

	// 设置信号处理
	signal(SIGINT, handle_sigint);
	signal(SIGTERM, handle_sigint);

	// 主循环
	while (!stop) {
		LOG_DEBUG("tick config.interval %ld\n", config.interval);
		current_mem = get_current_memory(config.cgroup_path);
		if (current_mem < 0) {
			LOG_ERROR("Failed to obtain the current memory usage\n");
			break;
		}

		if (current_mem < config.min_size)
			goto SLEEP;

		psi_some = get_psi_some(config.cgroup_path, config.interval);
		if (psi_some < 0.0) {
			LOG_ERROR("Failed to get PSI information.\n");
			break;
		}

		// Calculate the amount of memory to be reclaimed
		if (psi_some < config.psi_threshold) {
			reclaim_mem = (long)psiPolicyFunc(psi_some, config.reclaim_ratio, current_mem,
							  config.psi_threshold);
			reclaim_mem &= PAGE_ALIGN_MASK;
			LOG_DEBUG("reclaim_mem %ld config.min_size %ld\n", reclaim_mem, config.min_size);

/*			long new_mem = current_mem - reclaim_mem;
			if (new_mem < config.min_size) {
				reclaim_mem = 0;
			}
*/
			if (reclaim_mem < RECLAMIN_ONCE_MIN) {
				reclaim_mem = 0;
			}

			// Adjust memory
			if (reclaim_mem) {
				if (adjust_memory(config.cgroup_path, reclaim_mem) < 0) {
					LOG_ERROR("Failed to adjust memory limits\n");
				}
			}

			LOG_DEBUG("reclaim %ld kB  PSI some: %.2f current_mem %ld\n\n\n",
			        reclaim_mem >> 10 , psi_some, current_mem);
		} else {
			reclaim_mem = 0;
			LOG_DEBUG("stop %ld kB  PSI some: %.2f current_mem %ld\n\n\n",
			        reclaim_mem >> 10 , psi_some, current_mem);
		}

SLEEP:
		sleep(config.interval);
	}

	LOG_DEBUG("exit\n");
	return 0;
}
