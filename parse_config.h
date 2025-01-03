/*
 * Copyright (C) 2022, Bytedance. All rights reserved
 * Author: JiaHao
 * Create: Fri Jan 10 16:50:38 2025
 */
#ifndef PARSE_CONFIG_H
#define PARSE_CONFIG_H

// Defines the configuration parameter structure
struct config {
	char *cgroup_path;
	float psi_threshold;			// PSI threshold
	float reclaim_ratio;			// Reclaim ratio, between 0 and 1
	float reclaim_accuracy_ratio;		// Reclaim accuracy ratio, between 0 and 1
	float reclaim_scan_efficiency_ratio;	// Reclaim scan efficiency ratio, between 0 and 1
	long min_size;				// Minimum memory limit, in bytes
	long iterate_max_size;			// Single iterate maximum memory limit, in bytes
	long iterate_min_size;			// Single iterate minmum memory limit, in bytes
	long interval;				// Sampling interval, in seconds
};

struct config *get_sys_config(void);
void print_config(const struct config *config);
void parse_args(int argc, char *argv[]);

#endif // PARSE_CONFIG_H
