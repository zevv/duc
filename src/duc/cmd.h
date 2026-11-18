#ifndef ps_h
#define ps_h

#include <getopt.h>
#include <unistd.h>

#include "duc.h"
#include "ducrc.h"

struct cmd {
	char *name;
	int (*init)(duc *duc, int argc, char **argv);
	int (*main)(duc *duc, int argc, char **argv);
	char *descr_short;
	char *descr_long;
	char *usage;
	struct ducrc_option *options;
	int hidden;
};


/*
 * For subcommands that list the index reports: returns 0 when the
 * duc_get_report() loop ended past the last report, or logs the error and
 * returns -1 when it ended on a report that cannot be read
 */

int cmd_report_list_end(duc *duc);


#endif
