/*
 * Copyright (C) 2025, LiXiang. All rights reserved
 * Author: JiaHao
 * Create: Fri Jan 10 16:12:21 2025
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <getopt.h>
#include "logger.h"
#include "parse_config.h"

#define CONFIG_FILE_DEFAULT "LiMO.conf"

// Initialize the configuration structure
static struct config config = {
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

char buffer[256];

struct config *get_sys_config(void)
{
	return &config;
}
//Remove leading and trailing whitespace characters from a string
static char *trim_whitespace(char *str)
{
	char *end;

	// Remove leading whitespace characters
	while (isspace(*str))
		str++;

	// All characters are whitespace
	if (*str == 0)
		return str;

	// Remove trailing whitespace characters
	end = str + strlen(str) - 1;
	while (end > str && isspace(*end))
		end--;

	// Set new string end
	end[1] = '\0';

	return str;
}

// Read configuration file
static int read_config(const char *filename, struct config *config)
{
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		LOG_ERROR("Unable to open the configuration file\n");
		return -1;
	}

	memset (buffer, 0, sizeof(buffer));
	while (fgets(buffer, sizeof(buffer), fp)) {
		char *trimmed_line = trim_whitespace(buffer);

		// Skip empty lines and comments
		if (trimmed_line[0] == '\0' || trimmed_line[0] == '#')
			continue;

		char key[128], value[128];
		if (sscanf(trimmed_line, "%127[^=]=%127s", key, value) == 2) {
			char *trimmed_key = trim_whitespace(key);
			char *trimmed_value = trim_whitespace(value);

			if (strcmp(trimmed_key, "PSI_THRESHOLD") == 0) {
				config->psi_threshold = atof(trimmed_value);
			} else if (strcmp(trimmed_key, "RECLAIM_RATIO") == 0) {
				config->reclaim_ratio = atof(trimmed_value);
			} else if (strcmp(trimmed_key, "MIN_SIZE") == 0) {
				config->min_size = atol(trimmed_value);
			} else if (strcmp(trimmed_key, "ITERATE_MAX_SIZE") == 0) {
				config->iterate_max_size = atol(trimmed_value);
			} else if (strcmp(trimmed_key, "ITERATE_MIN_SIZE") == 0) {
				config->iterate_min_size = atol(trimmed_value);
			} else if (strcmp(trimmed_key, "RECLAIM_ACCURACY") == 0) {
				config->reclaim_accuracy_ratio = atof(trimmed_value);
			} else if (strcmp(trimmed_key, "SCAN_EFFICIENCY") == 0) {
				config->reclaim_scan_efficiency_ratio = atof(trimmed_value);
			} else if (strcmp(trimmed_key, "INTERVAL") == 0) {
				config->interval = atol(trimmed_value);
			} else {
				LOG_DEBUG("Unknown configuration item: %s\n", trimmed_key);
			}
		} else {
			LOG_DEBUG("Configuration file format error: %s\n", trimmed_line);
		}
	}

	fclose(fp);
	return 0;
}

static void print_help(const char *prog_name)
{
	printf("Usage: %s [options]\n", prog_name);
	printf("Options:\n");
	printf("  -c, --config <file>   Specify the configuration file path\n");
	printf("  -v, --verbose         Enable verbose output\n");
	printf("  -h, --help            Display this help message and exit\n");
}

void print_config(const struct config *config)
{
	printf("  cgroup path %s\n", config->cgroup_path);
	printf("  psi_threshold %.2f\n", config->psi_threshold);
	printf("  reclaim_ratio %.2f\n", config->reclaim_ratio);
	printf("  reclaim_accuracy_ratio %.2f\n", config->reclaim_accuracy_ratio);
	printf("  reclaim_scan_efficiency_ratio %.2f\n", config->reclaim_scan_efficiency_ratio);
	printf("  min_size %ld\n", config->min_size);
	printf("  iterate_max_size %ld\n", config->iterate_max_size);
	printf("  iterate_min_size %ld\n", config->iterate_min_size);
	printf("  interval %ld\n", config->interval);
}

// Define long options
static struct option long_options[] = {
	{"config",  required_argument, 0, 'c'},
	{"verbose", no_argument,       0, 'v'},
	{"help",    no_argument,       0, 'h'},
	{0,         0,                 0,  0 }
};

void parse_args(int argc, char *argv[])
{
	const char *config_file = CONFIG_FILE_DEFAULT;
	int option_index = 0;
	int set_cgroup = 0;
	int opt;

	while ((opt = getopt_long(argc, argv, "c:g:vh", long_options, &option_index)) != -1) {
		switch (opt) {
		case 'c':
			config_file = optarg;
			// Read configuration file
			if (read_config(config_file, &config) == -1) {
				exit(EXIT_FAILURE);
			}
		break;
		case 'g':
			config.cgroup_path = optarg;
//			config_file->cgroup_path = strdup(optarg);
			if (!config.cgroup_path) {
				LOG_ERROR( "Missing CGROUP item\n");
				exit(EXIT_FAILURE);
			}
			set_cgroup = 1;
			break;
		case 'v':
			enable_log_level();
			set_log_level(LOG_LEVEL_DEBUG);
			LOG_INFO("Enable DEBUG level logs\n");
			break;
		case 'h':
			print_help(argv[0]);
			exit(0);
		default:
			LOG_ERROR("Usage: %s [-g cgroup path]\n\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (!set_cgroup) {
		LOG_ERROR( "Missing CGROUP item\n");
		exit(EXIT_FAILURE);
	}
}
