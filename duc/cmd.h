#ifndef ps_h
#define ps_h

#include <getopt.h>
#include <unistd.h>

#include "ducrc.h"

struct cmd {
	char *name;
	int (*main)(int argc, char **argv);
	char *description;
	char *usage;
	char *help;
	struct ducrc_option *options;
};


#endif
