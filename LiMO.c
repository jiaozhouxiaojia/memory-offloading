/*
 * Copyright (C) 2024, LiXiang. All rights reserved
 * Author: JiaHao
 * Create: Thu Dec 26 17:32:34 2024
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include "logger.h"
#include "cgroup_op.h"
#include "parse_config.h"

#define PAGE_ALIGN_MASK						(~4095)		// 4k page size align

#define min(x,y) ({						\
	typeof(x) _x = (x);					\
	typeof(y) _y = (y);					\
	(void) (&_x == &_y);					\
	_x < _y ? _x : _y; })

#define max(x,y) ({						\
	typeof(x) _x = (x);					\
	typeof(y) _y = (y);					\
	(void) (&_x == &_y);					\
	_x > _y ? _x : _y; })

#define DIV(a, b) ({						\
	typeof(a) _a = (a);					\
	typeof(b) _b = (b);					\
	(void) (&_a == &_b);					\
	((_b) != 0) ? ((double)(_a) / (double)(_b)) : 0.0;	\
})

static struct key_memory_stat key_mem_stat;
static struct key_memory_stat key_mem_stat_last;

// Global variable for signal handling
volatile sig_atomic_t stop = 0;

// Signal handling function
static void handle_sigint(int sig)
{
	LOG_ERROR("LiMO exit receive signal %d\n", sig);
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

static long psi_advice(long current_mem, float psi_some,
		       struct config *config)
{
	float result;

	result = max(0.0, 1.0 - DIV(psi_some, config->psi_threshold)) * config->reclaim_ratio * current_mem;

	LOG_DEBUG("psi %.2f psi_threshold %.2f config->reclaim_ratio %.2f current_mem %ld result %.2f\n",
		  psi_some, config->psi_threshold, config->reclaim_ratio, current_mem, result);

	return (long)result;
}

static long refault_advice(unsigned long off_load_size, struct config *config)
{
	long pgscanDelta, pgstealDelta, refaultDelta;
	float reclaim_scan_efficiency_ratio = 1.0;
	float reclaim_accuracy_ratio = 1.0;

	key_mem_stat_last = key_mem_stat;
	get_key_memory_stat(config->cgroup_path, &key_mem_stat);
	LOG_DEBUG("pswpin %ld pswpout %ld\n", key_mem_stat.pswpin, key_mem_stat.pswpout);
	LOG_DEBUG("key_mem_stat.pgscan  %ld key_mem_stat_last.pgscan %ld\n", key_mem_stat.pgscan, key_mem_stat_last.pgscan);
	LOG_DEBUG("key_mem_stat.pgsteal  %ld key_mem_stat_last.pgsteal %ld\n", key_mem_stat.pgsteal, key_mem_stat_last.pgsteal);
	LOG_DEBUG("key_mem_stat.workingset_activate_anon  %ld key_mem_stat_last.workingset_activate_anon %ld\n", key_mem_stat.workingset_activate_anon, key_mem_stat_last.workingset_activate_anon);
	LOG_DEBUG("key_mem_stat.workingset_activate_file  %ld key_mem_stat_last.workingset_activate_file %ld\n", key_mem_stat.workingset_activate_file, key_mem_stat_last.workingset_activate_file);

	pgscanDelta = key_mem_stat.pgscan - key_mem_stat_last.pgscan;
	pgstealDelta = key_mem_stat.pgsteal - key_mem_stat_last.pgsteal;
	refaultDelta = key_mem_stat.workingset_activate_anon - key_mem_stat_last.workingset_activate_anon;
	refaultDelta += (key_mem_stat.workingset_activate_file - key_mem_stat_last.workingset_activate_file);

	if (pgstealDelta > 0 && pgscanDelta > 0) {
		reclaim_accuracy_ratio = 1.0 - DIV(refaultDelta, pgstealDelta);
		reclaim_scan_efficiency_ratio = DIV(pgstealDelta, pgscanDelta);
	} else if (pgstealDelta == 0 && pgscanDelta == 0) {
		// The memory statistics are not updated and are directly using reclaim_accuracy_ratio
		off_load_size = (unsigned long) max(0.0, (double)off_load_size * config->reclaim_accuracy_ratio);

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
	if (reclaim_accuracy_ratio < config->reclaim_accuracy_ratio ||
	    reclaim_scan_efficiency_ratio < config->reclaim_scan_efficiency_ratio) {
		// Decrease offloading size if detecting the reclaim accuracy or scan efficiency is below the targets
		off_load_size = (unsigned long) max(0.0, (double)off_load_size * reclaim_accuracy_ratio);
	}


	LOG_DEBUG("reclaim_accuracy_ratio %.2f reclaim_scan_efficiency_ratio %.2f off_load_size %lu pgscanDelta %ld pgstealDelta %ld refaultDelta %ld\n",
		  reclaim_accuracy_ratio, reclaim_scan_efficiency_ratio, off_load_size, pgscanDelta, pgstealDelta, refaultDelta);

	return off_load_size;
}


int main(int argc, char *argv[])
{
	struct config *config = get_sys_config();
	long reclaim_mem = 0;
	long current_mem = 0;
	float psi_some;

	// Parse command line arguments
	parse_args(argc, argv);
	print_config(config);


	// Set up signal handling
	signal(SIGINT, handle_sigint);
	signal(SIGTERM, handle_sigint);

	// Main loop
	while (!stop) {
		LOG_DEBUG("tick config.interval %ld\n", config->interval);

		current_mem = get_current_memory(config->cgroup_path);
		if (current_mem < 0) {
			LOG_ERROR("Failed to obtain the current memory usage\n");
			break;
		}

		if (current_mem < config->min_size)
			goto SLEEP;

		psi_some = get_psi_some(config->cgroup_path, config->interval);
		if (psi_some < 0.0) {
			LOG_ERROR("Failed to get PSI information.\n");
			break;
		}

		// Calculate the amount of memory to be reclaimed
		if (psi_some < config->psi_threshold) {
			reclaim_mem = psi_advice(current_mem, psi_some, config);

/*			long new_mem = current_mem - reclaim_mem;
			if (new_mem < config->min_size) {
				reclaim_mem = 0;
			}
*/

			if (reclaim_mem <= 0)
				goto SLEEP;

			reclaim_mem = refault_advice(reclaim_mem, config);
			if (reclaim_mem < config->iterate_min_size) {
				reclaim_mem = 0;
			}
			// Adjust memory
			if (reclaim_mem > 0) {
				reclaim_mem = min(reclaim_mem, config->iterate_max_size);
				// page alignment
				reclaim_mem &= PAGE_ALIGN_MASK;
				if (adjust_memory(config->cgroup_path, reclaim_mem) < 0) {
					LOG_ERROR("Failed to adjust memory limits\n");
				}
			}
			LOG_DEBUG("## reclaim %ld kB ##  PSI some: %.2f current_mem %ld KB config.min_size %ld KB\n\n\n",
				  reclaim_mem >> 10, psi_some, current_mem >> 10, config->min_size >> 10);
		} else {
			reclaim_mem = 0;
			LOG_DEBUG("stop %ld kB  PSI some: %.2f current_mem %ld KB\n\n\n",
				  reclaim_mem >> 10, psi_some, current_mem >> 10);
		}

SLEEP:
		sleep(config->interval);
	}

	LOG_DEBUG("exit\n");
	return 0;
}
