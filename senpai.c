/*
 * Copyright (C) 2024, LiXiang. All rights reserved
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
#include <getopt.h>
#include "logger.h"
#include "cgroup_op.h"
#include "parse_config.h"

#define PAGE_ALIGN_MASK		(~4095)		// 4k page size align

#define min(x,y) ({			\
	typeof(x) _x = (x);		\
	typeof(y) _y = (y);		\
	(void) (&_x == &_y);		\
	_x < _y ? _x : _y; })

#define max(x,y) ({			\
	typeof(x) _x = (x);		\
	typeof(y) _y = (y);		\
	(void) (&_x == &_y);		\
	_x > _y ? _x : _y; })

#define DIV(a, b) ({						\
	typeof(a) _a = (a);					\
	typeof(b) _b = (b);					\
	(void) (&_a == &_b);					\
	((_b) != 0) ? ((double)(_a) / (double)(_b)) : 0.0;	\
})

KeyMemoryStat key_mem_stat_curr;
KeyMemoryStat key_mem_stat_old;

// Initialize the configuration structure
Config config = {
	.cgroup_path = NULL,
	.psi_threshold = 0.1,
	.reclaim_ratio = 0.1,
	.reclaim_accuracy_ratio = 0.9,
	.reclaim_scan_efficiency_ratio = 0.3,
	.min_size = (10 << 20),			// 20M
	.iterate_max_size = (50 << 20),		// 50M
	.iterate_min_size = (1 << 20),		// 1M
	.interval = 5,
};

// Define global variables, initial value is INFO level, logging enabled
LogLevel g_log_level = LOG_LEVEL_INFO;
int g_log_enabled = 1;

char fullpath[FULL_PATH_SIZE];
char read_buffer[READ_BUFFER_SIZE];
// Global variable for signal handling
volatile sig_atomic_t stop = 0;

// Signal handling function
static void handle_sigint(int sig)
{
	stop = 1;
}

// Adjust memory
static inline int adjust_memory(const char *cgroup_path, long reclaim_mem)
{
	char value[64] = {0};
	long ret;

	snprintf(value, sizeof(value), "%ld", reclaim_mem);

	ret = write_cgroup_file(cgroup_path, "memory.reclaim", value);
//	LOG_DEBUG("real reclaim size %ld ret %ld\n", reclaim_mem, ret);

	return ret;
}

static long psi_advice(float psi_some, float reclaim_ratio,
		       long current_mem, float psi_threshold)
{
	float result;

	result = max(0.0, 1.0 - DIV(psi_some, psi_threshold)) * reclaim_ratio * current_mem;

	LOG_DEBUG("psi %.2f psi_threshold %.2f reclaim_ratio %.2f current_mem %ld result %.2f\n",
		  psi_some, psi_threshold, reclaim_ratio, current_mem, result);

	return (long)result;
}

static long refault_advice(unsigned long off_load_size, float accuracy_target, float scan_efficiency_target)
{
	long pgscanDelta, pgstealDelta, refaultDelta;
	float reclaim_scan_efficiency_ratio = 1.0;
	float reclaim_accuracy_ratio = 1.0;

	key_mem_stat_old = key_mem_stat_curr;
	get_key_memory_stat(config.cgroup_path, &key_mem_stat_curr);
	LOG_DEBUG("pswpin %ld pswpout %ld\n", key_mem_stat_curr.pswpin, key_mem_stat_curr.pswpout);
	LOG_DEBUG("key_mem_stat_curr.pgscan  %ld key_mem_stat_old.pgscan %ld\n", key_mem_stat_curr.pgscan, key_mem_stat_old.pgscan);
	LOG_DEBUG("key_mem_stat_curr.pgsteal  %ld key_mem_stat_old.pgsteal %ld\n", key_mem_stat_curr.pgsteal, key_mem_stat_old.pgsteal);
	LOG_DEBUG("key_mem_stat_curr.workingset_activate_anon  %ld key_mem_stat_old.workingset_activate_anon %ld\n", key_mem_stat_curr.workingset_activate_anon, key_mem_stat_old.workingset_activate_anon);
	LOG_DEBUG("key_mem_stat_curr.workingset_activate_file  %ld key_mem_stat_old.workingset_activate_file %ld\n", key_mem_stat_curr.workingset_activate_file, key_mem_stat_old.workingset_activate_file);

	pgscanDelta = key_mem_stat_curr.pgscan - key_mem_stat_old.pgscan;
	pgstealDelta = key_mem_stat_curr.pgsteal - key_mem_stat_old.pgsteal;
	refaultDelta = key_mem_stat_curr.workingset_activate_anon - key_mem_stat_old.workingset_activate_anon;
	refaultDelta += (key_mem_stat_curr.workingset_activate_file - key_mem_stat_old.workingset_activate_file);

	if (pgstealDelta > 0 && pgscanDelta > 0) {
		reclaim_accuracy_ratio = 1.0 - DIV(refaultDelta, pgstealDelta);
		reclaim_scan_efficiency_ratio = DIV(pgstealDelta, pgscanDelta);
	} else if (pgstealDelta == 0 && pgscanDelta == 0) {
		// The memory statistics are not updated and are directly using reclaim_accuracy_ratio
		off_load_size = (unsigned long) max(0.0, (double)off_load_size * accuracy_target);

		LOG_DEBUG("reclaim_accuracy_ratio %.2f reclaim_scan_efficiency_ratio %.2f off_load_size %lu pgscanDelta %ld pgstealDelta %ld refaultDelta %ld\n",
			  reclaim_accuracy_ratio, reclaim_scan_efficiency_ratio, off_load_size, pgscanDelta, pgstealDelta, refaultDelta);
		return off_load_size;
	}
/*
	if (reclaim_accuracy_ratio < 0.5
		|| reclaim_scan_efficiency_ratio < 0.1) {
		// Decrease offloading size if detecting the reclaim accuracy or scan efficiency is below the targets
		LOG_DEBUG("return 0 !!! reclaim_accuracy_ratio %.2f reclaim_scan_efficiency_ratio %.2f pgscanDelta %ld pgstealDelta %ld refaultDelta %ld\n",
			reclaim_accuracy_ratio, reclaim_scan_efficiency_ratio, pgscanDelta, pgstealDelta, refaultDelta);
		return 0;
	}
*/
	if (reclaim_accuracy_ratio < accuracy_target ||
	    reclaim_scan_efficiency_ratio < scan_efficiency_target) {
		// Decrease offloading size if detecting the reclaim accuracy or scan efficiency is below the targets
		off_load_size = (unsigned long) max(0.0, (double)off_load_size * reclaim_accuracy_ratio);
	}


	LOG_DEBUG("reclaim_accuracy_ratio %.2f reclaim_scan_efficiency_ratio %.2f off_load_size %lu pgscanDelta %ld pgstealDelta %ld refaultDelta %ld\n",
		  reclaim_accuracy_ratio, reclaim_scan_efficiency_ratio, off_load_size, pgscanDelta, pgstealDelta, refaultDelta);

	return off_load_size;
}


int main(int argc, char *argv[])
{
	long reclaim_mem = 0;
	long current_mem = 0;
	float psi_some;

	// Parse command line arguments
	parse_args(argc, argv);
	print_config(&config);

	// Set up signal handling
	signal(SIGINT, handle_sigint);
	signal(SIGTERM, handle_sigint);

	// Main loop
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
			reclaim_mem = psi_advice(psi_some, config.reclaim_ratio, current_mem,
							  config.psi_threshold);

/*			long new_mem = current_mem - reclaim_mem;
			if (new_mem < config.min_size) {
				reclaim_mem = 0;
			}
*/

			reclaim_mem = refault_advice(reclaim_mem, config.reclaim_accuracy_ratio,
							config.reclaim_scan_efficiency_ratio);

			if (reclaim_mem < config.iterate_min_size) {
				reclaim_mem = 0;
			}
			// Adjust memory
			if (reclaim_mem > 0) {
				reclaim_mem = min(reclaim_mem, config.iterate_max_size);
				// page alignment
				reclaim_mem &= PAGE_ALIGN_MASK;
				if (adjust_memory(config.cgroup_path, reclaim_mem) < 0) {
					LOG_ERROR("Failed to adjust memory limits\n");
				}
			}
			LOG_DEBUG("reclaim %ld kB  PSI some: %.2f current_mem %ld KB config.min_size %ld KB\n\n\n",
			        reclaim_mem >> 10 , psi_some, current_mem >> 10, config.min_size >> 10);
		} else {
			reclaim_mem = 0;
			LOG_DEBUG("stop %ld kB  PSI some: %.2f current_mem %ld KB\n\n\n",
			        reclaim_mem >> 10 , psi_some, current_mem >> 10);
		}

SLEEP:
		sleep(config.interval);
	}

	LOG_DEBUG("exit\n");
	return 0;
}
