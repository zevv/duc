
#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "cmd.h"
#include "ducrc.h"


struct cmd cmd_help;
struct cmd cmd_info;
struct cmd cmd_index;
struct cmd cmd_ls;
struct cmd cmd_gui;
struct cmd cmd_graph;
struct cmd cmd_xml;
struct cmd cmd_cgi;


struct cmd *cmd_list[] = {
	&cmd_help,
	&cmd_info,
	&cmd_index,
	&cmd_gui,
	&cmd_ls,
	&cmd_graph,
	&cmd_xml,
	&cmd_cgi,
};

#define SUBCOMMAND_COUNT (sizeof(cmd_list) / sizeof(cmd_list[0]))

static struct cmd *find_cmd_by_name(const char *name);
static void help_cmd(struct cmd *cmd);




static struct ducrc_option option_list[] = {
	{ "database", 'd', DUCRC_TYPE_STRING, "~/.duc.db", "select database file to use" },
	{ "verbose",  'v', DUCRC_TYPE_BOOL,   "false",     "increase verbosity. can be passwd more then once for debugging" },
	{ "quiet",    'q', DUCRC_TYPE_BOOL,   "false",     "quiet mode, do not print any warning" },
	{ NULL }
};


int main(int argc, char **argv)
{
	struct cmd *cmd = NULL;

	if(argc >= 2) {
		cmd = find_cmd_by_name(argv[1]);
	}

	if(cmd == NULL) {
		if(getenv("QUERY_STRING")) {
			cmd = &cmd_cgi;
		} else {
			cmd = &cmd_help;
		}
	}
	/*
	 * Try to read configuration files from the following locations:
	 *
	 * - /etc/ducrc
	 * - ~/.ducrc
	 * - ./.ducrc
	 *
	 */

	struct ducrc *ducrc = ducrc_new();

	ducrc_read(ducrc, "/etc/ducrc");

	char *home = getenv("HOME");
	if(home) {
		char tmp[PATH_MAX];
		snprintf(tmp, sizeof(tmp), "%s/.ducrc", home);
		ducrc_read(ducrc, tmp);
	}

	ducrc_read(ducrc, "./.ducrc");
	ducrc_dump(ducrc);



	int r = cmd->main(argc-1, argv+1);
	if(r == -2) help_cmd(cmd);

	return (r == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


static struct cmd *find_cmd_by_name(const char *name)
{
	int i;

	for(i=0; i<SUBCOMMAND_COUNT; i++) {
		struct cmd *cmd = cmd_list[i];
		if(strcmp(cmd->name, name) == 0) {
			return cmd;
		}
	}
	return NULL;
}


static void show_options(struct ducrc_option *o)
{
	while(o && o->longopt) {
		char s[4] = "";
		char l[16] = "";

		if(o->shortopt) snprintf(s, sizeof(s), "-%c,", o->shortopt); 

		if(o->type != DUCRC_TYPE_BOOL) {
			snprintf(l, sizeof(l), "%s=VAL", o->longopt);
		} else {
			snprintf(l, sizeof(l), "%s", o->longopt);
		}

		printf(" %-4.4s --%-16.16s", s, l);
		if(o->description) printf("%s", o->description); 
		if(o->defval) printf(" [%s]", o->defval);
		printf("\n");

		o++;
	}
}


static void help_cmd(struct cmd *cmd)
{
	if(cmd->usage) {
		fprintf(stderr, "usage: duc %s %s\n", cmd->name, cmd->usage);
		fprintf(stderr, "\n");
	}
	
	printf("Options for the command '%s':\n", cmd->name);
	show_options(cmd->options);

	printf("\n");
	printf("Global options:\n");
	show_options(option_list);

#if NEE
	if(cmd->help) {
		fprintf(stderr, "%s", cmd->help);
	} else {
		fprintf(stderr, "No help for command\n");
	}
#endif
}


static int help_main(int argc, char **argv)
{
	struct cmd *cmd = NULL;

	if(argc > 1) cmd = find_cmd_by_name(argv[1]);

	if(cmd) {
		help_cmd(cmd);
	} else {
		fprintf(stderr, 
				"usage: duc <cmd> [options] [args]\n"
				"\n"
				"Available cmds:\n"
		       );

		int i;
		for(i=0; i<SUBCOMMAND_COUNT; i++) {
			struct cmd *c = cmd_list[i];
			fprintf(stderr, "  %-10.10s: %s\n", c->name, c->description);
		}
	}

	return 0;
}


struct cmd cmd_help = {
	.name = "help",
	.description = "Show help",
	.help = "",
	.main = help_main,
	.options = option_list,
};

/*
 * End
 */
