
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "ps.h"

int dump(struct db *db, const char *path);
off_t ps_index(struct db *db, const char *path);


struct cmd cmd_help;
struct cmd cmd_index;
struct cmd cmd_dump;


struct cmd *cmd_list[] = {
	&cmd_dump,
	&cmd_index,
	&cmd_help,
};

#define SUBCOMMAND_COUNT (sizeof(cmd_list) / sizeof(cmd_list[0]))



int main(int argc, char **argv)
{
	char *cmdname = "help";

	if(argc >= 2) cmdname = argv[1];

	struct cmd *cmd = &cmd_help;
	int i;

	for(i=0; i<SUBCOMMAND_COUNT; i++) {
		struct cmd *s = cmd_list[i];
		if(strcmp(s->name, cmdname) == 0) {
			cmd = s;
			break;
		}
	}

	return cmd->main(argc-1, argv+1);


}


static int help_main(int argc, char **argv)
{
	fprintf(stderr, 
		"usage: philesight <cmd> [options] [args]\n"
		"\n"
		"Available cmds:\n"
	);

	int i;
	for(i=0; i<SUBCOMMAND_COUNT; i++) {
		struct cmd *c = cmd_list[i];
		fprintf(stderr, "  %s: %s\n", c->name, c->description);
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
