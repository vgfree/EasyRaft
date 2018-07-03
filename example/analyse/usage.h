#pragma once

typedef struct
{
	/* options */
	char    *id;
	char    *cluster;
	char    *db_path;
	char    *db_size;
} options_t;

int parse_options(const int argc, const char *const argv[], options_t *opt);

