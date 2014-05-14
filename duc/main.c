
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "cmd.h"


struct cmd cmd_help;
struct cmd cmd_info;
struct cmd cmd_index;
struct cmd cmd_ls;
struct cmd cmd_draw;
struct cmd cmd_cgi;


struct cmd *cmd_list[] = {
	&cmd_help,
	&cmd_info,
	&cmd_index,
	&cmd_ls,
	&cmd_draw,
	&cmd_cgi,
};

#define SUBCOMMAND_COUNT (sizeof(cmd_list) / sizeof(cmd_list[0]))

static struct cmd *find_cmd_by_name(const char *name);
static void help_cmd(struct cmd *cmd);


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



static void help_cmd(struct cmd *cmd)
{
	if(cmd->usage) {
		fprintf(stderr, "usage: duc %s %s\n", cmd->name, cmd->usage);
		fprintf(stderr, "\n");
	}

	if(cmd->help) {
		fprintf(stderr, "%s", cmd->help);
	} else {
		fprintf(stderr, "No help for command\n");
	}
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
	.main = help_main
};

/*
 * End
 */
