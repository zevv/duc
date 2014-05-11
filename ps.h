#ifndef ps_h
#define ps_h

#include <getopt.h>
#include <unistd.h>

struct cmd {
	char *name;
	int (*main)(int argc, char **argv);
	char *description;
	char *help;
};


#endif
