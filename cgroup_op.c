/*
 * Copyright (C) 2025, LiXiang. All rights reserved
 * Author: JiaHao
 * Create: Tue Jan 07 11:22:30 2025
 */
#include "logger.h"
#include "cgroup_op.h"

// Write a string to a cgroup file
int write_cgroup_file(const char *path, const char *filename,
		      const char *buf)
{
	memset(fullpath, 0, FULL_PATH_SIZE * sizeof(char));
	snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);

	FILE *fp = fopen(fullpath, "w");
	if (!fp) {
		LOG_ERROR("Unable to open the cgroup file for writing\n");
		return -1;
	}

	if (fputs(buf, fp) == EOF) {
		LOG_ERROR("Failed to write the cgroup file\n");
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}

// Read the content of a file under cgroup, return a string
static char *read_cgroup_file(const char *path, const char *filename)
{
	char *buffer = NULL;
	size_t size = 0;

	memset(fullpath, 0, FULL_PATH_SIZE * sizeof(char));
	snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);

	FILE *fp = fopen(fullpath, "r");
	if (!fp) {
		LOG_ERROR("Unable to open cgroup file %s\n", fullpath);
		return NULL;
	}

	ssize_t len = getline(&buffer, &size, fp);
	if (len == -1) {
		LOG_ERROR("Failed to read the cgroup file\n");
		fclose(fp);
		return NULL;
	}

	fclose(fp);
	return buffer;
}

// Get current memory usage
long get_current_memory(const char *cgroup_path)
{
	char *buffer = read_cgroup_file(cgroup_path, "memory.current");
	if (!buffer) {
		LOG_ERROR("Read_cgroup_filememory.current failed \n");
		return -1;
	}

	long current_memory = atol(buffer);
	free(buffer);
	return current_memory;
}

// Get PSI some information
float get_psi_some(const char *cgroup_path, long interval)
{
	float some_total = -1.0;

	memset (read_buffer, 0,  READ_BUFFER_SIZE * sizeof(char));
	memset(fullpath, 0, FULL_PATH_SIZE * sizeof(char));
	snprintf(fullpath, sizeof(fullpath), "%s/%s", cgroup_path,
		 "memory.pressure");

	FILE *fp = fopen(fullpath, "r");
	if (!fp) {
		LOG_ERROR("Unable to open PSI file\n");
		return -1;
	}

	while (fgets(read_buffer, sizeof(read_buffer), fp)) {
		char *ptr = strstr(read_buffer, "some avg");
		if (ptr) {
			float avg10, avg60, avg300, total;
			if (sscanf(read_buffer, "some avg10=%f avg60=%f avg300=%f total=%f",
				   &avg10, &avg60, &avg300, &total) == 4) {
				some_total = avg10;
				LOG_DEBUG("some avg10=%f avg60=%f avg300=%f total=%f\n",
					   avg10, avg60, avg300, total);
				break;
			}
		}
	}

	fclose(fp);
	return some_total;
}

int get_key_memory_stat(const char *cgroup_path, KeyMemoryStat *key_mem_stat)
{
	memset(fullpath, 0, FULL_PATH_SIZE * sizeof(char));
	memset (read_buffer, 0,  READ_BUFFER_SIZE * sizeof(char));

	// 构建 memory.stat 文件的完整路径
	snprintf(fullpath, sizeof(fullpath), "%s/memory.stat", cgroup_path);

	// 打开 memory.stat 文件
	FILE *stat_file = fopen(fullpath, "r");
	if (stat_file == NULL) {
		LOG_ERROR("can not open %s: %s\n", fullpath, strerror(errno));
		return -1;
	}

	// 初始化结构体
	memset(key_mem_stat, 0, sizeof(KeyMemoryStat));

	// 逐行读取 memory.stat 文件
	while (fgets(read_buffer, sizeof(read_buffer), stat_file) != NULL) {
		char key[128];
		unsigned long long value;

		// 解析每一行，格式为 "key value"
		if (sscanf(read_buffer, "%127s %llu", key, &value) == 2) {
			if (strcmp(key, "workingset_activate_anon") == 0) {
				key_mem_stat->workingset_activate_anon = value;
			} else if (strcmp(key, "workingset_activate_file") == 0) {
				key_mem_stat->workingset_activate_file = value;
			} else if (strcmp(key, "pgfault") == 0) {
				key_mem_stat->pgfault = value;
			} else if (strcmp(key, "pgscan") == 0) {
				key_mem_stat->pgscan = value;
			} else if (strcmp(key, "pgsteal") == 0) {
				key_mem_stat->pgsteal = value;
			} else if (strcmp(key, "pswpin") == 0) {
				key_mem_stat->pswpin = value;
			} else if (strcmp(key, "pswpout") == 0) {
				key_mem_stat->pswpout = value;
			}
		} else {
			LOG_ERROR("can not parse %s", read_buffer);
		}
	}

	fclose(stat_file);
	return 0;
}

// 解析 memory.stat 文件并将结果存储到 MemoryStat 结构体中
int parse_memory_stat(const char *cgroup_path, MemoryStat *mem_stat)
{
	memset(fullpath, 0, FULL_PATH_SIZE * sizeof(char));
	memset (read_buffer, 0,  READ_BUFFER_SIZE * sizeof(char));

	// 构建 memory.stat 文件的完整路径
	snprintf(fullpath, sizeof(fullpath), "%s/memory.stat", cgroup_path);

	// 打开 memory.stat 文件
	FILE *stat_file = fopen(fullpath, "r");
	if (stat_file == NULL) {
		LOG_ERROR("can not open %s: %s\n", fullpath, strerror(errno));
		return -1;
	}

	// 初始化结构体
	memset(mem_stat, 0, sizeof(MemoryStat));

	// 逐行读取 memory.stat 文件
	while (fgets(read_buffer, sizeof(read_buffer), stat_file) != NULL) {
		char key[128];
		unsigned long long value;

		// 解析每一行，格式为 "key value"
		if (sscanf(read_buffer, "%127s %llu", key, &value) == 2) {
			// 根据键名将值存储到结构体中
			if (strcmp(key, "anon") == 0) {
				mem_stat->anon = value;
			} else if (strcmp(key, "file") == 0) {
				mem_stat->file = value;
			} else if (strcmp(key, "kernel_stack") == 0) {
				mem_stat->kernel_stack = value;
			} else if (strcmp(key, "slab") == 0) {
				mem_stat->slab = value;
			} else if (strcmp(key, "sock") == 0) {
				mem_stat->sock = value;
			} else if (strcmp(key, "shmem") == 0) {
				mem_stat->shmem = value;
			} else if (strcmp(key, "file_mapped") == 0) {
				mem_stat->file_mapped = value;
			} else if (strcmp(key, "file_dirty") == 0) {
				mem_stat->file_dirty = value;
			} else if (strcmp(key, "file_writeback") == 0) {
				mem_stat->file_writeback = value;
			} else if (strcmp(key, "anon_thp") == 0) {
				mem_stat->anon_thp = value;
			} else if (strcmp(key, "inactive_anon") == 0) {
				mem_stat->inactive_anon = value;
			} else if (strcmp(key, "active_anon") == 0) {
				mem_stat->active_anon = value;
			} else if (strcmp(key, "inactive_file") == 0) {
				mem_stat->inactive_file = value;
			} else if (strcmp(key, "active_file") == 0) {
				mem_stat->active_file = value;
			} else if (strcmp(key, "unevictable") == 0) {
				mem_stat->unevictable = value;
			} else if (strcmp(key, "slab_reclaimable") == 0) {
				mem_stat->slab_reclaimable = value;
			} else if (strcmp(key, "slab_unreclaimable") == 0) {
				mem_stat->slab_unreclaimable = value;
			} else if (strcmp(key, "workingset_refault_anon") == 0) {
				mem_stat->workingset_refault_anon = value;
			} else if (strcmp(key, "workingset_refault_file") == 0) {
				mem_stat->workingset_refault_file = value;
			} else if (strcmp(key, "workingset_activate_anon") == 0) {
				mem_stat->workingset_activate_anon = value;
			} else if (strcmp(key, "workingset_activate_file") == 0) {
				mem_stat->workingset_activate_file = value;
			} else if (strcmp(key, "workingset_nodereclaim") == 0) {
				mem_stat->workingset_nodereclaim = value;
			} else if (strcmp(key, "pgfault") == 0) {
				mem_stat->pgfault = value;
			} else if (strcmp(key, "pgmajfault") == 0) {
				mem_stat->pgmajfault = value;
			} else if (strcmp(key, "pgrefill") == 0) {
				mem_stat->pgrefill = value;
			} else if (strcmp(key, "pgscan") == 0) {
				mem_stat->pgscan = value;
			} else if (strcmp(key, "pgsteal") == 0) {
				mem_stat->pgsteal = value;
			} else if (strcmp(key, "pswpin") == 0) {
				mem_stat->pswpin = value;
			} else if (strcmp(key, "pswpout") == 0) {
				mem_stat->pswpout = value;
			} else if (strcmp(key, "pgscan_direct") == 0) {
				mem_stat->pgscan_direct = value;
			} else if (strcmp(key, "pgactivate") == 0) {
				mem_stat->pgactivate = value;
			} else if (strcmp(key, "pgdeactivate") == 0) {
				mem_stat->pgdeactivate = value;
			} else if (strcmp(key, "pglazyfree") == 0) {
				mem_stat->pglazyfree = value;
			} else if (strcmp(key, "pglazyfreed") == 0) {
				mem_stat->pglazyfreed = value;
			} else if (strcmp(key, "thp_fault_alloc") == 0) {
				mem_stat->thp_fault_alloc = value;
			} else if (strcmp(key, "thp_collapse_alloc") == 0) {
				mem_stat->thp_collapse_alloc = value;
			}
			// 可以根据需要添加更多字段的解析
		} else {
			LOG_ERROR("can not parse this line %s", read_buffer);
		}
	}

	// 关闭文件
	fclose(stat_file);
	return 0;
}

// 解析 memory.stat 文件，获取指定键的值
int parse_memory_stat_value(const char *cgroup_path, const char *key,
			    unsigned long long *value)
{
	int found = 0;

	memset(fullpath, 0, FULL_PATH_SIZE * sizeof(char));
	memset (read_buffer, 0,  READ_BUFFER_SIZE * sizeof(char));

	// 构建 memory.stat 文件的完整路径
	snprintf(fullpath, sizeof(fullpath), "%s/memory.stat", cgroup_path);

	// 打开 memory.stat 文件
	FILE *stat_file = fopen(fullpath, "r");
	if (stat_file == NULL) {
		LOG_ERROR("cant not open file %s: %s\n", fullpath, strerror(errno));
		return -1;
	}

	// 逐行读取 memory.stat 文件
	while (fgets(read_buffer, sizeof(read_buffer), stat_file) != NULL) {
		char current_key[128];
		unsigned long long current_value;

		// 解析每一行，格式为 "key value"
		if (sscanf(read_buffer, "%127s %llu", current_key, &current_value) == 2) {
			if (strcmp(current_key, key) == 0) {
				*value = current_value;
				found = 1;
				break;
			}
		} else {
			LOG_ERROR("can not parse this line %s", read_buffer);
		}
	}

	// 关闭文件
	fclose(stat_file);

	if (found) {
		return 0;
	} else {
		LOG_ERROR("not find '%s' in %s\n", key, fullpath);
		return -1;
	}
}

void parse_memory_stat_to_file(const char *cgroup_path,
			       const char *output_file_path)
{
	FILE *stat_file = NULL;
	FILE *output_file = NULL;

	memset(fullpath, 0, FULL_PATH_SIZE * sizeof(char));
	memset (read_buffer, 0,  READ_BUFFER_SIZE * sizeof(char));

	// 构建 memory.stat 文件的完整路径
	snprintf(fullpath, sizeof(fullpath), "%s/memory.stat", cgroup_path);

	// 打开 memory.stat 文件
	stat_file = fopen(fullpath, "r");
	if (stat_file == NULL) {
		LOG_ERROR("can not open file %s: %s\n", fullpath, strerror(errno));
		return;
	}

	// 打开输出文件
	output_file = fopen(output_file_path, "a");
	if (output_file == NULL) {
		LOG_ERROR("can open output file %s: %s\n", output_file_path,
			strerror(errno));
		fclose(stat_file);
		return;
	}

	// 逐行读取 memory.stat 文件
	while (fgets(read_buffer, sizeof(read_buffer), stat_file) != NULL) {
		unsigned long long value;
		char key[128];

		// 解析每一行，格式为 "key value"
		if (sscanf(read_buffer, "%127s %llu", key, &value) == 2) {
			// 将解析后的键值对写入输出文件
			fprintf(output_file, "%s: %llu\n", key, value);
		} else {
			LOG_ERROR("can not parse  %s", read_buffer);
		}
	}

	// 关闭文件
	fclose(stat_file);
	fclose(output_file);
}
