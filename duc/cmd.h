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
	char *description;
	char *usage;
	struct ducrc_option *options;
};


#endif
