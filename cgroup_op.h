/*
 * Copyright (C) 2025, LiXiang. All rights reserved
 * Author: JiaHao
 * Create: Tue Jan 07 14:55:31 2025
 */

#ifndef CGROUP_OP_H
#define CGROUP_OP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Key memory statistics for memory reclamation
typedef struct {
	unsigned long long anon;
	unsigned long long file;
	unsigned long long file_mapped;
	unsigned long long file_dirty;
	unsigned long long file_writeback;
	unsigned long long anon_thp;
	unsigned long long inactive_anon;
	unsigned long long active_anon;
	unsigned long long inactive_file;
	unsigned long long active_file;
	unsigned long long workingset_refault_anon;
	unsigned long long workingset_refault_file;
	unsigned long long workingset_activate_anon;
	unsigned long long workingset_activate_file;
	unsigned long long pgfault;
	unsigned long long pgmajfault;
	unsigned long long pgrefill;
	unsigned long long pgscan;
	unsigned long long pgsteal;
	unsigned long long pswpin;
	unsigned long long pswpout;
	unsigned long long pgscan_direct;
	unsigned long long pgactivate;
	unsigned long long pgdeactivate;
} KeyMemoryStat;

// All memory statistics are used for debugging
typedef struct {
	unsigned long long anon;
	unsigned long long file;
	unsigned long long kernel;
	unsigned long long kernel_stack;
	unsigned long long slab;
	unsigned long long sock;
	unsigned long long shmem;
	unsigned long long file_mapped;
	unsigned long long file_dirty;
	unsigned long long file_writeback;
	unsigned long long anon_thp;
	unsigned long long inactive_anon;
	unsigned long long active_anon;
	unsigned long long inactive_file;
	unsigned long long active_file;
	unsigned long long unevictable;
	unsigned long long slab_reclaimable;
	unsigned long long slab_unreclaimable;
	unsigned long long workingset_refault_anon;
	unsigned long long workingset_refault_file;
	unsigned long long workingset_activate_anon;
	unsigned long long workingset_activate_file;
	unsigned long long workingset_restore_anon;
	unsigned long long workingset_restore_file;
	unsigned long long workingset_nodereclaim;
	unsigned long long pgfault;
	unsigned long long pgmajfault;
	unsigned long long pgrefill;
	unsigned long long pgscan;
	unsigned long long pgsteal;
	unsigned long long pswpin;
	unsigned long long pswpout;
	unsigned long long pgscan_direct;
	unsigned long long pgactivate;
	unsigned long long pgdeactivate;
	unsigned long long pglazyfree;
	unsigned long long pglazyfreed;
	unsigned long long thp_fault_alloc;
	unsigned long long thp_collapse_alloc;
} MemoryStat;

#define FULL_PATH_SIZE			512
#define READ_BUFFER_SIZE		256
extern char fullpath[FULL_PATH_SIZE];
extern char read_buffer[READ_BUFFER_SIZE];

long get_current_memory(const char *cgroup_path);
float get_psi_some(const char *cgroup_path, long interval);
int get_key_memory_stat(const char *cgroup_path, KeyMemoryStat *key_mem_stat);
int parse_memory_stat(const char *cgroup_path, MemoryStat *mem_stat);
int parse_memory_stat_value(const char *cgroup_path, const char *key,
			    unsigned long long *value);
void parse_memory_stat_to_file(const char *cgroup_path,
			       const char *output_file_path);
int write_cgroup_file(const char *path, const char *filename,
		      const char *buf);

#endif // CGROUP_OP_H
