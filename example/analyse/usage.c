#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "carg_parser.h"
#include "usage.h"

#define VERSION "0.1.0"
static const char *const        program_name = "analyse";
static const char               *invocation_name = 0;

static void show_version(void)
{
	printf("analyse %s\n", VERSION);
	printf("Copyright (C) 2018 Huiqi qian.\n");
	printf("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
		"This is free software: you are free to change and redistribute it.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n");
}

static void show_help(void)
{
	fprintf(stdout, "analyse - a unique ticket server\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "Usage: %s [options]\n", invocation_name);
	fprintf(stdout, "  analyse --id ID --cluster CLUSTER [-p DB_PATH | -s DB_SIZE]\n");
	fprintf(stdout, "  analyse --version\n");
	fprintf(stdout, "  analyse --help\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "Options:\n");
	fprintf(stdout, "  -i --id=<arg>            This server's manually set Raft ID\n");
	fprintf(stdout, "  -c --cluster=<arg>       This cluster of all Raft node\n");
	fprintf(stdout, "  -p --db_path=<arg>       Path where database files will be kept [default: data]\n");
	fprintf(stdout, "  -s --db_size=<arg>       Size of database in megabytes [default: 1000]\n");
	fprintf(stdout, "  -v --version             Display version.\n");
	fprintf(stdout, "  -h --help                Prints a short usage summary.\n");
	fprintf(stdout, "\n");
}

static void show_error(const char *const msg, const int errcode, const char help)
{
	if (msg && msg[0]) {
		fprintf(stderr, "%s: %s", program_name, msg);

		if (errcode > 0) {
			fprintf(stderr, ": %s", strerror(errcode));
		}

		fprintf(stderr, "\n");
	}

	if (help) {
		fprintf(stderr, "Try '%s --help' for more information.\n",
			invocation_name);
	}
}

static const char *optname(const int code, const struct ap_Option options[])
{
	static char     buf[2] = "?";
	int             i;

	if (code != 0) {
		for (i = 0; options[i].code; ++i) {
			if (code == options[i].code) {
				if (options[i].name) {
					return options[i].name;
				} else {
					break;
				}
			}
		}
	}

	if ((code > 0) && (code < 256)) {
		buf[0] = code;
	} else {
		buf[0] = '?';
	}

	return buf;
}

static void options_init(options_t *opt)
{
	memset(opt, 0, sizeof(options_t));

	opt->id = strdup("0");
	opt->cluster = strdup("127.0.0.1:7000,127.0.0.1:7001");
	opt->db_path = strdup("data");
	opt->db_size = strdup("1000");
}

int parse_options(const int argc, const char *const argv[], options_t *opt)
{
	invocation_name = argv[0];

	options_init(opt);

	const struct ap_Option options[] =
	{
		{ 'i', "id",      ap_yes    },
		{ 'c', "cluster", ap_yes    },
		{ 'p', "db_path", ap_yes    },
		{ 's', "db_size", ap_yes    },
		{ 'v', "version", ap_no     },
		{ 'h', "help",    ap_no     },
		{   0, 0,         ap_no     }
	};

	struct Arg_parser parser;

	if (!ap_init(&parser, argc, argv, options, 0)) {
		show_error("Memory exhausted.", 0, 0); return -1;
	}

	if (ap_error(&parser)) {					/* bad option */
		show_error(ap_error(&parser), 0, 1); return -1;
	}

	int argind;

	for (argind = 0; argind < ap_arguments(&parser); ++argind) {
		const int               code = ap_code(&parser, argind);
		const char *const       arg = ap_argument(&parser, argind);

		if (!code) {
			break;						/* no more options */
		}

		switch (code)
		{
			case 'v':
				show_version();
				exit(0);

			case 'h':
				show_help();
				exit(0);

			case 'i':

				if (arg[0]) {
					opt->id = strdup(arg);
				}

				break;

			case 'c':

				if (arg[0]) {
					opt->cluster = strdup(arg);
				}

				break;

			case 'p':

				if (arg[0]) {
					opt->db_path = strdup(arg);
				}

				break;

			case 's':

				if (arg[0]) {
					opt->db_size = strdup(arg);
				}

				break;

			default:
				fprintf(stderr, "%s: internal error: uncaught option.\n", program_name);
				exit(3);
		}
	}		/* end process options */

	if (!ap_arguments(&parser)) {
		show_help();
		exit(0);
	}

	return 0;
}

